#!/bin/sh

#
# Copyright (c) 2026 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# sendfile over tmpfs

# "panic: vm_page_assert_busied: page 0xfffffe000015fb88 not busy @ ../../../vm/vm_page.c:5845" seen
# Triggered by: 72ddb6de1028 - main - unix: increase net.local.(stream|seqpacket).(recv|send)space to 64 KiB
# Fixed by: d198ad51ea73 - main - swap_pager_getpages(): some pages from ma[] might be bogus

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `sysctl -n vm.swap_total` -eq 0 ] && exit 0

. ../default.cfg

prog=$(basename "$0" .sh)

cat > /tmp/$prog.c <<EOF
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int use_sendfile = 1;

int
main(int argc, char *argv[])
{
	off_t rsize, wsize, pos;
	ssize_t n;
	struct stat st;
	int from, pair[2], pid, status;
	const char *from_name;
	char *buf, *cp;

	if (argc != 2)
		errx(1, "Usage: %s from", argv[0]);
	from_name = argv[1];

	if ((from = open(from_name, O_RDONLY)) == -1)
		err(1, "open read %s", from_name);

	if (fstat(from, &st) == -1)
		err(1, "stat %s", from_name);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1)
		err(1, "socketpair");

	pid = fork();
	if (pid == -1)
		err(1, "fork");
	if (pid == 0) {	/* child */
		setproctitle("reader");
		close(pair[0]);
		buf = malloc(st.st_size);
		if (buf == NULL)
			err(1, "malloc %jd", st.st_size);
		pos = 0;
		sleep(1);
		for (;;) {
			rsize = 413; /* arbitrary small block size */
			if (rsize > st.st_size - pos)
				rsize = st.st_size - pos;
			n = read(pair[1], buf + pos, rsize);
			if (n == -1)
				err(1, "read()");
			else if (n == 0)
				errx(1, "Short read: Read %jd bytes out of %jd\n",
					(intmax_t)pos, (intmax_t)st.st_size);
			pos += n;
			if (pos == st.st_size)
				break;
		}
		close(pair[1]);
		_exit(0);
	}
	setproctitle("writer");
	close(pair[1]);

	if (use_sendfile == 1) {
		pos = 0;
		for (;;) {
			n = sendfile(from, pair[0], pos, st.st_size - pos,
			    NULL, &wsize, 0);
			if (n == -1) {
				if (errno != EAGAIN)
					err(1, "sendfile()");
			}
			if (wsize != st.st_size)
				fprintf(stderr, "sendfile() wrote %jd bytes\n", (intmax_t)wsize);
			pos += wsize;
			if (pos == st.st_size)
				break;
		}
	} else {
		fprintf(stderr, "Not using sendfile().\n");
		if ((cp = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, from, 0)) == MAP_FAILED)
			err(1, "mmap");

		if ((n = write(pair[0], cp, st.st_size)) == -1)
			err(1, "write()");
		if (n != st.st_size)
			errx(1, "short write: %jd of %jd\n", (intmax_t)n, (intmax_t)st.st_size);
		if (munmap(cp, st.st_size) == -1)
			err(1, "munmap()");
	}
	if (waitpid(pid, &status, 0) != pid)
		err(1, "waitpid()");
	close(pair[0]);

	return (status != 0);
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c || exit 1

mount | grep -q "on $mntpoint " && umount -f $mntpoint
mount -t tmpfs dummy $mntpoint || exit 1
dd if=/dev/zero of=$mntpoint/file bs=1m count=1024 status=none # 1Gb

../testcases/swap/swap -t 5m -i 40 > /dev/null 2>&1 &
sleep 5
/tmp/$prog $mntpoint/file
while pkill swap; do :; done
wait

umount $mntpoint
rm -f /tmp/$prog /tmp/$prog.c $diskimage
exit 0
