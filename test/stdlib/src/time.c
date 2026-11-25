#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "linux.h"

time_t time(time_t *tloc) {
    struct linux_timespec ts;
    long r = linux_clock_gettime(CLOCK_REALTIME, &ts);
    if (r < 0) {
        struct linux_timeval tv;
        r = linux_gettimeofday(&tv);
        if (r < 0) { errno = -(int)r; return (time_t)-1; }
        if (tloc) *tloc = (time_t)tv.tv_sec;
        return (time_t)tv.tv_sec;
    }
    if (tloc) *tloc = (time_t)ts.tv_sec;
    return (time_t)ts.tv_sec;
}

clock_t clock(void) {
    struct linux_timespec ts;
    if (linux_clock_gettime(CLOCK_MONOTONIC, &ts) < 0) return (clock_t)-1;
    return (clock_t)(ts.tv_sec * CLOCKS_PER_SEC + ts.tv_nsec / 1000);
}

double difftime(time_t time1, time_t time0) {
    return (double)(time1 - time0);
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    struct linux_timespec r, m, *mp = rem ? &m : NULL;
    if (req == NULL) { errno = EFAULT; return -1; }
    r.tv_sec = req->tv_sec;
    r.tv_nsec = req->tv_nsec;
    long rc = linux_nanosleep(&r, mp);
    if (rc < 0) { errno = -(int)rc; if (rem) { rem->tv_sec = m.tv_sec; rem->tv_nsec = m.tv_nsec; } return -1; }
    if (rem) { rem->tv_sec = m.tv_sec; rem->tv_nsec = m.tv_nsec; }
    return 0;
}

int clock_gettime(int clockid, struct timespec *tp) {
    struct linux_timespec ts;
    if (tp == NULL) { errno = EFAULT; return -1; }
    long r = linux_clock_gettime(clockid, &ts);
    if (r < 0) { errno = -(int)r; return -1; }
    tp->tv_sec = (time_t)ts.tv_sec;
    tp->tv_nsec = (long)ts.tv_nsec;
    return 0;
}

int clock_getres(int clockid, struct timespec *res) {
    struct linux_timespec ts;
    if (res == NULL) return 0;
    long r = linux_clock_getres(clockid, &ts);
    if (r < 0) { errno = -(int)r; return -1; }
    res->tv_sec = (time_t)ts.tv_sec;
    res->tv_nsec = (long)ts.tv_nsec;
    return 0;
}

static int is_leap(long y) {
    if (y % 4 != 0) return 0;
    if (y % 100 != 0) return 1;
    return y % 400 == 0;
}

static const short ydays[2][13] = {
    {0,31,59,90,120,151,181,212,243,273,304,334,365},
    {0,31,60,91,121,152,182,213,244,274,305,335,366}
};

static struct tm g_tm;

struct tm *gmtime_r(const time_t *timep, struct tm *result) {
    if (timep == NULL || result == NULL) return NULL;
    long long t = (long long)*timep;
    long days = (long)(t / 86400);
    long rem = (long)(t % 86400);
    if (rem < 0) { rem += 86400; days--; }
    result->tm_sec = (int)(rem % 60); rem /= 60;
    result->tm_min = (int)(rem % 60); rem /= 60;
    result->tm_hour = (int)rem;
    /* 1970-01-01 was Thursday */
    result->tm_wday = (int)((days + 4) % 7);
    if (result->tm_wday < 0) result->tm_wday += 7;

    long year = 1970;
    while (days >= (is_leap(year) ? 366 : 365)) { days -= is_leap(year) ? 366 : 365; year++; }
    while (days < 0) { year--; days += is_leap(year) ? 366 : 365; }
    result->tm_year = (int)(year - 1900);
    result->tm_yday = (int)days;
    int leap = is_leap(year);
    int mon = 0;
    while (mon < 11 && days >= ydays[leap][mon+1]) mon++;
    result->tm_mon = mon;
    result->tm_mday = (int)(days - ydays[leap][mon] + 1);
    result->tm_isdst = 0;
    return result;
}

struct tm *gmtime(const time_t *timep) {
    return gmtime_r(timep, &g_tm);
}

struct tm *localtime_r(const time_t *timep, struct tm *result) {
    /* No timezone database in freestanding guest; treat local as UTC. */
    return gmtime_r(timep, result);
}

struct tm *localtime(const time_t *timep) {
    return localtime_r(timep, &g_tm);
}

