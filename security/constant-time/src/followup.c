#include "compare.h"
#include "sha256.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#include <winsock2.h>
#include <windows.h>
#define CLOSE_SOCKET closesocket
typedef SOCKET socket_t;
typedef HANDLE thread_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCKET close
typedef int socket_t;
typedef pthread_t thread_t;
#endif

#define TCP_ITERATIONS 200
#define HMAC_ITERATIONS 2000000

static const unsigned char hmac_key[] = "constant-time-demo-key";
static const unsigned char token[] = "the-real-secret-token";

static void hmac_sha256(const unsigned char *message, size_t message_len,
                        unsigned char digest[32]) {
    unsigned char key_block[64] = {0};
    unsigned char inner[32];
    unsigned char outer[96];
    SHA256_CTX ctx;

    memcpy(key_block, hmac_key, sizeof(hmac_key) - 1);
    for (size_t i = 0; i < sizeof(key_block); i++) key_block[i] ^= 0x36;
    sha256_init(&ctx);
    sha256_update(&ctx, key_block, sizeof(key_block));
    sha256_update(&ctx, message, message_len);
    sha256_final(&ctx, inner);

    memcpy(key_block, hmac_key, sizeof(hmac_key) - 1);
    for (size_t i = 0; i < sizeof(key_block); i++) key_block[i] ^= 0x5c;
    memcpy(outer, key_block, sizeof(key_block));
    memcpy(outer + sizeof(key_block), inner, sizeof(inner));
    sha256_hash(outer, sizeof(outer), digest);
}

static double elapsed_ms(clock_t start) {
    return 1000.0 * (double)(clock() - start) / CLOCKS_PER_SEC;
}

static void run_hmac_experiment(void) {
    unsigned char expected[32];
    unsigned char candidate[32];
    hmac_sha256(token, sizeof(token) - 1, expected);
    printf("HMAC token comparison (%d calls per row)\n", HMAC_ITERATIONS);
    printf("match_len       insecure (ms)   constant-time (ms)\n");
    for (int match_len = 0; match_len <= 32; match_len += 8) {
        memcpy(candidate, expected, sizeof(candidate));
        if (match_len < 32) candidate[match_len] ^= 0xff;
        clock_t start = clock();
        volatile int sink = 0;
        for (int i = 0; i < HMAC_ITERATIONS; i++)
            sink = insecure_compare(expected, candidate, sizeof(expected));
        double insecure_ms = elapsed_ms(start);
        start = clock();
        for (int i = 0; i < HMAC_ITERATIONS; i++)
            sink = constant_time_compare(expected, candidate, sizeof(expected));
        printf("%-16d%-16.2f%-16.2f\n", match_len, insecure_ms,
               elapsed_ms(start));
        (void)sink;
    }
}

static int send_all(socket_t socket, const unsigned char *data, size_t len) {
    while (len > 0) {
        int sent = send(socket, (const char *)data, (int)len, 0);
        if (sent <= 0) return 0;
        data += sent;
        len -= (size_t)sent;
    }
    return 1;
}

static int receive_all(socket_t socket, unsigned char *data, size_t len) {
    while (len > 0) {
        int received = recv(socket, (char *)data, (int)len, 0);
        if (received <= 0) return 0;
        data += received;
        len -= (size_t)received;
    }
    return 1;
}

typedef struct {
    unsigned char expected[32];
    int use_constant_time;
    unsigned short port;
} server_args;

#ifdef _WIN32
static unsigned __stdcall server_main(void *raw_args)
#else
static void *server_main(void *raw_args)
#endif
{
    server_args *args = (server_args *)raw_args;
    socket_t listener = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(args->port);
    bind(listener, (struct sockaddr *)&address, sizeof(address));
    listen(listener, 1);
    socket_t client = accept(listener, NULL, NULL);
    unsigned char candidate[32], reply;
    for (int row = 0; row <= 32; row += 8) {
        for (int i = 0; i < TCP_ITERATIONS; i++) {
            if (!receive_all(client, candidate, sizeof(candidate))) break;
            int equal = args->use_constant_time
                ? constant_time_compare(args->expected, candidate, sizeof(candidate))
                : insecure_compare(args->expected, candidate, sizeof(candidate));
            reply = (unsigned char)equal;
            if (!send_all(client, &reply, 1)) break;
        }
    }
    CLOSE_SOCKET(client);
    CLOSE_SOCKET(listener);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static void run_tcp_experiment(void) {
    unsigned char expected[32];
    hmac_sha256(token, sizeof(token) - 1, expected);
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    printf("TCP loopback round-trip (%d calls per row)\n", TCP_ITERATIONS);
    printf("mode             match_len       elapsed (ms)\n");
    for (int mode = 0; mode < 2; mode++) {
        server_args args;
        memcpy(args.expected, expected, sizeof(expected));
        args.use_constant_time = mode;
        args.port = (unsigned short)(42000 + mode);
#ifdef _WIN32
        thread_t thread = (HANDLE)_beginthreadex(NULL, 0, server_main, &args, 0, NULL);
#else
        thread_t thread;
        pthread_create(&thread, NULL, server_main, &args);
#endif
        socket_t client = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in address = {0};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(args.port);
        while (connect(client, (struct sockaddr *)&address, sizeof(address)) != 0) {}
        for (int row = 0; row <= 32; row += 8) {
            unsigned char candidate[32], reply;
            memcpy(candidate, expected, sizeof(candidate));
            if (row < 32) candidate[row] ^= 0xff;
            clock_t start = clock();
            for (int i = 0; i < TCP_ITERATIONS; i++) {
                send_all(client, candidate, sizeof(candidate));
                receive_all(client, &reply, 1);
            }
            printf("%-17s%-16d%.2f\n", mode ? "constant-time" : "insecure",
                   row, elapsed_ms(start));
        }
        CLOSE_SOCKET(client);
#ifdef _WIN32
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
#else
        pthread_join(thread, NULL);
#endif
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

int main(void) {
    run_hmac_experiment();
    run_tcp_experiment();
    return 0;
}