#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock;
    struct sockaddr_in server;
    char request[1024], response[4096];

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Socket creation failed\n");
        return 1;
    }

    server.sin_addr.s_addr = inet_addr("194.164.54.96"); // example.com
    server.sin_family = AF_INET;
    server.sin_port = htons(80);

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Connection failed!\n");
        return 1;
    }

    // Build HTTP GET request
    sprintf(request,
            "GET / HTTP/1.1\r\n"
            "Host: schneidersoft.net\r\n"
            "Connection: close\r\n\r\n");

    // Send request
    send(sock, request, strlen(request), 0);

    // Receive and print response
    int r;
    while ((r = recv(sock, response, sizeof(response)-1, 0)) > 0) {
        response[r] = '\0';
        printf("%s", response);
    }

    close(sock);
    return 0;
}
