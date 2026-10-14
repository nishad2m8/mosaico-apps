// SPDX-License-Identifier: Apache-2.0
//
// Near-Earth branch of SGP4 following D. Vallado's public-domain reference
// implementation ("Revisiting Spacetrack Report #3", 2006), WGS-72 constants,
// improved (non-AFSPC) operation mode.

#include "sgp4.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TWO_PI           (2.0 * M_PI)
#define DEG2RAD          (M_PI / 180.0)
#define MIN_PER_DAY      1440.0
#define MS_PER_DAY       86400000.0
#define JD_UNIX_EPOCH    2440587.5
#define JD_J2000         2451545.0

/* WGS-72 */
#define EARTH_RADIUS_KM  6378.135
#define EARTH_MU         398600.8
#define J2               0.001082616
#define J3              -0.00000253881
#define J4              -0.00000165597
#define J3OJ2            (J3 / J2)
#define X2O3             (2.0 / 3.0)

#define DEEP_SPACE_PERIOD_MIN 225.0
#define KEPLER_TOLERANCE      1.0e-12
#define KEPLER_MAX_ITER       10
#define ECC_MIN               1.0e-6
#define ECC_SMALL             1.0e-4

static double xke(void)
{
    return 60.0 / sqrt(EARTH_RADIUS_KM * EARTH_RADIUS_KM * EARTH_RADIUS_KM / EARTH_MU);
}

/* Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant). */
static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static bool tle_checksum_ok(const char *line)
{
    if (strlen(line) < SGP4_TLE_LINE_LEN - 1 || !isdigit((unsigned char)line[68])) {
        return false;
    }
    int sum = 0;
    for (int i = 0; i < 68; ++i) {
        if (isdigit((unsigned char)line[i])) {
            sum += line[i] - '0';
        } else if (line[i] == '-') {
            sum += 1;
        }
    }
    return sum % 10 == line[68] - '0';
}

/* Parse columns [start, start+len) as a double; implied_point prefixes "0.". */
static bool field(const char *line, int start, int len, bool implied_point, double *out)
{
    char buf[24];
    int n = 0;
    if (implied_point) {
        buf[n++] = '0';
        buf[n++] = '.';
    }
    for (int i = 0; i < len && n < (int)sizeof(buf) - 1; ++i) {
        const char c = line[start + i];
        if (c != ' ') {
            buf[n++] = c;
        }
    }
    buf[n] = '\0';
    if (n == 0 || (implied_point && n == 2)) {
        return false;
    }
    char *end = NULL;
    *out = strtod(buf, &end);
    return end != NULL && *end == '\0';
}

/* TLE exponent notation, e.g. " 11345-3" = 0.11345e-3. */
static bool exp_field(const char *line, int start, double *out)
{
    double mantissa, exponent;
    if (!field(line, start + 1, 5, true, &mantissa) || !field(line, start + 6, 2, false, &exponent)) {
        return false;
    }
    *out = (line[start] == '-' ? -1.0 : 1.0) * mantissa * pow(10.0, exponent);
    return true;
}

static sgp4_err_t parse(sgp4_record_t *rec, const char *l1, const char *l2)
{
    if (l1[0] != '1' || l2[0] != '2' || !tle_checksum_ok(l1) || !tle_checksum_ok(l2)) {
        return SGP4_ERR_PARSE;
    }
    double year2, epoch_days, incl, node, ecc, argp, ma, mean_motion;
    if (!field(l1, 18, 2, false, &year2) || !field(l1, 20, 12, false, &epoch_days) ||
            !exp_field(l1, 53, &rec->bstar) ||
            !field(l2, 8, 8, false, &incl) || !field(l2, 17, 8, false, &node) ||
            !field(l2, 26, 7, true, &ecc) || !field(l2, 34, 8, false, &argp) ||
            !field(l2, 43, 8, false, &ma) || !field(l2, 52, 11, false, &mean_motion) ||
            mean_motion <= 0.0) {
        return SGP4_ERR_PARSE;
    }
    const int year = (int)year2 < 57 ? 2000 + (int)year2 : 1900 + (int)year2;
    rec->epoch_unix_ms = days_from_civil(year, 1, 1) * (int64_t)MS_PER_DAY +
                         (int64_t)llround((epoch_days - 1.0) * MS_PER_DAY);
    rec->inclo = incl * DEG2RAD;
    rec->nodeo = node * DEG2RAD;
    rec->ecco = ecc;
    rec->argpo = argp * DEG2RAD;
    rec->mo = ma * DEG2RAD;
    rec->no = mean_motion * TWO_PI / MIN_PER_DAY;   /* Kozai mean motion, rad/min */
    return SGP4_OK;
}

