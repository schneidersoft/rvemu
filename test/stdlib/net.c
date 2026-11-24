/*
 * BSD socket wrappers and a small stub resolver for the freestanding RVEMU
 * guest.
 *
 * All traffic is forwarded to the host by the emulator; this file only speaks
 * the Linux RISC-V socket ABI and parses DNS/UDP replies for A records.
 * Addresses are kept in network byte order inside the usual socket structs,
 * which lets the emulator copy them byte for byte.
 */

#include <errno.h>
#include <netdb.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "linux.h"

enum {
    DNS_PORT = 53,
    DNS_PACKET_SIZE = 512,
    DNS_QUERY_SIZE = 256,
    DNS_HEADER_SIZE = 12,
    DNS_TIMEOUT_SECONDS = 2,
    DNS_TYPE_A = 1,
    DNS_CLASS_IN = 1,
    DNS_QUESTION_ID = 0x1337,
    DNS_MAX_LABELS = 128,
};

static uint16_t byte_swap16(uint16_t value) { return (uint16_t)((value << 8) | (value >> 8)); }

static uint32_t byte_swap32(uint32_t value) {
    return (uint32_t)(((value & 0xFF000000U) >> 24) | ((value & 0x00FF0000U) >> 8) | ((value & 0x0000FF00U) << 8) | ((value & 0x000000FFU) << 24));
}

static uint16_t get_be16(const uint8_t *data) { return (uint16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]); }

static void put_be16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

/* Translate a dotted quad into the network byte order stored in sockaddr_in. */
static int parse_ipv4(const char *text, uint32_t *address) {
    uint32_t result = 0;
    int parts = 0;
    int value = 0;
    int digits = 0;

    if (text == NULL) {
        return -1;
    }

    for (;;) {
        char character = *text++;

        if (character >= '0' && character <= '9') {
            if (digits == 3) {
                return -1;
            }
            value = value * 10 + (character - '0');
            if (value > 255) {
                return -1;
            }
            digits++;
        } else if (character == '.' || character == '\0') {
            if (digits == 0 || parts == 4) {
                return -1;
            }
            result = (result << 8) | (uint32_t)value;
            parts++;
            value = 0;
            digits = 0;
            if (character == '\0') {
                break;
            }
        } else {
            return -1;
        }
    }

    if (parts != 4) {
        return -1;
    }
    /* result is in host order (first octet in the top byte); s_addr must hold
     * network byte order, i.e. the first octet in the first memory byte. */
    *address = byte_swap32(result);
    return 0;
}

static size_t build_query(uint8_t *packet, const char *name, uint16_t id) {
    size_t offset = DNS_HEADER_SIZE;

    memset(packet, 0, DNS_HEADER_SIZE);
    put_be16(packet, id);
    put_be16(packet + 2, 0x0100); /* recursion desired */
    put_be16(packet + 4, 1);      /* one question */

    while (*name != '\0') {
        const char *dot = name;
        size_t label;

        while (*dot != '\0' && *dot != '.') {
            dot++;
        }
        label = (size_t)(dot - name);
        if (label == 0 || label > 63 || offset + label + 5 > DNS_QUERY_SIZE) {
            return 0;
        }
        packet[offset++] = (uint8_t)label;
        memcpy(packet + offset, name, label);
        offset += label;
        name = *dot == '\0' ? dot : dot + 1;
    }

    packet[offset++] = 0; /* root label */
    put_be16(packet + offset, DNS_TYPE_A);
    put_be16(packet + offset + 2, DNS_CLASS_IN);
    return offset + 4;
}

/* Return the offset just past a (possibly compressed) name, or -1. */
static long skip_name(const uint8_t *packet, size_t size, size_t offset) {
    int labels = 0;

    while (offset < size) {
        uint8_t length = packet[offset];

        if (length == 0) {
            return (long)offset + 1;
        }
        if ((length & 0xc0) == 0xc0) {
            if (offset + 2 > size) {
                return -1;
            }
            return (long)offset + 2;
        }
        if ((length & 0xc0) != 0 || length > 63 || labels++ > DNS_MAX_LABELS) {
            return -1;
        }
        offset += (size_t)length + 1;
    }
    return -1;
}

