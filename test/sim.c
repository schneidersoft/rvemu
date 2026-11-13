#ifndef SIM_H
#define SIM_H

#include <stdio.h>

#include "linux.h"

void __assert_func(const char *filename, int line, const char *assert_func, const char *expr) {
    fprintf(stderr, "%s %d %s FAILED: %s\n", filename, line, assert_func, expr);
    fflush(NULL);
    linux_exit_group(1);
}

#endif
