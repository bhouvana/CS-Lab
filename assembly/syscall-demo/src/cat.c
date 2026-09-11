// Byte-for-byte stdin-to-stdout copy using only the raw syscall wrappers.
#include <stddef.h>

extern long my_read(int fd, void* buf, unsigned long count);
extern long my_write(int fd, const void* buf, unsigned long count);
extern void my_exit(int status) __attribute__((noreturn));

int main(void) {
    char buffer[4096];

    for (;;) {
        long read_count = my_read(0, buffer, sizeof(buffer));
        if (read_count == 0) {
            my_exit(0);
        }
        if (read_count < 0) {
            my_exit(1);
        }

        long written = 0;
        while (written < read_count) {
            long write_count = my_write(1, buffer + written, (unsigned long)(read_count - written));
            if (write_count <= 0) {
                my_exit(1);
            }
            written += write_count;
        }
    }
}