#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Copy of callout_reset_on.sh. Waiting to see if this catches anything.

. ../default.cfg

rm -f /tmp/crwriter2 /tmp/crlogger2 || exit 1

cat > /tmp/crwriter2.c <<EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

char *txt[] = {
	"0 This is a line of text: abcdefghijklmnopqrstuvwxyz",
	"1 Another line of text: ABCDEFGHIJKLMNOPQRSTUVWXYZ",
	"2 A different line of text",
	"3 A very, very different text",
	"4 A much longer line with a lot of characters in the line",
	"5 Now this is a quite long line of text, with both upper and lower case letters, and one digit!"
};

#define RUNTIME (10 * 60)

int
main(void)
{
	time_t start;
	int j, n;
	char help[256];

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		j = arc4random() % 6;
		n = arc4random() % strlen(txt[j]);
		strncpy(help, txt[j], n);
		help[n] = 0;
		printf("%s\n", txt[j]);
		if ((arc4random() % 1000) == 1)
			usleep(100000);
	}

	return (0);
}
EOF
mycc -o /tmp/crwriter2 -Wall -Wextra -O2 -g /tmp/crwriter2.c
rm -f /tmp/crwriter2.c

cat > /tmp/crlogger2.c <<EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>

volatile u_int *share;

#define SYNC 0

pid_t pid;
int bufsize;
int port;
int alarm_exit;

void
killer(void)
{
	setproctitle("killer");
	while (share[SYNC] == 0)
		;
	alarm(120);
	for (;;) {
		if (pid == 0)
			break;
		if (kill(pid, SIGUSR1) == -1)
			break;
		usleep(arc4random() % 2000 + 10);
	}
	_exit(0);
}

void
handler(int s __unused)
{
}

void
ahandler(int s __unused)
{
	if (alarm_exit)
		_exit(0);
}

/* Read form socket, discard */
static void
reader(void) {
	int tcpsock, msgsock;
	int on;
	socklen_t len;
	struct sockaddr_in inetaddr, inetpeer;
	int n, *buf;

	setproctitle("reader - init");
	on = 1;
	if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		err(1, "socket(), %s:%d", __FILE__, __LINE__);

	if (setsockopt(tcpsock,
	    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
		err(1, "setsockopt(), %s:%d", __FILE__, __LINE__);

	inetaddr.sin_family = AF_INET;
	inetaddr.sin_addr.s_addr = INADDR_ANY;
	inetaddr.sin_port = htons(port);
	inetaddr.sin_len = sizeof(inetaddr);

	signal(SIGUSR1, handler);
	alarm(60);
	if (bind(tcpsock,
	    (struct sockaddr *)&inetaddr, sizeof (inetaddr)) < 0)
		err(1, "bind(), %s:%d", __FILE__, __LINE__);

	if (listen(tcpsock, 5) < 0)
		err(1, "listen(), %s:%d", __FILE__, __LINE__);

	len = sizeof(inetpeer);
	if ((msgsock = accept(tcpsock,
	    (struct sockaddr *)&inetpeer, &len)) < 0)
		err(1, "accept(), %s:%d", __FILE__, __LINE__);

	if ((buf = malloc(bufsize)) == NULL)
			err(1, "malloc(%d), %s:%d", bufsize, __FILE__, __LINE__);
	setproctitle("reader");
	alarm(0);
	signal(SIGALRM, ahandler);
	for (;;) {
		ualarm(arc4random() % 5000 + 100, 0);
		if ((n = recvfrom(msgsock, buf, bufsize, 0, NULL, NULL)) < 0) {
			if (errno == EAGAIN)
				continue;
			err(1, "read(), %s:%d", __FILE__, __LINE__);
		}
		if (n == 0)
			break;
		if (write(msgsock, "OK", 3) != 3)
			err(1, "write ack. %s:%d", __FILE__, __LINE__);

	}
	close(msgsock);
	_exit(0);
}

/* read from stdin, write to socket */
static void
writer(void) {
	int tcpsock, on;
	struct sockaddr_in inetaddr;
	struct hostent *hostent;
	int i, r;
	char line[1024], ack[80];;

	setproctitle("writer - init");
	share[SYNC] = 1;
	signal(SIGUSR1, handler);
	signal(SIGALRM, ahandler);
	alarm(60);
	on = 1;
	for (i = 1; i < 5; i++) {
		if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			err(1, "socket(), %s:%d", __FILE__, __LINE__);

		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			err(1, "setsockopt(), %s:%d", __FILE__, __LINE__);

		hostent = gethostbyname ("localhost");
		bzero(&inetaddr, sizeof(inetaddr));
		memcpy (&inetaddr.sin_addr.s_addr, hostent->h_addr,
			sizeof (struct in_addr));

		inetaddr.sin_family = AF_INET;
		inetaddr.sin_port = htons(port);
		inetaddr.sin_len = sizeof(inetaddr);

		r = connect(tcpsock, (struct sockaddr *) &inetaddr,
			sizeof(inetaddr));
		if (r == 0)
			break;
		sleep(1);
		close(tcpsock);
	}
	if (r < 0)
		err(1, "connect(), %s:%d", __FILE__, __LINE__);

	setproctitle("writer");
	alarm(0);
	while (fgets(line, sizeof(line), stdin) != NULL) {
		alarm(10);
		alarm_exit = 1;
		if (write(tcpsock, line, strlen(line)) < 0)
			err(1, "socket write(). %s:%d", __FILE__, __LINE__);
		alarm_exit = 0;
		ualarm(arc4random() % 5000 + 1000, 0);
		if (recvfrom(tcpsock, ack, 4, 0, NULL, NULL) < 0) {
			if (errno == EAGAIN)
				continue;
			err(1, "read(), %s:%d", __FILE__, __LINE__);
		}
	}
	sleep(30);
	return;
}

int
main(int argc, char **argv)
{

	pid_t kpid;
	size_t len;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		_exit(1);
	}
	port = atoi(argv[1]);
	bufsize = 128;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	signal(SIGCHLD, SIG_IGN);
	if ((pid = fork()) == 0)
		reader();

	if ((kpid = fork()) == 0)
		killer();

	writer();
	sleep(1);
	kill(pid, SIGINT);
	kill(kpid, SIGINT);

	return (0);
}
EOF
mycc -o /tmp/crlogger2 -Wall -Wextra -O2 -g /tmp/crlogger2.c
rm -f /tmp/crlogger2.c

N=50
cd /tmp
for j in `jot $N`; do
	/tmp/crwriter2 | /tmp/crlogger2 1236$j &
done
wait
rm -f /tmp/crwriter2 /tmp/crlogger2
exit 0
