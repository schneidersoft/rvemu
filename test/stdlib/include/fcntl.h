#ifndef _FCNTL_H
#define _FCNTL_H

#include <guest.h>

#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_GETOWN 5
#define F_SETOWN 6

int open(const char *path, int flags, ...);
int fcntl(int fd, int cmd, ...);

#endif
