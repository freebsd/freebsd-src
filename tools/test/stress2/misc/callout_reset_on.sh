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

# Scenario based on pr. kern/166340
# Process under FreeBSD 9.0 hangs in uninterruptable sleep with apparently
# no syscall (empty wchan).

# http://people.freebsd.org/~pho/stress/log/callout_reset_on.txt
# Fixed in r243901.

# panic: Bad link elm 0xfffff80012ba8ec8 prev->next != elm
# https://people.freebsd.org/~pho/stress/log/rrs005.txt
# Fixed in r278623.

# "ritwait DE    0- 0:00.01 crlogger: writer" seen.
# https://people.freebsd.org/~pho/stress/log/kostik917.txt
# Fixed in r302981

. ../default.cfg

rm -f /tmp/crwriter /tmp/crlogger || exit 1

cat > /tmp/crwriter.c <<EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *txt[] = {
	"0 This is a line of text: abcdefghijklmnopqrstuvwxyz",
	"1 Another line of text: ABCDEFGHIJKLMNOPQRSTUVWXYZ",
	"2 A different line of text",
	"3 A very, very different text",
	"4 A much longer line with a lot of characters in the line",
	"5 Now this is a quite long line of text, with both upper and lower case letters, and one digit!"
};

int
main(void)
{
	int i, j, n;
	char help[256];

	for (i = 0; i < 100000; i++) {
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
mycc -o /tmp/crwriter -Wall -Wextra -O2 -g /tmp/crwriter.c
rm -f /tmp/crwriter.c

cat > /tmp/crlogger.c <<EOF
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <err.h>
#include <sys/types.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#define BARRIER_CREATE 1
#define BARRIER_WAIT 2
#define BARRIER_DELETE 3

void
barrier(int mode)
{
	int fd;
	char path[128];

	if (mode == BARRIER_CREATE) {
		snprintf(path, sizeof(path), "barrier.%d", getpid());
		if ((fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600)) < 0)
			err(1, "%s", path);
	} else if (mode == BARRIER_WAIT) {
		snprintf(path, sizeof(path), "barrier.%d", getppid());
		for(;;) {
			if (access(path, R_OK) == -1)
				break;
			usleep(10000);
		}
	} else if (mode == BARRIER_DELETE) {
		snprintf(path, sizeof(path), "barrier.%d", getpid());
		if (unlink(path) == -1)
			err(1, "unlink(%s)", path);
	} else
		errx(1, "Bad barrier mode: %d", mode);
}

pid_t pid;
int bufsize;
int port;
int alarm_exit;

void
killer(void)
{
	setproctitle("killer");
	alarm(120);
	barrier(BARRIER_WAIT);
	for (;;) {
		if (pid == 0)
			break;
		if (kill(pid, SIGUSR1) == -1)
			break;
		usleep(1000);
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
		ualarm(5000, 0);
		if ((n = recvfrom(msgsock, buf, 4, 0, NULL, NULL)) < 0) {
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
	signal(SIGUSR1, handler);
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
	barrier(BARRIER_DELETE);
	alarm(0);
	while (fgets(line, sizeof(line), stdin) != NULL) {
		alarm(10);
		alarm_exit = 1;
		if (write(tcpsock, line, strlen(line)) < 0)
			err(1, "socket write(). %s:%d", __FILE__, __LINE__);
		alarm_exit = 0;
		ualarm(5000, 0);
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

	if (argc != 2)
		errx(1, "Usage: %s <port number>\n", argv[0]);
	port = atoi(argv[1]);
	bufsize = 128;

	barrier(BARRIER_CREATE);
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
mycc -o /tmp/crlogger -Wall -Wextra -O2 -g /tmp/crlogger.c
rm -f /tmp/crlogger.c

N=200
cd /tmp
start=`date '+%s'`
for i in `jot 40`; do
	for j in `jot $N`; do
		/tmp/crwriter | /tmp/crlogger 1236$j 2>/dev/null &
	done

	for j in `jot $N`; do
		wait
	done
	[ $((`date '+%s'` - start)) -gt 1200 ] && break
done
rm -f /tmp/crwriter /tmp/crlogger ./barrier.*
exit 0
