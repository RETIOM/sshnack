ROOT  ?= ..
CC     = gcc
CFLAGS = -Wall -Iinclude
LDLIBS = -lpthread -lsqlite3
BIN    = $(ROOT)/bin/server
SRCS   = $(shell find src -name '*.c')

.PHONY: all debug clean

all:
	@mkdir -p $(ROOT)/bin
	$(CC) $(CFLAGS) -O2 $(SRCS) -o $(BIN) $(LDLIBS)

debug:
	@mkdir -p $(ROOT)/bin
	$(CC) $(CFLAGS) -g -DDEBUG $(SRCS) -o $(BIN) $(LDLIBS)

clean:
	rm -f $(BIN)
