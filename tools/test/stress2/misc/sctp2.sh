#!/bin/sh

# Based on https://gist.github.com/zonque/7d03568eab14a2bb57cb by
# Daniel Mack github@zonque.org

# Modified version of sctp.sh by Michael Tuexen <tuexen@freebsd.org>:
# * Use loopback as the address of the server on both side initialized using
#   htonl(INADDR_LOOPBACK).
# * Negotiate only 1 stream in both directions since only one stream is used.
# * Don't use initmsg.sinit_max_instreams as an argument in listen(), which
#   does not make sense.
#   Use an arbitrary positive integer, 5 in this case.
# * Initialize flags before calling sctp_recvmsg().

# "panic: Don't own TCB lock" seen:
# https://people.freebsd.org/~pho/stress/log/sctp2.txt

# "panic: soclose: SS_NOFDREF on enter" seen:
# https://people.freebsd.org/~pho/stress/log/sctp2-2.txt

kldstat -v | grep -q sctp || kldload sctp.ko
cat > /tmp/sctp2.c <<EOF
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int my_port_num;

static void
die(const char *s)
{
	perror(s);
	exit(1);
}

static void
server(void)
{
	struct sctp_sndrcvinfo sndrcvinfo;
	struct sockaddr_in servaddr = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
		.sin_port = htons(my_port_num),
	};
	struct sctp_initmsg initmsg = {
		.sinit_num_ostreams = 1,
		.sinit_max_instreams = 1,
		.sinit_max_attempts = 4,
	};
	int listen_fd, conn_fd, flags, ret, in;

	listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	if (listen_fd < 0)
		die("socket");

	ret = bind(listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	if (ret < 0)
		die("bind");

	ret = setsockopt(listen_fd, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
			sizeof(initmsg));
	if (ret < 0)
		die("setsockopt");

	ret = listen(listen_fd, 5);
	if (ret < 0)
		die("listen");

	for (;;) {
		char buffer[1024];

		printf("Waiting for connection\n");
		fflush(stdout);

		conn_fd = accept(listen_fd, (struct sockaddr *) NULL, NULL);
		if(conn_fd < 0)
			die("accept()");

		printf("New client connected\n");
		fflush(stdout);

		flags = 0;
		in = sctp_recvmsg(conn_fd, buffer, sizeof(buffer), NULL, 0,
				&sndrcvinfo, &flags);
		if (in > 0) {
			printf("Received data: %s\n", buffer);
			fflush(stdout);
		}

		close(conn_fd);
	}
}

static void
client(void)
{
	struct sockaddr_in servaddr = {
		.sin_family = AF_INET,
		.sin_port = htons(my_port_num),
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};
	int conn_fd, ret;
	const char *msg = "Hello, Server!";

	conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	if (conn_fd < 0)
		die("socket()");

	ret = connect(conn_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	if (ret < 0)
		die("connect()");

	ret = sctp_sendmsg(conn_fd, (void *) msg, strlen(msg) + 1, NULL, 0, 0, 0, 0, 0, 0 );
	if (ret < 0)
		die("sctp_sendmsg");

	close(conn_fd);
}

int
main(int argc __unused, char *argv[])
{

	my_port_num = atoi(argv[1]);
	if (strstr(basename(argv[0]), "server"))
		server();
	else
		client();

	return (0);
}
EOF

cc -o /tmp/server -Wall -Wextra -O2 /tmp/sctp2.c || exit
ln -sf /tmp/server /tmp/client

parallel=100
for i in `jot $parallel 62324`; do
	/tmp/server $i > /dev/null &
done
(cd ../testcases/swap; ./swap -t 1m -i 20 -l 100) &
sleep 2

start=`date +%s`
while [ $((`date +%s` - start)) -lt 60 ]; do
	pids=
	for i in `jot 50`; do
		for j in `jot $parallel 62324`; do
			/tmp/client $j &
			pids="$pids $!"
		done
	done
	for i in $pids; do
		wait $i
	done
done
pkill server
wait
while pkill swap; do :; done
wait
rm -f /tmp/sctp2.c /tmp/server /tmp/client
exit 0