static int find_a_record(const uint8_t *packet, size_t size, uint32_t *address) {
    size_t offset;
    size_t answers;
    long end;

    if (size < DNS_HEADER_SIZE) {
        return -1;
    }

    answers = get_be16(packet + 6);
    end = skip_name(packet, size, DNS_HEADER_SIZE);
    if (end < 0 || (size_t)end + 4 > size) {
        return -1;
    }
    offset = (size_t)end + 4; /* question type and class */

    for (size_t i = 0; i < answers; i++) {
        uint16_t type;
        uint16_t record_class;
        uint16_t length;
        const uint8_t *data;

        end = skip_name(packet, size, offset);
        if (end < 0 || (size_t)end + 10 > size) {
            return -1;
        }

        type = get_be16(packet + end);
        record_class = get_be16(packet + end + 2);
        length = get_be16(packet + end + 8); /* name, type, class, ttl, rdlength */
        data = packet + end + 10;
        if ((size_t)end + 10 + (size_t)length > size) {
            return -1;
        }
        if (type == DNS_TYPE_A && record_class == DNS_CLASS_IN && length == 4) {
            memcpy(address, data, 4);
            return 0;
        }
        offset = (size_t)end + 10 + (size_t)length;
    }
    return -1;
}

/* Read the first IPv4 nameserver listed in /etc/resolv.conf. */
static int resolver_from_file(uint32_t *address) {
    char buffer[2048];
    char *line;
    long length = 0;
    int fd = (int)linux_openat(AT_FDCWD, "/etc/resolv.conf", O_RDONLY, 0);

    if (fd < 0) {
        return -1;
    }
    while (length < (long)sizeof(buffer) - 1) {
        long received = linux_read(fd, buffer + length, (unsigned long)((long)sizeof(buffer) - 1 - length));
        if (received <= 0) {
            break;
        }
        length += received;
    }
    linux_close(fd);
    if (length <= 0) {
        return -1;
    }
    buffer[length] = '\0';

    line = buffer;
    while (*line != '\0') {
        char *end = line;
        char saved;
        const char *text;

        while (*end != '\0' && *end != '\n') {
            end++;
        }
        saved = *end;
        *end = '\0';

        text = line;
        while (*text == ' ' || *text == '\t') {
            text++;
        }
        if (strncmp(text, "nameserver", 10) == 0 && (text[10] == ' ' || text[10] == '\t')) {
            const char *stop;
            char candidate[32];
            size_t candidate_length;

            text += 10;
            while (*text == ' ' || *text == '\t') {
                text++;
            }
            stop = text;
            while (*stop != '\0' && *stop != ' ' && *stop != '\t') {
                stop++;
            }
            candidate_length = (size_t)(stop - text);
            if (candidate_length > 0 && candidate_length < sizeof(candidate)) {
                memcpy(candidate, text, candidate_length);
                candidate[candidate_length] = '\0';
                if (parse_ipv4(candidate, address) == 0) {
                    return 0;
                }
            }
        }

        if (saved == '\0') {
            break;
        }
        line = end + 1;
    }
    return -1;
}

static int query(uint32_t server, const char *name, uint16_t id, uint8_t *response, size_t *response_size) {
    uint8_t request[DNS_QUERY_SIZE];
    struct sockaddr_in destination;
    struct timeval timeout = {DNS_TIMEOUT_SECONDS, 0};
    size_t request_length = build_query(request, name, id);
    long received;
    int fd;

    if (request_length == 0) {
        return -1;
    }

    fd = (int)linux_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        return -1;
    }
    linux_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = byte_swap16(DNS_PORT);
    destination.sin_addr.s_addr = server;

    if (linux_sendto(fd, request, request_length, 0, &destination, sizeof(destination)) != (long)request_length) {
        linux_close(fd);
        return -1;
    }
    received = linux_recvfrom(fd, response, DNS_PACKET_SIZE, 0, 0, 0);
    linux_close(fd);

    if (received < DNS_HEADER_SIZE || get_be16(response) != id || (get_be16(response + 2) & 0x8000) == 0) {
        return -1;
    }
    *response_size = (size_t)received;
    return 0;
}

