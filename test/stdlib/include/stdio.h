#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct __stdio_file FILE;

typedef ssize_t cookie_read_function_t(void *cookie, char *buf, size_t size);
typedef ssize_t cookie_write_function_t(void *cookie, const char *buf, size_t size);
typedef int cookie_seek_function_t(void *cookie, off_t *offset, int whence);
typedef int cookie_close_function_t(void *cookie);

typedef struct {
    cookie_read_function_t *read;
    cookie_write_function_t *write;
    cookie_seek_function_t *seek;
    cookie_close_function_t *close;
} cookie_io_functions_t;

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
int fclose(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);

FILE *fmemopen(void *buf, size_t size, const char *mode);
FILE *open_memstream(char **ptr, size_t *sizeloc);
FILE *fopencookie(void *cookie, const char *mode, cookie_io_functions_t io_funcs);

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

/* Platform hooks. Return the number of bytes transferred, or -1 on error. */
long _read(int fd, void *buffer, size_t length);
long _write(int fd, const void *buffer, size_t length);

/* printf-style output through a caller supplied character function. */
int fctprintf(void (*out)(char character, void *arg), void *arg, const char *format, ...);

#endif
