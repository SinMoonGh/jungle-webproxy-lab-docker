# Makefile for echo_client and echo_server

CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lpthread

# 디렉토리 및 파일
SRCDIR = .
TARGETS = echo_client echo_server

# all: server client

# make client
client: clean echo_client run_client

clean:
	rm -f *.o
	rm -f ./echo_client
	rm -f ./echo_server

echo_client: $(SRCDIR)/echo_client.c $(SRCDIR)/csapp.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run_client:
	./echo_client localhost 12345

# make server
server: clean echo_server run_server

echo_server: $(SRCDIR)/echo_server.c $(SRCDIR)/csapp.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run_server:
	./echo_server 12345

# all: $(TARGETS)

# echo_client: $(SRCDIR)/echo_client.c $(SRCDIR)/csapp.c
# 	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# echo_server: $(SRCDIR)/echo_server.c $(SRCDIR)/csapp.c
# 	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# test_client: $(SRCDIR)/test_client.c $(SRCDIR)/csapp.c
# 	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# clean:
# 	rm -f $(TARGETS)