#ifndef _FCNTL_H
#define _FCNTL_H

#include <guest.h>

int open(const char *path, int flags, ...);

#endif