sgp4_err_t sgp4_init(sgp4_record_t *rec, const char *line1, const char *line2)
{
    if (!rec || !line1 || !line2) {
        return SGP4_ERR_PARSE;
    }
    memset(rec, 0, sizeof(*rec));
    sgp4_err_t err = parse(rec, line1, line2);
    if (err != SGP4_OK) {
        return err;
    }
    if (TWO_PI / rec->no >= DEEP_SPACE_PERIOD_MIN) {
        return SGP4_ERR_DEEP_SPACE;
    }

    const double k = xke();
    const double ecco = rec->ecco, inclo = rec->inclo;

    /* initl: recover the Brouwer mean motion */
    const double eccsq = ecco * ecco;
    const double omeosq = 1.0 - eccsq;
    const double rteosq = sqrt(omeosq);
    const double cosio = cos(inclo);
    const double cosio2 = cosio * cosio;
    const double ak = pow(k / rec->no, X2O3);
    const double d1 = 0.75 * J2 * (3.0 * cosio2 - 1.0) / (rteosq * omeosq);
    double del = d1 / (ak * ak);
    const double adel = ak * (1.0 - del * del - del * (1.0 / 3.0 + 134.0 * del * del / 81.0));
    del = d1 / (adel * adel);
    rec->no = rec->no / (1.0 + del);
    const double no = rec->no;

    const double ao = pow(k / no, X2O3);
    const double sinio = sin(inclo);
    const double po = ao * omeosq;
    const double con42 = 1.0 - 5.0 * cosio2;
    rec->con41 = -con42 - cosio2 - cosio2;
    const double posq = po * po;
    const double rp = ao * (1.0 - ecco);

    /* sgp4init, near-Earth branch */
    const double ss = 78.0 / EARTH_RADIUS_KM + 1.0;
    const double qzms2t = pow((120.0 - 78.0) / EARTH_RADIUS_KM, 4);
    rec->isimp = rp < (220.0 / EARTH_RADIUS_KM + 1.0);

    double sfour = ss, qzms24 = qzms2t;
    const double perige = (rp - 1.0) * EARTH_RADIUS_KM;
    if (perige < 156.0) {
        sfour = perige < 98.0 ? 20.0 : perige - 78.0;
        qzms24 = pow((120.0 - sfour) / EARTH_RADIUS_KM, 4);
        sfour = sfour / EARTH_RADIUS_KM + 1.0;
    }
    const double pinvsq = 1.0 / posq;
    const double tsi = 1.0 / (ao - sfour);
    rec->eta = ao * ecco * tsi;
    const double etasq = rec->eta * rec->eta;
    const double eeta = ecco * rec->eta;
    const double psisq = fabs(1.0 - etasq);
    const double coef = qzms24 * pow(tsi, 4.0);
    const double coef1 = coef / pow(psisq, 3.5);
    const double cc2 = coef1 * no * (ao * (1.0 + 1.5 * etasq + eeta * (4.0 + etasq)) +
                       0.375 * J2 * tsi / psisq * rec->con41 * (8.0 + 3.0 * etasq * (8.0 + etasq)));
    rec->cc1 = rec->bstar * cc2;
    double cc3 = 0.0;
    if (ecco > ECC_SMALL) {
        cc3 = -2.0 * coef * tsi * J3OJ2 * no * sinio / ecco;
    }
    rec->x1mth2 = 1.0 - cosio2;
    rec->cc4 = 2.0 * no * coef1 * ao * omeosq *
               (rec->eta * (2.0 + 0.5 * etasq) + ecco * (0.5 + 2.0 * etasq) -
                J2 * tsi / (ao * psisq) *
                (-3.0 * rec->con41 * (1.0 - 2.0 * eeta + etasq * (1.5 - 0.5 * eeta)) +
                 0.75 * rec->x1mth2 * (2.0 * etasq - eeta * (1.0 + etasq)) * cos(2.0 * rec->argpo)));
    rec->cc5 = 2.0 * coef1 * ao * omeosq * (1.0 + 2.75 * (etasq + eeta) + eeta * etasq);

    const double cosio4 = cosio2 * cosio2;
    const double temp1 = 1.5 * J2 * pinvsq * no;
    const double temp2 = 0.5 * temp1 * J2 * pinvsq;
    const double temp3 = -0.46875 * J4 * pinvsq * pinvsq * no;
    rec->mdot = no + 0.5 * temp1 * rteosq * rec->con41 +
                0.0625 * temp2 * rteosq * (13.0 - 78.0 * cosio2 + 137.0 * cosio4);
    rec->argpdot = -0.5 * temp1 * con42 + 0.0625 * temp2 * (7.0 - 114.0 * cosio2 + 395.0 * cosio4) +
                   temp3 * (3.0 - 36.0 * cosio2 + 49.0 * cosio4);
    const double xhdot1 = -temp1 * cosio;
    rec->nodedot = xhdot1 + (0.5 * temp2 * (4.0 - 19.0 * cosio2) + 2.0 * temp3 * (3.0 - 7.0 * cosio2)) * cosio;
    rec->omgcof = rec->bstar * cc3 * cos(rec->argpo);
    rec->xmcof = ecco > ECC_SMALL ? -X2O3 * coef * rec->bstar / eeta : 0.0;
    rec->nodecf = 3.5 * omeosq * xhdot1 * rec->cc1;
    rec->t2cof = 1.5 * rec->cc1;
    const double cosio_p1 = fabs(cosio + 1.0) > 1.5e-12 ? 1.0 + cosio : 1.5e-12;
    rec->xlcof = -0.25 * J3OJ2 * sinio * (3.0 + 5.0 * cosio) / cosio_p1;
    rec->aycof = -0.5 * J3OJ2 * sinio;
    rec->delmo = pow(1.0 + rec->eta * cos(rec->mo), 3);
    rec->sinmao = sin(rec->mo);
    rec->x7thm1 = 7.0 * cosio2 - 1.0;

    if (!rec->isimp) {
        const double cc1sq = rec->cc1 * rec->cc1;
        rec->d2 = 4.0 * ao * tsi * cc1sq;
        const double temp = rec->d2 * tsi * rec->cc1 / 3.0;
        rec->d3 = (17.0 * ao + sfour) * temp;
        rec->d4 = 0.5 * temp * ao * tsi * (221.0 * ao + 31.0 * sfour) * rec->cc1;
        rec->t3cof = rec->d2 + 2.0 * cc1sq;
        rec->t4cof = 0.25 * (3.0 * rec->d3 + rec->cc1 * (12.0 * rec->d2 + 10.0 * cc1sq));
        rec->t5cof = 0.2 * (3.0 * rec->d4 + 12.0 * rec->cc1 * rec->d3 + 6.0 * rec->d2 * rec->d2 +
                            15.0 * cc1sq * (2.0 * rec->d2 + cc1sq));
    }

    sgp4_state_t check;
    return sgp4_propagate(rec, rec->epoch_unix_ms, &check);
}

