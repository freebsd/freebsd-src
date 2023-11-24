#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# Test scenario by Nate Eldredge neldredge math ucsdnedu
# kern/127213: [tmpfs] sendfile on tmpfs data corruption
# Variation of tmpfs7.sh where UFS is used instead of tmpfs.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

set -e

odir=`pwd`
cd /tmp
cat > sendfile6_server.c <<EOF
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "util.h"

int main(int argc, char *argv[]) {
	int f, listener, connection;
	if (argc < 3) {
		fprintf(stderr, "Usage: %s filename socketname\n", argv[0]);
		exit(1);
	}
	if ((f = open(argv[1], O_RDONLY)) < 0) {
		perror(argv[1]);
		exit(1);
	}
	if ((listener = listen_unix_socket(argv[2])) < 0) {
		exit(1);
	}
	if ((connection = accept_unix_socket(listener)) >= 0) {
		real_sendfile(f, connection);
	}
	return 0;
}
EOF
cat > sendfile6_client.c <<EOF
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "util.h"

int main(int argc, char *argv[]) {
	int s;
	if (argc < 2) {
		fprintf(stderr, "Usage: %s socketname\n", argv[0]);
		exit(1);
	}
	if ((s = connect_unix_socket(argv[1])) < 0) {
		exit(1);
	}
	fake_sendfile(s, 1);
	return 0;
}
EOF

cat > util.c <<EOF
/* send data from file to unix domain socket */

#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int create_unix_socket(void) {
	int fd;
	if ((fd = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return -1;
	}
	return fd;
}

int make_unix_sockaddr(const char *pathname, struct sockaddr_un *sa) {
	memset(sa, 0, sizeof(*sa));
	sa->sun_family = PF_LOCAL;
	if (strlen(pathname) + 1 > sizeof(sa->sun_path)) {
//		fprintf(stderr, "%s: pathname too long (max %lu)\n",
//			pathname, sizeof(sa->sun_path));
		errno = ENAMETOOLONG;
		return -1;
	}
	strcpy(sa->sun_path, pathname);
	return 0;
}

static char *sockname;
void delete_socket(void) {
	unlink(sockname);
}

int listen_unix_socket(const char *path) {
	int fd;
	struct sockaddr_un sa;
	if (make_unix_sockaddr(path, &sa) < 0)
		return -1;
	if ((fd = create_unix_socket()) < 0)
		return -1;
	if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind");
		close(fd);
		return -1;
	}
	sockname = strdup(path);
	atexit(delete_socket);

	if (listen(fd, 5) < 0) {
		perror("listen");
		close(fd);
		return -1;
	}
	return fd;
}

int accept_unix_socket(int fd) {
	int s;
	if ((s = accept(fd, NULL, 0)) < 0) {
		perror("accept");
		return -1;
	}
	return s;
}

int connect_unix_socket(const char *path) {
	int fd;
	struct sockaddr_un sa;
	if (make_unix_sockaddr(path, &sa) < 0)
		return -1;
	if ((fd = create_unix_socket()) < 0)
		return -1;
	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("connect");
		return -1;
	}
	return fd;
}

#define BUFSIZE 65536

int fake_sendfile(int from, int to) {
	char buf[BUFSIZE];
	int v;
	int sent = 0;
	while ((v = read(from, buf, BUFSIZE)) > 0) {
		int d = 0;
		while (d < v) {
			int w = write(to, buf, v - d);
			if (w <= 0) {
				perror("write");
				return -1;
			}
			d += w;
			sent += w;
		}
	}
	if (v != 0) {
		perror("read");
		return -1;
	}
	return sent;
}

int real_sendfile(int from, int to) {
	int v;
	v = sendfile(from, to, 0, 0, NULL, NULL, 0);
	if (v < 0) {
		perror("sendfile");
	}
	return v;
}
EOF

cat > util.h <<EOF
/* send data from file to unix domain socket */

#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

int create_unix_socket(void);
int make_unix_sockaddr(const char *pathname, struct sockaddr_un *sa);
int listen_unix_socket(const char *path);
int accept_unix_socket(int fd);
int connect_unix_socket(const char *path);
int fake_sendfile(int from, int to);
int real_sendfile(int from, int to);
EOF

mycc -c -Wall -Wextra -O2 util.c
mycc -o sendfile6_server -Wall -Wextra -O2 sendfile6_server.c util.o
mycc -o sendfile6_client -Wall -Wextra -O2 sendfile6_client.c util.o
rm -f sendfile6_server.c sendfile6_client.c util.c util.o util.h mysocket

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

dd if=/dev/random of=$mntpoint/data bs=123456 count=1 status=none
./sendfile6_server $mntpoint/data mysocket &
sleep 0.2
./sendfile6_client mysocket > data.$$
wait
cmp $mntpoint/data data.$$ ||
	{ echo "FAIL Data mismatch"; ls -l $mntpoint/data data.$$; }
rm -f data.$$ sendfile6_server sendfile6_client mysocket

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
