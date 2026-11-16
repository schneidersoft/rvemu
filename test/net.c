
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define HOST "google.com"
#define PORT "80"
#define BUFFER_SIZE 4096

int main(void) {
    struct addrinfo hints, *res;
    int sock;
    char buffer[BUFFER_SIZE];

    struct timeval start, end;

    // 1. DNS resolution
    printf("[*] Resolving %s...\n", HOST);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(HOST, PORT, &hints, &res);

    if (status != 0) {
        fprintf(stderr, "DNS error: %s\n", gai_strerror(status));
        return 1;
    }

    printf("[+] DNS resolution successful\n");

    // 2. Create TCP socket
    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sock < 0) {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }

    // 3. Connect to server
    printf("[*] Connecting to %s:%s...\n", HOST, PORT);

    gettimeofday(&start, NULL);

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sock);
        freeaddrinfo(res);
        return 1;
    }

    gettimeofday(&end, NULL);

    freeaddrinfo(res);

    double tcp_time = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;

    printf("[+] TCP connection established!\n");
    printf("[+] TCP connection time: %.2f ms\n", tcp_time);

    // 4. Send HTTP GET request
    char request[512];

    snprintf(request, sizeof(request),
             "GET / HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: Simple-C-Monitor/1.0\r\n"
             "Connection: close\r\n"
             "\r\n",
             HOST);

    printf("[*] Sending HTTP request...\n");

    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        close(sock);
        return 1;
    }

    // 5. Receive server response
    printf("\n========== HTTP RESPONSE ==========\n");

    int bytes;
    int first = 1;

    while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {

        buffer[bytes] = '\0';

        if (first) {
            gettimeofday(&end, NULL);

            double response_time = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;

            printf("[+] Time to first response: %.2f ms\n", response_time);

            first = 0;
        }

        printf("%s", buffer);
    }

    printf("\n===================================\n");

    if (bytes < 0)
        perror("recv");

    // 6. Cleanup
    close(sock);

    printf("[+] Connection closed\n");

    return 0;
}
