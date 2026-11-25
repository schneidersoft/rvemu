#ifndef _ERRNO_H
#define _ERRNO_H

/*
 * The Linux asm-generic error numbers live in guest.h (shared with the
 * raw syscall helpers and the host emulator); this header only adds the
 * C library variable.
 */
#include <guest.h>

extern int errno;

#endif
