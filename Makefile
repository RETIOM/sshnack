export ROOT := $(CURDIR)

.PHONY: all server tui debug clean

all: server tui

server tui:
	$(MAKE) -C $@

debug:
	$(MAKE) -C server debug
	$(MAKE) -C tui    debug

clean:
	$(MAKE) -C server clean
	$(MAKE) -C tui    clean
	rm -rf bin
