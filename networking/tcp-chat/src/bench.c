// Experiment: how does broadcast latency scale with client count?
// Connects N clients, has one send a message, and times how long
// until the LAST of the other N-1 clients receives it. One server for
// the whole run (restarting a server per round turned out to be
// fragile -- see README) -- each round connects fresh clients and
// closes them before the next round starts.
#include "chat.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BENCH_PORT 5588

static pid_t start_server(void) {
    fflush(stdout); // avoid duplicating our own buffered output into the forked child
    pid_t pid = fork();
    if (pid == 0) {
        if (!freopen("/dev/null", "w", stdout)) _exit(126);
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", BENCH_PORT);
        execl("./server", "./server", port_str, (char*)NULL);
        _exit(127);
    }
    usleep(200000);
    return pid;
}

static int connect_client(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BENCH_PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    for (int attempt = 0; attempt < 30; attempt++) {
        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) return fd;
        usleep(20000);
    }
    return -1;
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static double time_broadcast(int num_clients) {
    int fds[MAX_CLIENTS] = {0};
    for (int i = 0; i < num_clients; i++) {
        fds[i] = connect_client();
        // The new client's join notice broadcasts to every
        // previously-connected client, not just fds[0] -- drain it
        // from all of them, or stale bytes corrupt a later read.
        for (int j = 0; j < i; j++) {
            char drain[256];
            recv(fds[j], drain, sizeof(drain), 0);
        }
    }

    double t0 = now_ms();
    const char* msg = "ping\n";
    send(fds[0], msg, strlen(msg), 0);
    for (int i = 1; i < num_clients; i++) {
        char buf[256];
        recv(fds[i], buf, sizeof(buf), 0);
    }
    double elapsed = now_ms() - t0;

    for (int i = 0; i < num_clients; i++) close(fds[i]);
    // Give the server's select() loop time to notice all N
    // disconnects before the next round connects new clients into the
    // same (reused) client-slot array.
    usleep(150000);
    return elapsed;
}

int main(void) {
    pid_t server_pid = start_server();

    time_broadcast(4); // warm-up, discarded (see README)

    printf("Benchmark: broadcast latency vs. client count\n");
    printf("(best of 3 trials per size, after a warm-up round)\n\n");
    printf("%-14s%s\n", "clients", "ms until all others received the message");
    int sizes[] = {2, 4, 8, 16, 32};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        double best = -1.0;
        for (int trial = 0; trial < 3; trial++) {
            double ms = time_broadcast(sizes[i]);
            if (best < 0.0 || ms < best) best = ms;
        }
        printf("%-14d%.3f\n", sizes[i], best);
        fflush(stdout);
    }

    kill(server_pid, SIGTERM);
    waitpid(server_pid, NULL, 0);
    return 0;
}
