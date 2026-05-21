CC     = gcc
TARGET = sshnack

SRCS = src/server/server.c \
       src/server/api/api.c \
       src/server/db/db.c \
       src/server/threadpool/threadpool.c

all:
	$(CC) -Wall $(SRCS) -o $(TARGET) -lpthread -lsqlite3

debug:
	$(CC) -Wall -g -DDEBUG $(SRCS) -o $(TARGET) -lpthread -lsqlite3

clean:
	rm -f $(TARGET)
