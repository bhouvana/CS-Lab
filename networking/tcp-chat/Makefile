# Linux/POSIX only (POSIX sockets: socket/bind/listen/accept/select).
UNAME_S := $(shell uname -s)
CC := gcc
CFLAGS := -Wall -Wextra -O2 -Iinclude

.PHONY: build test benchmark clean

ifeq ($(UNAME_S),Linux)

build: server client

server: src/server.c include/chat.h
	$(CC) $(CFLAGS) src/server.c -o server

client: src/client.c include/chat.h
	$(CC) $(CFLAGS) src/client.c -o client

chat_test: tests/test_chat.c include/chat.h
	$(CC) $(CFLAGS) tests/test_chat.c -o chat_test

chat_bench: src/bench.c include/chat.h
	$(CC) $(CFLAGS) src/bench.c -o chat_bench

test: server chat_test
	./chat_test

benchmark: server chat_bench
	./chat_bench

else

build test benchmark:
	@echo "skip: networking/tcp-chat is Linux/POSIX only (see README) -- detected '$(UNAME_S)'"

endif

clean:
	rm -f server client chat_test chat_bench