sgp4_err_t sgp4_propagate(const sgp4_record_t *rec, int64_t unix_ms, sgp4_state_t *out)
{
    const double k = xke();
    const double t = (double)(unix_ms - rec->epoch_unix_ms) / 60000.0;   /* minutes */

    const double xmdf = rec->mo + rec->mdot * t;
    const double argpdf = rec->argpo + rec->argpdot * t;
    const double nodedf = rec->nodeo + rec->nodedot * t;
    double argpm = argpdf;
    double mm = xmdf;
    const double t2 = t * t;
    double nodem = nodedf + rec->nodecf * t2;
    double tempa = 1.0 - rec->cc1 * t;
    double tempe = rec->bstar * rec->cc4 * t;
    double templ = rec->t2cof * t2;

    if (!rec->isimp) {
        const double delomg = rec->omgcof * t;
        const double delmtemp = 1.0 + rec->eta * cos(xmdf);
        const double delm = rec->xmcof * (delmtemp * delmtemp * delmtemp - rec->delmo);
        const double temp = delomg + delm;
        mm = xmdf + temp;
        argpm = argpdf - temp;
        const double t3 = t2 * t;
        const double t4 = t3 * t;
        tempa = tempa - rec->d2 * t2 - rec->d3 * t3 - rec->d4 * t4;
        tempe = tempe + rec->bstar * rec->cc5 * (sin(mm) - rec->sinmao);
        templ = templ + rec->t3cof * t3 + t4 * (rec->t4cof + t * rec->t5cof);
    }

    const double am = pow(k / rec->no, X2O3) * tempa * tempa;
    const double nm = k / pow(am, 1.5);
    double em = rec->ecco - tempe;
    if (em >= 1.0 || em < -0.001) {
        return SGP4_ERR_ECCENTRICITY;
    }
    if (em < ECC_MIN) {
        em = ECC_MIN;
    }
    mm = mm + rec->no * templ;
    double xlm = mm + argpm + nodem;
    nodem = fmod(nodem, TWO_PI);
    argpm = fmod(argpm, TWO_PI);
    xlm = fmod(xlm, TWO_PI);
    mm = fmod(xlm - argpm - nodem, TWO_PI);

    const double sinip = sin(rec->inclo);
    const double cosip = cos(rec->inclo);

    /* long-period periodics */
    const double axnl = em * cos(argpm);
    double temp = 1.0 / (am * (1.0 - em * em));
    const double aynl = em * sin(argpm) + temp * rec->aycof;
    const double xl = mm + argpm + nodem + temp * rec->xlcof * axnl;

    /* Kepler's equation */
    const double u = fmod(xl - nodem, TWO_PI);
    double eo1 = u, tem5 = 9999.9, sineo1 = 0.0, coseo1 = 0.0;
    for (int ktr = 1; fabs(tem5) >= KEPLER_TOLERANCE && ktr <= KEPLER_MAX_ITER; ++ktr) {
        sineo1 = sin(eo1);
        coseo1 = cos(eo1);
        tem5 = 1.0 - coseo1 * axnl - sineo1 * aynl;
        tem5 = (u - aynl * coseo1 + axnl * sineo1 - eo1) / tem5;
        if (fabs(tem5) >= 0.95) {
            tem5 = tem5 > 0.0 ? 0.95 : -0.95;
        }
        eo1 += tem5;
    }

    /* short-period periodics */
    const double ecose = axnl * coseo1 + aynl * sineo1;
    const double esine = axnl * sineo1 - aynl * coseo1;
    const double el2 = axnl * axnl + aynl * aynl;
    const double pl = am * (1.0 - el2);
    if (pl < 0.0) {
        return SGP4_ERR_ECCENTRICITY;
    }
    const double rl = am * (1.0 - ecose);
    const double rdotl = sqrt(am) * esine / rl;
    const double rvdotl = sqrt(pl) / rl;
    const double betal = sqrt(1.0 - el2);
    temp = esine / (1.0 + betal);
    const double sinu = am / rl * (sineo1 - aynl - axnl * temp);
    const double cosu = am / rl * (coseo1 - axnl + aynl * temp);
    double su = atan2(sinu, cosu);
    const double sin2u = (cosu + cosu) * sinu;
    const double cos2u = 1.0 - 2.0 * sinu * sinu;
    temp = 1.0 / pl;
    const double temp1 = 0.5 * J2 * temp;
    const double temp2 = temp1 * temp;

    const double mrt = rl * (1.0 - 1.5 * temp2 * betal * rec->con41) + 0.5 * temp1 * rec->x1mth2 * cos2u;
    su = su - 0.25 * temp2 * rec->x7thm1 * sin2u;
    const double xnode = nodem + 1.5 * temp2 * cosip * sin2u;
    const double xinc = rec->inclo + 1.5 * temp2 * cosip * sinip * cos2u;
    const double mvt = rdotl - nm * temp1 * rec->x1mth2 * sin2u / k;
    const double rvdot = rvdotl + nm * temp1 * (rec->x1mth2 * cos2u + 1.5 * rec->con41) / k;

    const double sinsu = sin(su), cossu = cos(su);
    const double snod = sin(xnode), cnod = cos(xnode);
    const double sini = sin(xinc), cosi = cos(xinc);
    const double xmx = -snod * cosi;
    const double xmy = cnod * cosi;
    const double ux = xmx * sinsu + cnod * cossu;
    const double uy = xmy * sinsu + snod * cossu;
    const double uz = sini * sinsu;
    const double vx = xmx * cossu - cnod * sinsu;
    const double vy = xmy * cossu - snod * sinsu;
    const double vz = sini * cossu;

    const double vkmpersec = EARTH_RADIUS_KM * k / 60.0;
    out->r[0] = mrt * ux * EARTH_RADIUS_KM;
    out->r[1] = mrt * uy * EARTH_RADIUS_KM;
    out->r[2] = mrt * uz * EARTH_RADIUS_KM;
    out->v[0] = (mvt * ux + rvdot * vx) * vkmpersec;
    out->v[1] = (mvt * uy + rvdot * vy) * vkmpersec;
    out->v[2] = (mvt * uz + rvdot * vz) * vkmpersec;
    return mrt < 1.0 ? SGP4_ERR_DECAYED : SGP4_OK;
}

double sgp4_gmst(int64_t unix_ms)
{
    const double jd = (double)unix_ms / MS_PER_DAY + JD_UNIX_EPOCH;
    const double tut1 = (jd - JD_J2000) / 36525.0;
    double temp = -6.2e-6 * tut1 * tut1 * tut1 + 0.093104 * tut1 * tut1 +
                  (876600.0 * 3600.0 + 8640184.812866) * tut1 + 67310.54841;
    temp = fmod(temp * DEG2RAD / 240.0, TWO_PI);
    return temp < 0.0 ? temp + TWO_PI : temp;
}
