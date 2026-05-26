FROM gcc:15.2.0 AS builder


RUN apt-get update && apt-get install -y \
    libsqlite3-dev \
&& rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY server/Makefile ./server/
COPY server/include ./server/include
COPY server/src ./server/src

RUN make -C server

FROM debian:trixie-slim

RUN apt-get update && apt-get install -y \
    sqlite3 \
&& rm -rf /var/lib/apt/lists/*

WORKDIR /usr/local/app

COPY --from=builder /app/bin/server /usr/local/app/
RUN chmod +x ./server
COPY init.sql ./

COPY server/entrypoint.sh ./
RUN chmod +x entrypoint.sh

VOLUME /data
EXPOSE 8080

ENTRYPOINT ["./entrypoint.sh"]
CMD ["./server"]
