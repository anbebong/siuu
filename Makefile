CC=gcc
CFLAGS=-Wall -O2

all: sse_server test_send_cli

sse_server: main.o sse_server.o
	$(CC) $(CFLAGS) -o sse_server main.o sse_server.o

test_send_cli: test_send_cli.o sse_server.o
	$(CC) $(CFLAGS) -o test_send_cli test_send_cli.o sse_server.o

main.o: main.c sse_server.h
	$(CC) $(CFLAGS) -c main.c

test_send_cli.o: test_send_cli.c sse_server.h
	$(CC) $(CFLAGS) -c test_send_cli.c

sse_server.o: sse_server.c sse_server.h
	$(CC) $(CFLAGS) -c sse_server.c

clean:
	rm -f *.o sse_server test_send_cli
