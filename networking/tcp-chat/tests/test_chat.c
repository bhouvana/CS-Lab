// Integration tests: fork()+exec() the real server binary, then drive
// it with raw client sockets (no interactive stdin needed -- that's
// what src/client.c is for). Plain assert-based, no framework.
//
// Each test gets its OWN server process on its OWN port. Sharing one
// server across tests caused intermittent failures: a previous test's
// asynchronous disconnect-broadcast (the server notices a close()
// only when its own select() loop next wakes up, on its own timing)
// could still be in flight when the next test's clients connected,
// so a "left" notice from test N sometimes arrived during test N+1's
// read instead. Fresh server per test = no shared state, no leakage.
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t start_server(int port) {
    // Classic fork() gotcha: stdout here is fully buffered (not a
    // TTY), so any of THIS process's unflushed printf output would
    // otherwise be duplicated into the child and printed again
    // whenever the child's copy of the buffer eventually flushes.
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        if (!freopen("/dev/null", "w", stdout)) _exit(126); // keep test output focused on our own printf's
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", port);
        execl("./server", "./server", port_str, (char*)NULL);
        _exit(127); // execl failed
    }
    usleep(200000); // give the server a moment to bind and listen
    return pid;
}

static void stop_server(pid_t pid) {
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
}

static int connect_client(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    for (int attempt = 0; attempt < 30; attempt++) {
        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) return fd;
        usleep(50000);
    }
    fprintf(stderr, "could not connect to test server on port %d\n", port);
    exit(1);
}

static ssize_t recv_line(int fd, char* buf, size_t size) {
    ssize_t n = recv(fd, buf, size - 1, 0);
    if (n > 0) buf[(size_t)n] = '\0';
    return n;
}

static void test_join_broadcast_to_existing_clients_normal_case(void) {
    int port = 5601;
    pid_t server = start_server(port);
    int a = connect_client(port);
    int b = connect_client(port); // a should be notified of b joining

    char buf[256];
    ssize_t n = recv_line(a, buf, sizeof(buf));
    assert(n > 0);
    assert(strstr(buf, "joined") != NULL);

    close(a);
    close(b);
    stop_server(server);
    printf("ok: an existing client is notified when a new client joins\n");
}

static void test_message_broadcasts_to_others_not_sender_normal_case(void) {
    int port = 5602;
    pid_t server = start_server(port);
    int a = connect_client(port);
    int b = connect_client(port);
    char buf[256];
    recv_line(a, buf, sizeof(buf)); // consume b's join notice

    const char* msg = "hello from a\n";
    send(a, msg, strlen(msg), 0);

    ssize_t n = recv_line(b, buf, sizeof(buf));
    assert(n > 0);
    assert(strstr(buf, "hello from a") != NULL);

    close(a);
    close(b);
    stop_server(server);
    printf("ok: a message broadcasts to other clients\n");
}

static void test_disconnect_notifies_remaining_clients_normal_case(void) {
    int port = 5603;
    pid_t server = start_server(port);
    int a = connect_client(port);
    int b = connect_client(port);
    char buf[256];
    recv_line(a, buf, sizeof(buf)); // consume b's join notice

    close(b); // b disconnects
    ssize_t n = recv_line(a, buf, sizeof(buf));
    assert(n > 0);
    assert(strstr(buf, "left") != NULL);

    close(a);
    stop_server(server);
    printf("ok: remaining clients are notified when a client disconnects\n");
}

static void test_third_client_does_not_see_earlier_private_exchange_edge_case(void) {
    // c connects after a's message to b already happened -- it must
    // not receive anything from that earlier exchange.
    int port = 5604;
    pid_t server = start_server(port);
    int a = connect_client(port);
    int b = connect_client(port);
    char buf[256];
    recv_line(a, buf, sizeof(buf)); // consume b's join

    send(a, "secret\n", 7, 0);
    recv_line(b, buf, sizeof(buf)); // b receives it; drain it

    int c = connect_client(port);
    // Bounded wait: if any stray data shows up for c, select() returns
    // >0 and the assert fails; a clean pass means nothing arrived.
    struct timeval tv = {0, 200000};
    fd_set set;
    FD_ZERO(&set);
    FD_SET(c, &set);
    int ready = select(c + 1, &set, NULL, NULL, &tv);
    assert(ready == 0); // nothing pending for c

    close(a);
    close(b);
    close(c);
    stop_server(server);
    printf("ok: a newly joined client sees no earlier private exchange\n");
}

static void test_simultaneous_disconnects_do_not_crash_server_regression(void) {
    // Multiple clients closing at once (a single select() wakeup can
    // report several disconnects together) used to kill the server
    // outright: broadcasting a "left" notice can send() to a client
    // whose own disconnect is being processed later in the same pass,
    // raising SIGPIPE, whose default action terminates the process.
    // Fixed with signal(SIGPIPE, SIG_IGN) in server.c.
    int port = 5605;
    pid_t server = start_server(port);

    int fds[6];
    for (int i = 0; i < 6; i++) {
        fds[i] = connect_client(port);
        char drain[256];
        for (int j = 0; j < i; j++) recv(fds[j], drain, sizeof(drain), 0);
    }
    for (int i = 0; i < 6; i++) close(fds[i]); // all at once, no delay between them
    usleep(300000); // give the server time to process every disconnect

    // The server must still be alive and accepting new connections.
    int survivor = connect_client(port);
    assert(survivor >= 0);

    close(survivor);
    stop_server(server);
    printf("ok: simultaneous disconnects don't crash the server (SIGPIPE regression)\n");
}

static void test_nick_command_changes_message_prefix(void) {
    int port = 5606;
    pid_t server = start_server(port);
    int a = connect_client(port);
    int b = connect_client(port);
    char buf[256];
    recv_line(a, buf, sizeof(buf));

    send(a, "/nick Ada\n", 10, 0);
    recv_line(b, buf, sizeof(buf));
    assert(strstr(buf, "is now Ada") != NULL);

    send(a, "hello\n", 6, 0);
    recv_line(b, buf, sizeof(buf));
    assert(strstr(buf, "Ada: hello") != NULL);

    close(a);
    close(b);
    stop_server(server);
    printf("ok: /nick changes the message prefix\n");
}

int main(void) {
    test_join_broadcast_to_existing_clients_normal_case();
    test_message_broadcasts_to_others_not_sender_normal_case();
    test_disconnect_notifies_remaining_clients_normal_case();
    test_third_client_does_not_see_earlier_private_exchange_edge_case();
    test_simultaneous_disconnects_do_not_crash_server_regression();
    test_nick_command_changes_message_prefix();
    printf("all tests passed\n");
    return 0;
}
