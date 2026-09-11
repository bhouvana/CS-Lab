// connect() then select() over (stdin, socket) so typing and receiving
// can happen at the same time without threads.
#include "chat.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <server-ip> [port]\n", argv[0]);
        return 2;
    }
    int port = argc > 2 ? atoi(argv[2]) : DEFAULT_PORT;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid address: %s\n", argv[1]);
        return 2;
    }
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }
    printf("connected to %s:%d -- type messages, Ctrl-D to quit\n", argv[1], port);

    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(fd, &readfds);
        int maxfd = fd > STDIN_FILENO ? fd : STDIN_FILENO;

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) break;

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char line[BUF_SIZE];
            if (!fgets(line, sizeof(line), stdin)) break; // EOF: Ctrl-D
            send(fd, line, strlen(line), 0);
        }
        if (FD_ISSET(fd, &readfds)) {
            char buf[BUF_SIZE];
            ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                printf("server closed the connection\n");
                break;
            }
            buf[n] = '\0';
            fputs(buf, stdout);
        }
    }
    close(fd);
    return 0;
}
