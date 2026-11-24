/*
 * Hermetic net self-test.
 *
 * Dials a local HTTP server on 127.0.0.1:8931 (test/net/net.py serves
 * test/net/index.html in-process), performs an
 * HTTP/1.1 `GET /`, reads the whole response, and verifies:
 *
 *   - the HTTP status is 200
 *   - the body contains the marker `rvemu-net-ok`
 *
 * No DNS or external network is needed: the stub resolver
 * (test/stdlib/net.c) short-circuits IPv4 literals, and 127.0.0.1 reaches
 * the server straight through the host's loopback via the emulator's
 * socket forwarding.
 *
 * Exit status: 0 (PASS) iff the checks above held; anything else (no
 * server, connection refused, non-200, missing marker) is a FAIL.
 */

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define HOST "127.0.0.1"
#define PORT "8931"
#define MARKER "rvemu-net-ok"
#define BUFFER_SIZE 8192 /* server headers + index.html, with room to spare */

int main(void) {
    struct addrinfo hints, *res;
    int status;
    int sock;
    int bytes;
    int first = 1;
    int ok = 1;
    char buffer[BUFFER_SIZE + 1];
    size_t total = 0;
    struct timeval t0, t1;

    printf("[*] hermetic net test: dial %s:%s\n", HOST, PORT);

    gettimeofday(&t0, NULL);

    // 1. Resolve — an IPv4 literal; the stub resolver does not emit DNS.
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(HOST, PORT, &hints, &res);

    if (status != 0) {
        fprintf(stderr, "FAIL: getaddrinfo(%s): %s\n", HOST, gai_strerror(status));
        return 1;
    }

    // 2. Connect.
    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sock < 0) {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sock);
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res);

    printf("[+] TCP connection to %s:%s established\n", HOST, PORT);

    // 3. Send the request.
    char request[512];

    snprintf(request, sizeof(request),
             "GET / HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: rvemu-net-test/1.0\r\n"
             "Connection: close\r\n"
             "\r\n",
             HOST);

    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        close(sock);
        return 1;
    }

    printf("[*] GET / sent\n");

    // 4. Read the whole response. The server closes the connection after
    //    its reply, so `recv()` returns 0 when it is done.
    while ((bytes = recv(sock, buffer + total, sizeof(buffer) - 1 - total, 0)) > 0) {
        if (first) {
            first = 0;
            gettimeofday(&t1, NULL);
        }
        total += (size_t)bytes;
    }

    if (bytes < 0) {
        perror("recv");
        close(sock);
        return 1;
    }

    buffer[total] = '\0';
    close(sock);

    printf("[*] received %d bytes, connection closed\n", (int)total);

    if (!first) {
        double ttfb = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_usec - t0.tv_usec) / 1000.0;
        printf("[+] first byte after %.2f ms\n", ttfb);
    }

    // 5. Verify the response.
    printf("\n--- server response ---\n%s\n--------------------\n", buffer);

    if (total == 0) {
        fprintf(stderr, "FAIL: server closed the connection without sending anything\n");
        ok = 0;
    }
    else if (strncmp(buffer, "HTTP/1.0 200", 12) != 0 && strncmp(buffer, "HTTP/1.1 200", 12) != 0) {
        fprintf(stderr, "FAIL: expected HTTP 200, got: %s\n", buffer);
        ok = 0;
    }
    else if (strstr(buffer, MARKER) == NULL) {
        fprintf(stderr, "FAIL: marker %s not found in response\n", MARKER);
        ok = 0;
    }

    if (ok) {
        printf("[+] PASS: HTTP 200 with marker %s from %s:%s\n", MARKER, HOST, PORT);
        return 0;
    }

    printf("[-] FAIL\n");
    return 1;
}
