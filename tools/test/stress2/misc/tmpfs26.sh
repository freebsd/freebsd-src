#!/bin/sh

# Bug 272678 - VFS: Incorrect data in read from concurrent write 

# Test scenario by: Kristian Nielsen <knielsen@knielsen-hq.org>

. ../default.cfg

prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
#include <stdio.h>
#include <pthread.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 0x42

const char *filename = "testfile.bin";

static FILE *write_file;
static FILE *read_file;
static pthread_mutex_t write_mutex;
static pthread_cond_t write_cond;
static pthread_mutex_t read_mutex;
static pthread_cond_t read_cond;
static pthread_mutex_t state_mutex;
static pthread_cond_t state_cond;
static int write_state;
static int read_state;

void *
writer_routine(void *arg __unused)
{
	unsigned char data[44];

	memset(data, MAGIC, sizeof(data));
	pthread_mutex_lock(&write_mutex);

	for (;;) {

		while (write_state != 1)
			pthread_cond_wait(&write_cond, &write_mutex);

		fwrite(data, 1, sizeof(data), write_file);
		fflush(write_file);

		pthread_mutex_lock(&state_mutex);
		write_state = 2;
		pthread_cond_signal(&state_cond);
		pthread_mutex_unlock(&state_mutex);
	}
}

void *
reader_routine(void *arg __unused)
{

	for (;;) {
		unsigned char buf[387];
		int len;

		while (read_state != 1)
			pthread_cond_wait(&read_cond, &read_mutex);

		len = fread(buf, 1, sizeof(buf), read_file);
		if (len < (int)sizeof(buf) && ferror(read_file)) {
			perror(" read file");
			exit(1);
		}
		for (int i = 0; i < len; ++i) {
			if (buf[i] != MAGIC) {
				fprintf(stderr, "ERROR! invalid value read 0x%2x at %d of %d, pos %ld\n",
						buf[i], i, len, ftell(read_file));
				exit(126);
			}
		}

		pthread_mutex_lock(&state_mutex);
		read_state = 2;
		pthread_cond_signal(&state_cond);
		pthread_mutex_unlock(&state_mutex);
	}
}

void
create_threads(void)
{
	pthread_t write_thread_id, read_thread_id;
	pthread_attr_t attr;

	pthread_mutex_init(&write_mutex, NULL);
	pthread_mutex_init(&read_mutex, NULL);
	pthread_mutex_init(&state_mutex, NULL);
	pthread_cond_init(&write_cond, NULL);
	pthread_cond_init(&read_cond, NULL);
	pthread_cond_init(&state_cond, NULL);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&write_thread_id, &attr, writer_routine, NULL);
	pthread_create(&read_thread_id, &attr, reader_routine, NULL);
}

int
main(int argc, char *argv[])
{
	int num_iter = 1000;
	int i;
	unsigned char buf[343];

	if (argc >= 2)
		num_iter = atoi(argv[1]);

	write_state = 0;
	read_state = 0;

	create_threads();
	memset(buf, MAGIC, sizeof(buf));

	for (i = 0; i < num_iter; ++i) {
		/* Write the first part of the file. */
		pthread_mutex_lock(&write_mutex);
		write_file = fopen(filename, "wb");
		if (!write_file) {
			perror(" open file");
			exit(1);
		}
		fwrite(buf, 1, sizeof(buf), write_file);
		fflush(write_file);

		/* Open a read handle on the file. */
		pthread_mutex_lock(&read_mutex);
		read_file = fopen(filename, "rb");
		if (!read_file) {
			perror(" open read file");
			exit(1);
		}

		write_state = 1;
		read_state = 1;
		pthread_cond_signal(&write_cond);
		pthread_mutex_unlock(&write_mutex);
		pthread_cond_signal(&read_cond);
		pthread_mutex_unlock(&read_mutex);

		pthread_mutex_lock(&state_mutex);
		while (write_state != 2 || read_state != 2)
			pthread_cond_wait(&state_cond, &state_mutex);
		pthread_mutex_unlock(&state_mutex);

		/* Close and remove the file, ready for another iteration. */
		pthread_mutex_lock(&write_mutex);
		fclose(write_file);
		write_state = 0;
		pthread_mutex_unlock(&write_mutex);

		pthread_mutex_lock(&read_mutex);
		fclose(read_file);
		read_state = 0;
		pthread_mutex_unlock(&read_mutex);

		unlink(filename);
	}

	return (0);
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O2 /tmp/$prog.c -lpthread || exit 1

mount -t tmpfs dummy $mntpoint
cd $mntpoint
/tmp/$prog; s=$?
cd -
umount $mntpoint

rm /tmp/$prog /tmp/$prog.c
exit $s
