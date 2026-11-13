#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __stdio_file FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Write one character to the platform's console. */
void _putchar(char character);

int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *buffer, const char *format, ...);
int snprintf(char *buffer, size_t count, const char *format, ...);

int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsnprintf(char *buffer, size_t count, const char *format, va_list ap);

/* printf-style output through a caller supplied character function. */
int fctprintf(void (*out)(char character, void *arg), void *arg, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
