#ifndef _NETDB_H
#define _NETDB_H

#include <stddef.h>
#include <sys/socket.h>

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};

enum {
    AI_PASSIVE = 1,
    AI_CANONNAME = 2,
    AI_NUMERICHOST = 4,
};

/* getaddrinfo/getnameinfo error codes, glibc semantics. */
enum {
    EAI_BADFLAGS = -1,
    EAI_NONAME = -2,
    EAI_FAMILY = -3,
    EAI_SOCKTYPE = -4,
    EAI_SERVICE = -5,
    EAI_NODATA = -6,
    EAI_MEMORY = -7,
    EAI_SYSTEM = -8,
};

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **result);
void freeaddrinfo(struct addrinfo *result);
const char *gai_strerror(int code);

#endif
