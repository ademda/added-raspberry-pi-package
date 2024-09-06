TARGETS = client server
CC := $(TARGET_CC)

all: $(TARGETS)

client: client.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

server: server.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)