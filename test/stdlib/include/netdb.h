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
    AI_NUMERICSERV = 1024,
};

enum {
    NI_NUMERICHOST = 1,
    NI_NUMERICSERV = 2,
    NI_NOFQDN = 4,
    NI_NAMEREQD = 8,
    NI_DGRAM = 16,
};

/* getaddrinfo/getnameinfo error codes, glibc semantics. */
enum {
    EAI_BADFLAGS = -1,
    EAI_NONAME = -2,
    EAI_AGAIN = -3,
    EAI_FAIL = -4,
    EAI_FAMILY = -6,
    EAI_SOCKTYPE = -7,
    EAI_SERVICE = -8,
    EAI_MEMORY = -10,
    EAI_SYSTEM = -11,
    EAI_OVERFLOW = -12,
    EAI_NODATA = -5,
};

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **result);
void freeaddrinfo(struct addrinfo *result);
const char *gai_strerror(int code);
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);

#endif
