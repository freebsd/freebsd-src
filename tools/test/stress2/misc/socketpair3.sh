#!/bin/sh

# Bug 227285 - File descriptor passing does not work reliably on SMP system
# (cache coherency issue?)
# "socketpair3: read failed in parent: 0, so_error: No error: 0 (0), ret2: 0"
# seen.

# Original test scenario by: jan.kokemueller@gmail.com

# Page fault seen in WiP socket code.

. ../default.cfg
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/socketpair3.c
mycc -o socketpair3 -Wall -Wextra -O0 -g socketpair3.c -lnv || exit 1
rm -f socketpair3.c
cd $odir

for i in `jot 6`; do
	$dir/socketpair3 &
	pids="$pids $!"
done
s=0
for i in $pids; do
	wait $i
	[ $? -ne 0 ] && s=1
done
[ -f socketpair3.core -a $s -eq 0 ] &&
    { ls -l socketpair3.core; mv socketpair3.core /tmp; s=1; }

rm -rf $dir/socketpair3
exit $s

EOF
#include <sys/types.h>

#include <sys/procdesc.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <err.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// From libnv.
extern int fd_send(int sock, const int *fds, size_t nfds);
extern int fd_recv(int sock, int *fds, size_t nfds);

int main(void)
{
	pid_t pid;
	time_t start;
	int child_fd;
	int sock[2];

	start = time(NULL);
	while (time(NULL) - start < 60) {
		if (socketpair(PF_UNIX, SOCK_STREAM, 0, sock) < 0)
			err(1, "socketpair");

		pid = pdfork(&child_fd, PD_CLOEXEC);
		if (pid < 0)
			err(1, "pdfork");

		if (pid == 0) {
			ssize_t ret;
			int sock_child[2];
			int dummy = 0;

			close(sock[0]);
			if (socketpair(PF_UNIX, SOCK_STREAM, /**/
			    0, sock_child) < 0)
				err(1, "socketpair");

			if (fd_send(sock[1], &sock_child[0], 1) != 0)
				errx(1, "fd_send failed");
#ifdef WORKAROUND
			if (read(sock[1], &dummy, 1) != 1)
				err(1, "write");
#endif

			close(sock_child[0]);

			if (write(sock_child[1], &dummy, 1) != 1)
				err(1, "write");

			if ((ret = read(sock_child[1], &dummy, 1)) != 1)
				errx(1, "read failed in child: %d",
				    (int)ret);

			close(sock_child[1]);

			_exit(0);
		}

		close(sock[1]);

		int sock_child;
		uint8_t dummy;

		if (fd_recv(sock[0], &sock_child, 1) != 0)
			errx(1, "fd_recv failed");
#ifdef WORKAROUND
		if (write(sock[0], &dummy, 1) != 1)
			err(1, "write");
#endif

		ssize_t ret;
		if ((ret = read(sock_child, &dummy, 1)) != 1) {
			int error;
			socklen_t err_len = sizeof(error);

			if (getsockopt(sock_child, SOL_SOCKET, SO_ERROR,
			    &error, &err_len) < 0)
				err(1, "getsockopt");

			ssize_t ret2 = read(sock_child, &dummy, 1);

			errx(1,
			    "read failed in parent: %d, so_error: %s (%d), "
			    "ret2: %d", (int)ret, strerror(error), error,
			    (int)ret2);
		}

		if (write(sock_child, &dummy, 1) != 1)
			err(1, "write");

		close(sock_child);

		struct pollfd pfd = { .fd = child_fd };
		poll(&pfd, 1, -1);

		close(child_fd);
		close(sock[0]);
	}

	return (0);
}
