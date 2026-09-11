# Linux/POSIX only (POSIX sockets: socket/bind/listen/accept/select).
UNAME_S := $(shell uname -s)
CC := gcc
CFLAGS := -Wall -Wextra -O2 -Iinclude

.PHONY: build test test-asan benchmark experiments clean

ifeq ($(UNAME_S),Linux)

build: server client poll_server queue_server

server: src/server.c include/chat.h
	$(CC) $(CFLAGS) src/server.c -o server

client: src/client.c include/chat.h
	$(CC) $(CFLAGS) src/client.c -o client

poll_server: src/server_poll.c include/chat.h
	$(CC) $(CFLAGS) src/server_poll.c -o poll_server

queue_server: src/server_queue.c include/chat.h
	$(CC) $(CFLAGS) src/server_queue.c -o queue_server

chat_test: tests/test_chat.c include/chat.h
	$(CC) $(CFLAGS) tests/test_chat.c -o chat_test

# ASan/UBSan (CS-LAB.md §40) as a separate target -- see os/allocator's
# README for why it isn't the default `test` on this repo's toolchain.
# chat_test fork()+exec()s "./server" by that exact relative path, so
# the sanitized server has to land there too, replacing the plain one.
server_asan: src/server.c include/chat.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g src/server.c -o server

chat_test_asan: tests/test_chat.c include/chat.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g tests/test_chat.c -o chat_test_asan

chat_bench: src/bench.c include/chat.h
	$(CC) $(CFLAGS) src/bench.c -o chat_bench

test: server chat_test
	./chat_test

test-asan: server_asan chat_test_asan
	./chat_test_asan

benchmark: server chat_bench
	./chat_bench

experiments: poll_server queue_server
	@echo "built poll_server and queue_server"

else

build test test-asan benchmark:
	@echo "skip: networking/tcp-chat is Linux/POSIX only (see README) -- detected '$(UNAME_S)'"

endif

clean:
	rm -f server client poll_server queue_server chat_test chat_test_asan chat_bench
