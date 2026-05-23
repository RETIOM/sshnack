CC         = gcc
CFLAGS     = -Wall -std=c23 -D_DEFAULT_SOURCE -Iinclude
TUI_CFLAGS = $(CFLAGS) -Isrc/tui/extern
SRV_TARGET     = sshnack-server
TUI_TARGET = sshnack-tui

SRV_SRCS = src/server/server.c \
       src/server/api/api.c \
       src/server/api/auth.c \
       src/server/api/router/router.c \
       src/server/api/router/radix_tree/radix_tree.c \
       src/server/api/router/handlers/products.c \
       src/server/api/router/handlers/balance.c \
       src/server/api/router/handlers/orders.c \
       src/server/api/router/handlers/admin.c \
       src/server/services/balance_service.c \
       src/server/services/products_service.c \
       src/server/services/orders_service.c \
       src/server/db/db.c \
       src/server/threadpool/threadpool.c

TUI_SRCS = $(wildcard src/tui/*.c) src/tui/extern/cJSON.c

.PHONY: all server tui debug clean

all: server tui

server:
	$(CC) $(CFLAGS) -O2 $(SRV_SRCS) -o $(SRV_TARGET) -lpthread -lsqlite3

tui:
	$(CC) $(TUI_CFLAGS) -O2 $(TUI_SRCS) -o $(TUI_TARGET) -lcurl -lncursesw

debug:
	$(CC) $(CFLAGS) -g -DDEBUG $(SRV_SRCS) -o $(SRV_TARGET) -lpthread -lsqlite3

clean:
	rm -f $(SRV_TARGET) $(TUI_TARGET)
