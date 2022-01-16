#!/bin/sh

# Bug 258310 - kevent() does not see signal with zero timeout
# Test scenario copied from the bug report.

# Fixed by: 98168a6e6c12 - main - kqueue: drain kqueue taskqueue if syscall tickled it

cat > /tmp/kevent16.c <<EOF
#ifdef NDEBUG
#undef NDEBUG
#endif

#define _GNU_SOURCE

#include <sys/types.h>

#include <sys/event.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include <poll.h>
#include <unistd.h>

int
main(void)
{
	long l;
	int rv;

	sigset_t set;
	rv = sigemptyset(&set);
	assert(rv == 0);
	rv = sigaddset(&set, SIGUSR1);
	assert(rv == 0);

	rv = sigprocmask(SIG_BLOCK, &set, NULL);
	assert(rv == 0);

	int skq = kqueue();
	assert(skq >= 0);

	struct kevent kev;
	EV_SET(&kev, SIGUSR1, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
	rv = kevent(skq, &kev, 1, NULL, 0, NULL);
	assert(rv == 0);

	int kq = kqueue();
	assert(kq >= 0);

	EV_SET(&kev, skq, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);
	rv = kevent(kq, &kev, 1, NULL, 0, NULL);
	assert(rv == 0);

	for (l = 0; l < 1000000; l++) {
		rv = kill(getpid(), SIGUSR1);
		assert(rv == 0);

		/* Turn this into `#if 1` to avoid the race. */
#if 0
		rv = kevent(kq, NULL, 0, &kev, 1, NULL);
#else
		rv = kevent(kq, NULL, 0, &kev, 1, &(struct timespec) { 0, 0 });
#endif
		assert(rv == 1);
		rv = kevent(kq, NULL, 0, &kev, 1, &(struct timespec) { 0, 0 });
		assert(rv == 0);

		rv = kevent(skq, NULL, 0, &kev, 1, &(struct timespec) { 0, 0 });
		assert(rv == 1);
		rv = kevent(skq, NULL, 0, &kev, 1, &(struct timespec) { 0, 0 });
		assert(rv == 0);

		siginfo_t siginfo;

		rv = sigtimedwait(&set, &siginfo, &(struct timespec) { 0, 0 });
		assert(rv == SIGUSR1);

		rv = sigtimedwait(&set, &siginfo, &(struct timespec) { 0, 0 });
		assert(rv < 0);
		assert(errno == EAGAIN);
	}
}
EOF
cc -o /tmp/kevent16 -Wall -Wextra -O2 /tmp/kevent16.c || exit 1

/tmp/kevent16; s=$?

rm -f /tmp/kevent16.c kevent16 kevent16.core
exit $s
