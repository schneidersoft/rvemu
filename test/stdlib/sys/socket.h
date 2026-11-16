#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef unsigned int socklen_t;
typedef uint16_t sa_family_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

enum {
    AF_UNSPEC = 0,
    AF_UNIX = 1,
    AF_INET = 2,
    AF_INET6 = 10,
};

enum {
    PF_UNSPEC = AF_UNSPEC,
    PF_UNIX = AF_UNIX,
    PF_INET = AF_INET,
    PF_INET6 = AF_INET6,
};

enum {
    SOCK_STREAM = 1,
    SOCK_DGRAM = 2,
    SOCK_RAW = 3,
    SOCK_SEQPACKET = 5,
};

enum {
    SOL_SOCKET = 1,
    IPPROTO_IP = 0,
    IPPROTO_ICMP = 1,
    IPPROTO_TCP = 6,
    IPPROTO_UDP = 17,
    IPPROTO_ICMPV6 = 58,
    IPPROTO_RAW = 255,
};

enum {
    SHUT_RD = 0,
    SHUT_WR = 1,
    SHUT_RDWR = 2,
};

/* Linux socket option numbers (asm-generic, identical on every 64-bit ABI). */
enum {
    SO_DEBUG = 1,
    SO_REUSEADDR = 2,
    SO_TYPE = 3,
    SO_ERROR = 4,
    SO_DONTROUTE = 5,
    SO_BROADCAST = 6,
    SO_SNDBUF = 7,
    SO_RCVBUF = 8,
    SO_KEEPALIVE = 9,
    SO_OOBINLINE = 10,
    SO_RCVTIMEO = 20,
    SO_SNDTIMEO = 21,
};

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
int connect(int sockfd, const struct sockaddr *address, socklen_t length);
ssize_t send(int sockfd, const void *buffer, size_t length, int flags);
ssize_t recv(int sockfd, void *buffer, size_t length, int flags);
int setsockopt(int sockfd, int level, int option, const void *value, socklen_t length);
int getsockopt(int sockfd, int level, int option, void *value, socklen_t *length);
int shutdown(int sockfd, int how);

#endif
