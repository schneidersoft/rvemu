#ifndef _ERRNO_H
#define _ERRNO_H

/*
 * The Linux asm-generic error numbers live in <linux.h> (they are shared with
 * the raw syscall helpers); this header only adds the C library variable.
 */
#include <linux.h>

extern int errno;

#endif
