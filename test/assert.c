#include <stdio.h>
#include <stdlib.h>

void __assert_func(const char *filename, int line, const char *assert_func, const char *expr) {
    fprintf(stderr, "%s %d %s FAILED: %s\n", filename, line, assert_func, expr);
    fflush(NULL);
    exit(1);
}
