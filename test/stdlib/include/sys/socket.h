#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <guest.h>

typedef unsigned int socklen_t;
typedef uint16_t sa_family_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX AF_UNIX
#define PF_INET AF_INET
#define PF_INET6 AF_INET6

#define INADDR_ANY 0x00000000U
#define INADDR_BROADCAST 0xffffffffU
#define INADDR_LOOPBACK 0x7f000001U
#define INADDR_NONE 0xffffffffU

struct in_addr {
    in_addr_t s_addr;
};

struct in6_addr {
    uint8_t s6_addr[16];
};

struct sockaddr {
    sa_family_t sa_family;
    unsigned char sa_data[14];
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};

struct sockaddr_in6 {
    sa_family_t sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *address, socklen_t length);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *address, socklen_t *length);
int connect(int sockfd, const struct sockaddr *address, socklen_t length);
ssize_t send(int sockfd, const void *buffer, size_t length, int flags);
ssize_t sendto(int sockfd, const void *buffer, size_t length, int flags, const struct sockaddr *dest, socklen_t dest_len);
ssize_t recv(int sockfd, void *buffer, size_t length, int flags);
ssize_t recvfrom(int sockfd, void *buffer, size_t length, int flags, struct sockaddr *src, socklen_t *src_len);
int getsockname(int sockfd, struct sockaddr *address, socklen_t *length);
int getpeername(int sockfd, struct sockaddr *address, socklen_t *length);
int setsockopt(int sockfd, int level, int option, const void *value, socklen_t length);
int getsockopt(int sockfd, int level, int option, void *value, socklen_t *length);
int shutdown(int sockfd, int how);

#endif
