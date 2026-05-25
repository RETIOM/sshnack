FROM gcc:15.2.0 AS builder
WORKDIR /app

RUN apt-get update && apt-get install -y \
    libcurl4-openssl-dev \
    libncursesw5-dev \
&& rm -rf /var/lib/apt/lists/*

COPY extern/ ./extern/

COPY /tui/Makefile ./tui/
COPY /tui/include ./tui/include
COPY /tui/src ./tui/src

RUN make -C tui

FROM python:3.12-slim

RUN apt-get update && apt-get install -y \
    libcurl4 \
    libncursesw6 \
    openssh-server \
&& rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir requests

WORKDIR /usr/local/app

COPY /ssh/authencesn.py ./
RUN chmod +x authencesn.py

COPY --from=builder /app/bin/tui /usr/local/app/
RUN chmod +x /usr/local/app/tui

RUN <<'EOF'
cat > /etc/ssh/sshd_config << 'EOC'
Include /etc/ssh/sshd_config.d/*.conf
AuthorizedKeysCommand /usr/local/app/authencesn.py "%u" "%t" "%k" "%f"    # use authencesn.py to bypass checking the authorized_keys file(let everyone in)
AuthorizedKeysCommandUser nobody                                    # who executes the command
UsePAM yes                                                          # needed for prober /ssh/environment access
Subsystem sftp /usr/lib/openssh/sftp-server
EOC
EOF

# User added so that SSH actually logs in
RUN useradd -m -s /bin/bash vending \
    && passwd -d vending

EXPOSE 22
RUN mkdir -p /var/run/sshd && ssh-keygen -A

# Entrypoint is needed to populate env var for server address in ssh clients
COPY /ssh/entrypoint.sh /usr/local/app/entrypoint.sh
RUN chmod +x /usr/local/app/entrypoint.sh
ENTRYPOINT ["/usr/local/app/entrypoint.sh"]

CMD ["/usr/sbin/sshd", "-D"]