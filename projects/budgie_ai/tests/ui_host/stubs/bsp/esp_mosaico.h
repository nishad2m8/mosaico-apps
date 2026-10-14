#pragma once
#include <stdbool.h>
bool bsp_display_lock(int timeout_ms);
void bsp_display_unlock(void);