time_t mktime(struct tm *tm) {
    if (tm == NULL) return (time_t)-1;
    long year = tm->tm_year + 1900;
    int mon = tm->tm_mon;
    /* normalize month */
    while (mon < 0) { mon += 12; year--; }
    while (mon > 11) { mon -= 12; year++; }
    long days = 0;
    for (long y = 1970; y < year; y++) days += is_leap(y) ? 366 : 365;
    for (long y = year; y < 1970; y++) days -= is_leap(y) ? 366 : 365;
    days += ydays[is_leap(year)][mon] + tm->tm_mday - 1;
    long long t = (long long)days * 86400LL + (long long)tm->tm_hour * 3600LL + (long long)tm->tm_min * 60LL + tm->tm_sec;
    /* normalize tm fields */
    gmtime_r((time_t *)&t, tm);
    return (time_t)t;
}

static size_t fmt_num(char *s, size_t max, int val, int width, char pad) {
    char tmp[16];
    int n = 0;
    unsigned int v = val < 0 ? (unsigned int)-val : (unsigned int)val;
    if (val < 0) tmp[n++] = '-';
    char digits[16]; int dn = 0;
    if (v == 0) digits[dn++] = '0';
    else while (v) { digits[dn++] = '0' + (v % 10); v /= 10; }
    int padn = width - dn - (val < 0 ? 1 : 0);
    for (int i = 0; i < padn && n < 15; i++) tmp[n++] = pad;
    for (int i = dn - 1; i >= 0 && n < 15; i--) tmp[n++] = digits[i];
    size_t w = 0;
    for (int i = 0; i < n && w + 1 < max; i++) s[w++] = tmp[i];
    if (max) s[w < max ? w : max-1] = '\0';
    return (size_t)n;
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    static const char *wday[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char *mon[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    if (s == NULL || format == NULL || tm == NULL || max == 0) return 0;
    size_t o = 0;
    for (const char *f = format; *f && o + 1 < max; f++) {
        if (*f != '%') { s[o++] = *f; continue; }
        f++;
        char buf[32]; buf[0] = '\0'; size_t n = 0;
        switch (*f) {
        case '%': buf[0]='%'; buf[1]='\0'; n=1; break;
        case 'n': buf[0]='\n'; buf[1]='\0'; n=1; break;
        case 't': buf[0]='\t'; buf[1]='\0'; n=1; break;
        case 'Y': n = fmt_num(buf, sizeof(buf), tm->tm_year + 1900, 4, '0'); break;
        case 'm': n = fmt_num(buf, sizeof(buf), tm->tm_mon + 1, 2, '0'); break;
        case 'd': n = fmt_num(buf, sizeof(buf), tm->tm_mday, 2, '0'); break;
        case 'H': n = fmt_num(buf, sizeof(buf), tm->tm_hour, 2, '0'); break;
        case 'M': n = fmt_num(buf, sizeof(buf), tm->tm_min, 2, '0'); break;
        case 'S': n = fmt_num(buf, sizeof(buf), tm->tm_sec, 2, '0'); break;
        case 'e': n = fmt_num(buf, sizeof(buf), tm->tm_mday, 2, ' '); break;
        case 'j': n = fmt_num(buf, sizeof(buf), tm->tm_yday + 1, 3, '0'); break;
        case 'a': if (tm->tm_wday>=0&&tm->tm_wday<7){strcpy(buf,wday[tm->tm_wday]);n=3;} break;
        case 'b': case 'h': if (tm->tm_mon>=0&&tm->tm_mon<12){strcpy(buf,mon[tm->tm_mon]);n=3;} break;
        case 'F': { char t[32]; snprintf(t,sizeof(t),"%04d-%02d-%02d",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday); strcpy(buf,t); n=strlen(buf);} break;
        case 'T': { char t[32]; snprintf(t,sizeof(t),"%02d:%02d:%02d",tm->tm_hour,tm->tm_min,tm->tm_sec); strcpy(buf,t); n=strlen(buf);} break;
        case 'R': { char t[32]; snprintf(t,sizeof(t),"%02d:%02d",tm->tm_hour,tm->tm_min); strcpy(buf,t); n=strlen(buf);} break;
        case '\0': f--; break;
        default: buf[0]='%'; buf[1]=*f; buf[2]='\0'; n=2; break;
        }
        for (size_t i = 0; i < n && o + 1 < max; i++) s[o++] = buf[i];
    }
    s[o] = '\0';
    return o;
}
