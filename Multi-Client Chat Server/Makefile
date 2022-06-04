DEBUG ?= 1
ifeq ($(DEBUG), 0)
    CCFLAGS=-Wall -g3 -pedantic
else
    CCFLAGS=-Wall -g3 -pedantic -D DEBUG_FLAG
endif

CC=gcc

all: client server

client: client.c proj.c
	$(CC) $(CCFLAGS) -o client client.c proj.c

server: server.c proj.c trie.c
	$(CC) $(CCFLAGS) -o server server.c proj.c trie.c

clean:
	rm -f client server
