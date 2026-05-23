CC     = gcc
CFLAGS = -Wall -Iinclude

SRV_SRCS = src/server/server.c \
           src/server/api/api.c \
           src/server/api/auth.c \
           src/server/api/router/router.c \
           src/server/api/router/radix_tree/radix_tree.c \
           src/server/api/router/handlers/products.c \
           src/server/api/router/handlers/balance.c \
           src/server/api/router/handlers/orders.c \
           src/server/api/router/handlers/admin.c \
           src/server/api/router/handlers/debug.c \
           src/server/services/balance_service.c \
           src/server/services/products_service.c \
           src/server/services/orders_service.c \
           src/server/db/db.c \
           src/server/threadpool/threadpool.c

TUI_SRCS = $(wildcard src/tui/*.c) src/tui/extern/cJSON.c

.PHONY: all server tui debug_server debug_tui clean

all: server tui

server:
	$(CC) $(CFLAGS) -O2 $(SRV_SRCS) -o server -lpthread -lsqlite3

tui:
	$(CC) $(CFLAGS) -Isrc/tui/extern -O2 $(TUI_SRCS) -o tui -lcurl -lncursesw

debug_server:
	$(CC) $(CFLAGS) -g -DDEBUG $(SRV_SRCS) -o server -lpthread -lsqlite3

debug_tui:
	$(CC) $(CFLAGS) -Isrc/tui/extern -g -DDEBUG $(TUI_SRCS) -o tui -lcurl -lncursesw

clean:
	rm -f server tui
