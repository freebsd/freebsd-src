#!/bin/sh

# sigaltstack(2) regression test by Steven Hartland <killing@multiplay.co.uk>
# Wrong altsigstack clearing on exec
# https://github.com/golang/go/issues/15658#issuecomment-287276856

# Fixed by r315453

cd /tmp
cat > test-sigs.c <<EOF
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

extern char **environ;

static void
die(const char *s)
{
       perror(s);
       exit(EXIT_FAILURE);
}

static void
setstack(void *arg __unused)
{
	stack_t ss;

	ss.ss_sp = malloc(SIGSTKSZ);
	if (ss.ss_sp == NULL)
		die("malloc");

	ss.ss_size = SIGSTKSZ;
	ss.ss_flags = 0;
	if (sigaltstack(&ss, NULL) < 0)
		die("sigaltstack set");
}

static void *
thread_exec(void *arg)
{
	struct timespec ts = {0, 1000};
	char *argv[] = {"./test-sigs", "no-more", 0};

	setstack(arg);
	nanosleep(&ts, NULL);

	execve(argv[0], &argv[0], environ);
	die("exec failed");

	return NULL;
}

static void *
thread_sleep(void *arg __unused)
{
	sleep(10);

	return NULL;
}

int
main(int argc, char** argv __unused)
{
	int j;
	pthread_t tid1, tid2;

	if (argc != 1) {
		stack_t ss;

		if (sigaltstack(NULL, &ss) < 0)
			die("sigaltstack get");

		if (ss.ss_sp != NULL || ss.ss_flags != SS_DISABLE ||
		    ss.ss_size != 0) {
			fprintf(stderr, "invalid signal stack after execve: "
			    "ss_sp=%p ss_size=%lu ss_flags=0x%x\n", ss.ss_sp,
			    (unsigned long)ss.ss_size,
			    (unsigned int)ss.ss_flags);
			return 1;
		}

		printf("valid signal stack is valid after execve\n");

		return 0;
	}

	// We have to use two threads to ensure that can detect the
	// issue when new threads are added to the head (pre 269095)
	// and the tail of the process thread list.
	j = pthread_create(&tid1, NULL, thread_exec, NULL);
	if (j != 0) {
	       errno = j;
	       die("pthread_create");
	}

	j = pthread_create(&tid2, NULL, thread_sleep, NULL);
	if (j != 0) {
	       errno = j;
	       die("pthread_create");
	}

	j = pthread_join(tid1, NULL);
	if (j != 0) {
		errno = j;
		die("pthread_join");
	}

	j = pthread_join(tid2, NULL);
	if (j != 0) {
		errno = j;
	}

	return 0;
}
EOF

cc -o test-sigs -Wall -Wextra -O2 -g test-sigs.c -lpthread || exit 1
./test-sigs
s=$?

rm -f test-sigs test-sigs.c
exit $s
