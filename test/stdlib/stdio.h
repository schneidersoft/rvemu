#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __stdio_file FILE;

#ifndef EOF
#define EOF (-1)
#endif

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Write one character to stdout. */
void _putchar(char character);

int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *buffer, const char *format, ...);
int snprintf(char *buffer, size_t count, const char *format, ...);

int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsnprintf(char *buffer, size_t count, const char *format, va_list ap);

int fputc(int character, FILE *stream);
int putc(int character, FILE *stream);
int putchar(int character);
int fputs(const char *string, FILE *stream);
int puts(const char *string);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
int fflush(FILE *stream);

int fgetc(FILE *stream);
int getc(FILE *stream);
int getchar(void);
int ungetc(int character, FILE *stream);
char *fgets(char *buffer, int size, FILE *stream);
size_t fread(void *ptr, size_t size, size_t count, FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
int fileno(FILE *stream);

/* Platform hooks. Return the number of bytes transferred, or -1 on error. */
long _read(int fd, void *buffer, size_t length);
long _write(int fd, const void *buffer, size_t length);

/* printf-style output through a caller supplied character function. */
int fctprintf(void (*out)(char character, void *arg), void *arg, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
