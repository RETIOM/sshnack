CC     = gcc
TARGET = sshnack

SRCS = src/server/server.c \
       src/server/api/api.c \
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

all:
	$(CC) -Wall $(SRCS) -o $(TARGET) -lpthread -lsqlite3

debug:
	$(CC) -Wall -g -DDEBUG $(SRCS) -o $(TARGET) -lpthread -lsqlite3

clean:
	rm -f $(TARGET)