static int socket_result(long result) {
    if (result < 0) {
        errno = -(int)result;
        return -1;
    }
    return (int)result;
}

static ssize_t count_result(long result) {
    if (result < 0) {
        errno = -(int)result;
        return -1;
    }
    return (ssize_t)result;
}

static int status_result(long result) { return count_result(result) < 0 ? -1 : 0; }

int socket(int domain, int type, int protocol) { return socket_result(linux_socket(domain, type, protocol)); }

int connect(int fd, const struct sockaddr *address, socklen_t length) { return status_result(linux_connect(fd, address, (long)length)); }

ssize_t send(int fd, const void *buffer, size_t length, int flags) {
    return count_result(linux_sendto(fd, buffer, (unsigned long)length, flags, NULL, 0));
}

ssize_t recv(int fd, void *buffer, size_t length, int flags) {
    return count_result(linux_recvfrom(fd, buffer, (unsigned long)length, flags, NULL, 0));
}

int setsockopt(int fd, int level, int option, const void *value, socklen_t length) {
    return status_result(linux_setsockopt(fd, level, option, value, (long)length));
}

int getsockopt(int fd, int level, int option, void *value, socklen_t *length) {
    if (length == NULL) {
        errno = EFAULT;
        return -1;
    }
    return status_result(linux_getsockopt(fd, level, option, value, length));
}

int shutdown(int fd, int how) { return status_result(linux_shutdown(fd, how)); }

static struct {
    struct addrinfo info;
    struct sockaddr_in address;
} resolution;

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **result) {
    uint32_t servers[3];
    uint8_t response[DNS_PACKET_SIZE];
    size_t response_size = 0;
    size_t server_count = 0;
    uint32_t address;
    uint16_t port = 0;

    if (node == NULL || result == NULL) {
        return EAI_NONAME;
    }
    if (hints != NULL) {
        if (hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET) {
            return EAI_FAMILY;
        }
        if (hints->ai_socktype != 0 && hints->ai_socktype != SOCK_STREAM) {
            return EAI_SOCKTYPE;
        }
    }

    if (service != NULL) {
        int value = atoi(service);
        if (value <= 0 || value > 65535) {
            return EAI_SERVICE;
        }
        port = (uint16_t)value;
    }

    memset(&resolution, 0, sizeof(resolution));
    resolution.info.ai_family = AF_INET;
    resolution.info.ai_socktype = SOCK_STREAM;
    resolution.info.ai_protocol = IPPROTO_TCP;
    resolution.info.ai_addrlen = sizeof(resolution.address);
    resolution.info.ai_addr = (struct sockaddr *)&resolution.address;
    resolution.address.sin_family = AF_INET;
    resolution.address.sin_port = byte_swap16(port);

    if (parse_ipv4(node, &resolution.address.sin_addr.s_addr) == 0) {
        *result = &resolution.info;
        return 0;
    }

    if (resolver_from_file(&address) == 0) {
        servers[server_count++] = address;
    }
    servers[server_count++] = 0x08080808U; /* 8.8.8.8 */
    servers[server_count++] = 0x01010101U; /* 1.1.1.1 */

    for (size_t i = 0; i < server_count; i++) {
        if (query(servers[i], node, DNS_QUESTION_ID, response, &response_size) == 0 &&
            find_a_record(response, response_size, &resolution.address.sin_addr.s_addr) == 0) {
            *result = &resolution.info;
            return 0;
        }
    }
    return EAI_NODATA;
}

void freeaddrinfo(struct addrinfo *result) { (void)result; }

const char *gai_strerror(int code) {
    switch (code) {
    case 0:
        return "Success";
    case EAI_BADFLAGS:
        return "Invalid flags";
    case EAI_NONAME:
        return "Name does not resolve";
    case EAI_FAMILY:
        return "Unsupported address family";
    case EAI_SOCKTYPE:
        return "Unsupported socket type";
    case EAI_SERVICE:
        return "Unsupported service";
    case EAI_NODATA:
        return "No address associated with name";
    case EAI_MEMORY:
        return "Memory allocation failure";
    case EAI_SYSTEM:
        return "System error";
    default:
        return "Unknown error";
    }
}
