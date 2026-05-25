ROOT  ?= ..
CC     = gcc
CFLAGS = -Wall -Iinclude -I$(ROOT)/extern/cJSON
LDLIBS = -lcurl -lncursesw
BIN    = $(ROOT)/bin/tui
SRCS   = $(shell find src -name '*.c') $(ROOT)/extern/cJSON/cJSON.c

.PHONY: all debug clean

all:
	@mkdir -p $(ROOT)/bin
	$(CC) $(CFLAGS) -O2 $(SRCS) -o $(BIN) $(LDLIBS)

debug:
	@mkdir -p $(ROOT)/bin
	$(CC) $(CFLAGS) -g -DDEBUG $(SRCS) -o $(BIN) $(LDLIBS)

clean:
	rm -f $(BIN)
