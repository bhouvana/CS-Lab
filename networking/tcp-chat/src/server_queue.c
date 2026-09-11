#include "chat.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define OUTPUT_CAP (64 * 1024)
struct client { int fd; size_t pending; char output[OUTPUT_CAP]; };
static struct client clients[MAX_CLIENTS];

static void drop_client(int slot) {
    close(clients[slot].fd);
    clients[slot].fd = -1;
    clients[slot].pending = 0;
}

static void queue_message(int exclude, const char* msg, size_t len) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == -1 || clients[i].fd == exclude) continue;
        if (len > OUTPUT_CAP - clients[i].pending) {
            drop_client(i);
            continue;
        }
        memcpy(clients[i].output + clients[i].pending, msg, len);
        clients[i].pending += len;
    }
}

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    int port = argc > 1 ? atoi(argv[1]) : DEFAULT_PORT;
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;
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
    printf("queued chat server listening on port %d\n", port);
    fflush(stdout);

    struct pollfd pollfds[MAX_CLIENTS + 1];
    for (;;) {
        pollfds[0] = (struct pollfd){listen_fd, POLLIN, 0};
        int count = 1;
        for (int i = 0; i < MAX_CLIENTS; i++) if (clients[i].fd != -1) {
            pollfds[count] = (struct pollfd){clients[i].fd, (short)(POLLIN | (clients[i].pending ? POLLOUT : 0)), 0};
            count++;
        }
        if (poll(pollfds, (nfds_t)count, -1) < 0) break;
        if (pollfds[0].revents & POLLIN) {
            int fd = accept(listen_fd, NULL, NULL);
            int slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) if (clients[i].fd == -1) { slot = i; break; }
            if (fd >= 0 && slot >= 0) {
                int flags = fcntl(fd, F_GETFL, 0);
                fcntl(fd, F_SETFL, flags | O_NONBLOCK);
                clients[slot] = (struct client){fd, 0, {0}};
                char msg[64];
                int m = snprintf(msg, sizeof(msg), "*** client %d joined ***\n", slot);
                queue_message(fd, msg, (size_t)m);
            } else if (fd >= 0) close(fd);
        }
        for (int p = 1; p < count; p++) {
            int slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) if (clients[i].fd == pollfds[p].fd) { slot = i; break; }
            if (slot < 0) continue;
            if (pollfds[p].revents & POLLOUT) {
                ssize_t sent = send(clients[slot].fd, clients[slot].output, clients[slot].pending, 0);
                if (sent > 0) {
                    memmove(clients[slot].output, clients[slot].output + sent, clients[slot].pending - (size_t)sent);
                    clients[slot].pending -= (size_t)sent;
                } else if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) drop_client(slot);
            }
            if (clients[slot].fd != -1 && (pollfds[p].revents & (POLLIN | POLLHUP | POLLERR))) {
                char buf[BUF_SIZE];
                ssize_t n = recv(clients[slot].fd, buf, sizeof(buf) - 1, 0);
                if (n <= 0) {
                    drop_client(slot);
                    int m = snprintf(buf, sizeof(buf), "*** client %d left ***\n", slot);
                    queue_message(-1, buf, (size_t)m);
                } else {
                    buf[n] = '\0';
                    char out[BUF_SIZE + 32];
                    int m = snprintf(out, sizeof(out), "client %d: %s", slot, buf);
                    queue_message(clients[slot].fd, out, (size_t)m);
                }
            }
        }
    }
    close(listen_fd);
    return 0;
}