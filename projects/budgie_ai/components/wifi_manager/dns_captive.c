// SPDX-License-Identifier: Apache-2.0

#include "dns_captive.h"

#include <string.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

static const char *TAG = "dns_captive";

#define DNS_PORT          53
#define DNS_PACKET_MAX    512
#define DNS_HEADER_SIZE   12
#define DNS_ANSWER_SIZE   16
#define DNS_TASK_STACK    3072
#define DNS_TASK_PRIO     4
#define DNS_RECV_TIMEOUT_MS 500

#define DNS_FLAG_RESPONSE 0x8000
#define DNS_FLAG_AA       0x0400
#define DNS_FLAG_RA       0x0080
#define DNS_TYPE_A        1
#define DNS_CLASS_IN      1
#define DNS_TTL_SECONDS   60

/* Owned by dns_captive_start/stop, which run from the Wi-Fi manager only */
static struct {
    TaskHandle_t task;
    volatile bool running;
    uint32_t ip;
    uint8_t *packet;
} s_dns;

/* Skip the QNAME of the first question; returns its end offset or 0 if malformed */
static size_t skip_name(const uint8_t *p, size_t len, size_t off)
{
    while (off < len) {
        uint8_t l = p[off];
        if (l == 0) {
            return off + 1;
        }
        if ((l & 0xC0) != 0) {
            return 0;   /* compression is not expected in a query name */
        }
        off += (size_t)l + 1;
    }
    return 0;
}

static int build_reply(uint8_t *p, int len)
{
    if (len < DNS_HEADER_SIZE) {
        return -1;
    }
    const uint16_t qdcount = (uint16_t)((p[4] << 8) | p[5]);
    if (qdcount < 1) {
        return -1;
    }
    size_t qend = skip_name(p, (size_t)len, DNS_HEADER_SIZE);
    if (qend == 0 || qend + 4 > (size_t)len) {
        return -1;
    }
    const uint16_t qtype = (uint16_t)((p[qend] << 8) | p[qend + 1]);
    qend += 4;  /* QTYPE + QCLASS */

    uint16_t flags = DNS_FLAG_RESPONSE | DNS_FLAG_AA | DNS_FLAG_RA;
    flags |= (uint16_t)(((p[2] << 8) | p[3]) & 0x7900);   /* keep opcode + RD */
    p[2] = (uint8_t)(flags >> 8);
    p[3] = (uint8_t)flags;
    p[4] = 0; p[5] = 1;                                    /* one question */
    p[8] = 0; p[9] = 0;                                    /* no authority */
    p[10] = 0; p[11] = 0;                                  /* no additional */

    if (qtype != DNS_TYPE_A || qend + DNS_ANSWER_SIZE > DNS_PACKET_MAX) {
        p[6] = 0; p[7] = 0;                                /* no answer (e.g. AAAA) */
        return (int)qend;
    }
    p[6] = 0; p[7] = 1;
    uint8_t *a = p + qend;
    a[0] = 0xC0; a[1] = DNS_HEADER_SIZE;                   /* pointer to the question name */
    a[2] = 0; a[3] = DNS_TYPE_A;
    a[4] = 0; a[5] = DNS_CLASS_IN;
    a[6] = 0; a[7] = 0; a[8] = 0; a[9] = DNS_TTL_SECONDS;
    a[10] = 0; a[11] = 4;
    memcpy(&a[12], &s_dns.ip, 4);
    return (int)(qend + DNS_ANSWER_SIZE);
}

static void dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket failed");
        goto done;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "bind failed");
        goto done;
    }
    struct timeval tv = { .tv_sec = 0, .tv_usec = DNS_RECV_TIMEOUT_MS * 1000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ESP_LOGI(TAG, "captive DNS running");
    while (s_dns.running) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        int n = recvfrom(sock, s_dns.packet, DNS_PACKET_MAX, 0, (struct sockaddr *)&from, &from_len);
        if (n <= 0) {
            continue;
        }
        int reply = build_reply(s_dns.packet, n);
        if (reply > 0) {
            sendto(sock, s_dns.packet, (size_t)reply, 0, (struct sockaddr *)&from, from_len);
        }
    }
done:
    if (sock >= 0) {
        close(sock);
    }
    s_dns.task = NULL;
    vTaskDelete(NULL);
}

esp_err_t dns_captive_start(uint32_t ipv4)
{
    if (s_dns.task != NULL) {
        s_dns.ip = ipv4;
        return ESP_OK;
    }
    if (s_dns.packet == NULL) {
        s_dns.packet = heap_caps_malloc(DNS_PACKET_MAX, MALLOC_CAP_8BIT);
        ESP_RETURN_ON_FALSE(s_dns.packet != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    }
    s_dns.ip = ipv4;
    s_dns.running = true;
    if (xTaskCreate(dns_task, "dns_captive", DNS_TASK_STACK, NULL, DNS_TASK_PRIO, &s_dns.task) != pdPASS) {
        s_dns.running = false;
        s_dns.task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void dns_captive_stop(void)
{
    if (s_dns.task == NULL) {
        return;
    }
    s_dns.running = false;
    /* The task notices within one receive timeout and clears s_dns.task */
    for (int i = 0; i < 20 && s_dns.task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(DNS_RECV_TIMEOUT_MS / 4));
    }
}
