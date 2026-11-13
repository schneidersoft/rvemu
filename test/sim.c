#ifndef SIM_H
#define SIM_H

#include <stdio.h>

#include "linux.h"

void __assert_func(const char *filename, int line, const char *assert_func, const char *expr) {
    printf("%s %d %s FAILED: %s", filename, line, assert_func, expr);
    linux_exit_group(1);
}

void _putchar(char character) { linux_write(2, &character, 1); }

#endif
