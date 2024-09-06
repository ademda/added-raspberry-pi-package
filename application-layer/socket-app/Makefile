TARGETS = client server
CC = gcc

all: $(TARGETS)

client: client.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

server: server.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)