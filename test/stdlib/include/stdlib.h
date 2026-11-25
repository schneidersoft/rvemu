#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

void exit(int status);
int atoi(const char *string);

/* Diagnostics are declared here because the stream header is printf-only. */
void perror(const char *string);
const char *strerror(int number);

#endif
