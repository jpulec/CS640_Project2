CC=gcc
CFLAGS=-Wall -std=gnu99 -ggdb3

all: requester sender emulator

requester: requester.c tracker.c utilities.c newpacket.c
	@echo "  [Building requester...]"; \
	$(CC) $(CFLAGS) -o $@ $^;          \
	echo "  [complete]"

sender: sender.c utilities.c newpacket.c
	@echo "  [Building sender...]"; \
	$(CC) $(CFLAGS) -o $@ $^;       \
	echo "  [complete]"

emulator: emulator.c utilities.c newpacket.c
	@echo "  [Building emulator...]"; \
	$(CC) $(CFLAGS) -o $@ $^;         \
	echo "  [complete]"

clean:
	@echo "Cleaning..."; \
	rm -rf *.o recvd.txt requester sender emulator

