#ifndef _TIME_H
#define _TIME_H

#include <stddef.h>

typedef long time_t;
typedef long clock_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

#define CLOCKS_PER_SEC 1000000L
#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

time_t time(time_t *tloc);
clock_t clock(void);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm *tm);
struct tm *gmtime(const time_t *timep);
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime(const time_t *timep);
struct tm *localtime_r(const time_t *timep, struct tm *result);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_gettime(int clockid, struct timespec *tp);
int clock_getres(int clockid, struct timespec *res);

#endif
