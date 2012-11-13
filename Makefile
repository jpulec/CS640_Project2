CFLAGS=-g -Wall
LDFLAGS=
CC=gcc
SHARED_SOURCES=utilities.c packet.c tracker.c
EXECUTABLES=sender requester emulator
.PHONY=all
.DEFAULT=all

all: $(EXECUTABLES)

$(EXECUTABLES): $(SHARED_SOURCES)
	$(CC) $(CFLAGS) $@.c $^ -o $@

clean:
	rm -rf *.o $(EXECUTABLES)
