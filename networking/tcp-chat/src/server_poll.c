#include "chat.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int client_fds[MAX_CLIENTS];

static void broadcast(int exclude_fd, const char* msg, size_t len) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] != -1 && client_fds[i] != exclude_fd) send(client_fds[i], msg, len, 0);
    }
}

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    int port = argc > 1 ? atoi(argv[1]) : DEFAULT_PORT;
    for (int i = 0; i < MAX_CLIENTS; i++) client_fds[i] = -1;
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return 1;
    listen(listen_fd, 8);
    printf("poll chat server listening on port %d\n", port);
    fflush(stdout);

    struct pollfd pollfds[MAX_CLIENTS + 1];
    for (;;) {
        pollfds[0].fd = listen_fd;
        pollfds[0].events = POLLIN;
        int count = 1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] != -1) {
                pollfds[count].fd = client_fds[i];
                pollfds[count].events = POLLIN;
                count++;
            }
        }
        if (poll(pollfds, (nfds_t)count, -1) < 0) break;
        if (pollfds[0].revents & POLLIN) {
            int fd = accept(listen_fd, NULL, NULL);
            int slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) if (client_fds[i] == -1) { slot = i; break; }
            if (fd >= 0 && slot >= 0) {
                client_fds[slot] = fd;
                char msg[64];
                int m = snprintf(msg, sizeof(msg), "*** client %d joined ***\n", slot);
                broadcast(fd, msg, (size_t)m);
            } else if (fd >= 0) close(fd);
        }
        for (int p = 1; p < count; p++) {
            int slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) if (client_fds[i] == pollfds[p].fd) { slot = i; break; }
            if (slot < 0 || !(pollfds[p].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            char buf[BUF_SIZE];
            ssize_t n = recv(client_fds[slot], buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                close(client_fds[slot]);
                client_fds[slot] = -1;
                char msg[64];
                int m = snprintf(msg, sizeof(msg), "*** client %d left ***\n", slot);
                broadcast(-1, msg, (size_t)m);
            } else {
                buf[n] = '\0';
                char out[BUF_SIZE + 32];
                int m = snprintf(out, sizeof(out), "client %d: %s", slot, buf);
                broadcast(client_fds[slot], out, (size_t)m);
            }
        }
    }
    close(listen_fd);
    return 0;
}