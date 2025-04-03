#!/bin/sh

# Original test scenario by nabijaczleweli@nabijaczleweli.xyz:
# Bug 283101 - pthread_cancel() doesn't cancel a thread that's currently in pause()
# Fixed by: 9f78c837d94f check_cancel: when in_sigsuspend, send SIGCANCEL unconditionally

. ../default.cfg
set -u
prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void *
thread(void *arg __unused)
{
	for(;;) {
		pause();
		printf("woke up from pause\n");
	}
}

static void
thread_cancel_and_join(pthread_t ptid)
{
	void *status = NULL;

	if (pthread_cancel(ptid)) {
		printf("pthread_cancel() failed\n");
		exit(1);
	}

	(void) pthread_join(ptid, &status);
	int error = (int)(uintptr_t)status;

	if (error) {
		if (status == PTHREAD_CANCELED) {
			printf("pthread_cancel() succeeded\n");
		} else {
			printf("pthread_join() error (not PTHREAD_CANCELED)\n");
			exit(1);
		}
	}
}

int
main(void)
{
	// Empirically, I've noticed that either the hang occurs somewhere between
	// 10 and 500 iterations, or it runs infinitely without ever hanging.
	// Therefore, stopping at 500th iteration, and looping from a shell script.

	// For quick results (usually under 10 minutes), invoke "./run" from a dozen
	// consoles or GNU screen windows in parallel.

	pid_t pid = getpid();

	for (uint64_t iteration = 1; iteration <= 500; ++iteration) {
		printf("PID %d, iteration %lu...", pid, iteration);

		pthread_t ptid;
		int err;

		err = pthread_create(&ptid, NULL, thread, NULL);

		if (err) {
			printf("pthread_create() failed with error: %d\n", err);
			return 1;
		}

		thread_cancel_and_join(ptid);

		printf("OK\n");

		// Tiny sleep
		usleep(20000);
	}
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O2 /tmp/$prog.c -lpthread || exit 1
(cd ../testcases/swap; ./swap -t 3m -i 20 > /dev/null) &
sleep 5
start=`date +%s`
while [ $((`date +%s` - start)) -lt 180 ]; do
	/tmp/$prog > /dev/null & pid=$!
	t1=`date +%s`
	while kill -0 $pid 2> /dev/null; do
		if [ $((`date +%s` - t1)) -gt 180 ]; then
			ps -lH $pid
#			exit 1 # For DEBUG
			kill -9 $pid; s=1
			echo fail
			break 2
		else
			sleep 1
		fi
	done
	wait $pid; s=$?
	[ $s -ne 0 ] && break
done
while pkill swap; do :; done
wait
rm -f /tmp/$prog /tmp/$prog.c
exit $s
