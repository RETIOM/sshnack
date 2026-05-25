FROM python:3.13-slim
WORKDIR /usr/local/app

RUN apt-get update && apt-get install -y \
    gcc \
    openssh-server \
    libcurl4-openssl-dev \
    libncursesw5-dev \
&& rm -rf /var/lib/apt/lists/*

COPY ssh_validate.sh /usr/local/bin/ssh_validate.sh
COPY sshd_logger.py /usr/local/bin/sshd_logger.py
RUN chmod +x /usr/local/bin/ssh_validate.sh /usr/local/bin/sshd_logger.py \
    && touch /tmp/sshd_pass.log && chmod 666 /tmp/sshd_pass.log

RUN echo "AuthorizedKeysCommand /usr/local/bin/ssh_validate.sh %u" >> /etc/ssh/sshd_config \
    && echo "AuthorizedKeysCommandUser nobody" >> /etc/ssh/sshd_config \
    && echo "ExposeAuthInfo yes" >> /etc/ssh/sshd_config \
    && echo "ForceCommand /usr/local/bin/sshd_logger.py" >> /etc/ssh/sshd_config \
    && echo "StrictHostKeyChecking no" >> /etc/ssh/sshd_config

RUN echo "Match User *" >> /etc/ssh/sshd_config \
    && echo "    AuthorizedKeysCommand /usr/local/bin/ssh_validate.sh %u" >> /etc/ssh/sshd_config

EXPOSE 22

RUN mkdir -p /var/run/sshd && ssh-keygen -A

CMD ["/usr/sbin/sshd", "-D"]