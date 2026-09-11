// socket() -> bind() -> listen() -> select() over (listener + clients)
// -> accept() / recv() / broadcast()
#include "chat.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static int client_fds[MAX_CLIENTS];

// Sends `msg` to every connected client except `exclude_fd` (pass -1
// to exclude none, e.g. for a disconnect notice).
static void broadcast(int exclude_fd, const char* msg, size_t len) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] != -1 && client_fds[i] != exclude_fd) send(client_fds[i], msg, len, 0);
    }
}

int main(int argc, char** argv) {
    // Without this, send()-ing to a client that has already closed its
    // end (very easy to hit: broadcast() can still reach a slot for a
    // client whose disconnect is being processed in this same select()
    // wakeup, just later in the loop) raises SIGPIPE, whose DEFAULT
    // action silently kills this entire process. Ignoring it makes
    // send() fail with EPIPE instead -- an ordinary, recoverable error.
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

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    listen(listen_fd, 8);

    printf("chat server listening on port %d\n", port);
    fflush(stdout);

    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        int maxfd = listen_fd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] != -1) {
                FD_SET(client_fds[i], &readfds);
                if (client_fds[i] > maxfd) maxfd = client_fds[i];
            }
        }

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(listen_fd, &readfds)) {
            int client_fd = accept(listen_fd, NULL, NULL);
            if (client_fd >= 0) {
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_fds[i] == -1) {
                        slot = i;
                        break;
                    }
                }
                if (slot == -1) {
                    fprintf(stderr, "server full, rejecting connection\n");
                    close(client_fd);
                } else {
                    client_fds[slot] = client_fd;
                    printf("client %d connected (fd=%d)\n", slot, client_fd);
                    fflush(stdout);
                    char msg[64];
                    int m = snprintf(msg, sizeof(msg), "*** client %d joined ***\n", slot);
                    broadcast(client_fd, msg, (size_t)m);
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] == -1 || !FD_ISSET(client_fds[i], &readfds)) continue;

            char buf[BUF_SIZE];
            ssize_t n = recv(client_fds[i], buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                printf("client %d disconnected\n", i);
                fflush(stdout);
                close(client_fds[i]);
                client_fds[i] = -1; // mark free BEFORE broadcasting, so the closed fd is never sent to
                char msg[64];
                int m = snprintf(msg, sizeof(msg), "*** client %d left ***\n", i);
                broadcast(-1, msg, (size_t)m);
            } else {
                buf[n] = '\0';
                char out[BUF_SIZE + 32];
                int m = snprintf(out, sizeof(out), "client %d: %s", i, buf);
                broadcast(client_fds[i], out, (size_t)m);
            }
        }
    }
    close(listen_fd);
    return 0;
}
