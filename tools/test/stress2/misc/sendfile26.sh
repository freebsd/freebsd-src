#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Jean-Sébastien Pédron <dumbbell@FreeBSD.org>
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

# "Written file doesn't match memory buffer" seen on main-n244961-e6bb49f12ca
# https://reviews.freebsd.org/D28811

. ../default.cfg
kldstat -v | grep -q zfs.ko  || { kldload zfs.ko ||
    exit 0; loaded=1; }

cat > /tmp/write_vs_sendfile.c <<EOF
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netinet/in.h>

#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define FILENAME "myfile.bin"
#define MAX_SIZE 50 * 1000 * 1000

int tcp_port;
int chunk_size;

static void *
sender_start(void *buffer __unused)
{
	int fd, sock, ret;
	struct sockaddr_in sa = { 0 };
	off_t cursor;

	printf("Sender: opening connection to TCP port %d\n", tcp_port);
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Sender: failed to create socket");
		return (NULL);
	}

	sa.sin_family = AF_INET;
	sa.sin_port = htons(tcp_port);
	sa.sin_addr.s_addr = htonl((((((127 << 8) | 0) << 8) | 0) << 8) | 1);

	ret = connect(sock, (struct sockaddr *)&sa, sizeof(sa));
	if (ret < 0) {
		perror("Sender: failed to connect to localhost");
		close(sock);
		return (NULL);
	}

	printf("Sender: opening %s\n", FILENAME);
	fd = open(FILENAME, O_CREAT|O_RDONLY, 0644);
	if (fd < 0) {
		perror("Sender: failed to open file");
		close(sock);
		return (NULL);
	}

	printf("Sender: starting sendfile(2) loop\n");
	cursor = 0;
	do {
		size_t to_send = chunk_size;
		off_t sbytes = 0;

		do {
#if defined(__FreeBSD__)
			ret = sendfile(fd, sock, cursor, to_send,
			    NULL, &sbytes, 0);
			if (ret == 0) {
				cursor += sbytes;
				to_send -= sbytes;
			}
#elif defined(__APPLE__)
			sbytes = to_send;
			ret = sendfile(fd, sock, cursor, &sbytes, NULL, 0);
			if (ret < 0 && (errno == EAGAIN || errno == EINTR)) {
				ret = 0;
			}
			if (ret == 0) {
				ret = 0;
				cursor += sbytes;
				to_send -= sbytes;
			}
#else
#error Not implemented
#endif
		} while (ret == 0 && to_send > 0);
	} while (cursor < MAX_SIZE);

	printf("Sender: closing socket\n");
	close(fd);
	printf("Sender: closing %s\n", FILENAME);
	close(sock);

	return (NULL);
}

static void *
writer_start(void *buffer)
{
	int fd, cursor, ret;

	printf("Writer: opening %s\n", FILENAME);
	fd = open(FILENAME, O_CREAT|O_RDWR|O_TRUNC|O_DIRECT, 0644);
	if (fd < 0) {
		perror("Writer: failed to open file");
		return (NULL);
	}

	/* We sleep one second to give a head start to the sendfile(2) thread
	 * above. */
	sleep(1);

	printf(
	    "Writer: writing chunks of %u bytes to a max of %u bytes\n",
	    chunk_size, MAX_SIZE);
	cursor = 0;
	do {
		ret = write(fd, buffer, chunk_size);
		if (ret < 0) {
			perror("Writer: failed to write file");
			break;
		}
		assert(ret == chunk_size);

		cursor += ret;
	} while (cursor < MAX_SIZE);

	printf("Writer: closing %s\n", FILENAME);
	close(fd);

	return (NULL);
}

int
check_file(void *buffer, int flags)
{
	int fd, ret, cursor;
	void *read_buffer;

	printf("Writer: opening %s\n", FILENAME);
	fd = open(FILENAME, O_RDONLY | flags | O_DIRECT);
	if (fd < 0) {
		perror("Checker: failed to open file");
		return (1);
	}

	read_buffer = malloc(chunk_size);
	if (buffer == NULL) {
		perror("Checker: failed to allocate buffer");
		close(fd);
		return (1);
	}

	cursor = 0;
	do {
		ret = read(fd, read_buffer, chunk_size);
		if (ret < 0) {
			perror("Checker: failed to read file");
			close(fd);
			free(read_buffer);
			return (1);
		}
		assert(ret == chunk_size);

		cursor += ret;

		ret = memcmp(buffer, read_buffer, chunk_size);
	} while (ret == 0 && cursor < MAX_SIZE);

	return (ret);
}

int
main(int argc, char *argv[])
{
	int ret;
	void *buffer;
	pthread_t sender;
	pthread_t writer;

	/* The sender thread will connect to the TCP port on the local host.
	 * The user is responsible for starting an instance of netcat like
	 * this:
	 *
	 *     nc -k -l 8080
	 */
	tcp_port = argc >= 2 ? atoi(argv[1]) : 8080;
	chunk_size = argc >= 3 ? atoi(argv[2]) : 32128;

	/* We initialize a buffer and fill it with 0xff bytes. The buffer is
	 * written many times to a file by the writer thread (see below). */
	buffer = malloc(chunk_size);
	if (buffer == NULL) {
		perror("Main: failed to allocate buffer");
		return (1);
	}

	memset(buffer, 255, chunk_size);

	unlink(FILENAME);

	/* The sender thread is responsible for sending the file written by the
	 * writer thread to a local TCP port. The goal is always try to send
	 * data which is not written to the file yet. */
	ret = pthread_create(&sender, NULL, &sender_start, buffer);
	if (ret != 0) {
		free(buffer);
		return (2);
	}

	/* The writer thread is responsible for writing the allocated buffer to
	 * the file until it reaches a size of 50 MB. */
	ret = pthread_create(&writer, NULL, &writer_start, buffer);
	if (ret != 0) {
		pthread_cancel(sender);
		pthread_join(sender, NULL);
		free(buffer);
		return (2);
	}

	pthread_join(writer, NULL);
	pthread_cancel(sender);
	pthread_join(sender, NULL);

	/* Now that both threads terminated, we check the content of the
	 * written file. The bug on ZFS on FreeBSD is that some portions of the
	 * file contains zeros instead of the expected 0xff bytes. */
	ret = check_file(buffer, 0);
	free(buffer);

	if (ret != 0) {
		fprintf(stderr, "\033[1;31mWritten file doesn't match memory buffer\033[0m\n");
	}

	return (ret);
}
EOF

mycc -o /tmp/write_vs_sendfile -Wall -Wextra -O2 /tmp/write_vs_sendfile.c -lpthread || exit 1
rm /tmp/write_vs_sendfile.c

u1=$mdstart
u2=$((u1 + 1))

mdconfig -l | grep -q md$u1 && mdconfig -d -u $u1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $u2

mdconfig -s 2g -u $u1
mdconfig -s 2g -u $u2

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank raidz md$u1 md$u2
zfs create stress2_tank/test

here=`pwd`
cd /stress2_tank/test
nc -k -l 8080 >/dev/null &
sleep .5
/tmp/write_vs_sendfile; s=$?
kill $!
wait
cd $here

zfs umount stress2_tank/test
zfs destroy -r stress2_tank
zpool destroy stress2_tank

mdconfig -d -u $u1
mdconfig -d -u $u2
[ -n "$loaded" ] && kldunload zfs.ko
rm /tmp/write_vs_sendfile
exit $s
