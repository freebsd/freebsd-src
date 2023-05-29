/* $Id: avx_sig.c,v 1.12 2021/12/11 22:47:09 kostik Exp $ */
/*
 * Naive test to check that context switches and signal delivery do
 * not corrupt AVX registers file (%xmm).  Run until some
 * inconsistency detected, then aborts.
 *
 * FreeBSD:
 * ${CC} -Wall -Wextra -O -g -o avx_sig avx_sig.c -lpthread
 * Linux
 * ${CC} -D_GNU_SOURCE -Wall -Wextra -O -g -o avx_sig avx_sig.c -lbsd -lpthread
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <errno.h>
#include <pthread.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif
#ifdef __linux__
#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif
#if !defined(__GLIBC__) || (__GLIBC__ * 100 + __GLIBC_MINOR__) < 236
#include <bsd/stdlib.h>
#endif
#endif
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* SIGALRM interval in seconds. */
#ifndef TIMO
#define	TIMO		5
#endif

#ifndef __unused
#define	__unused	__attribute__((__unused__))
#endif
#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

struct xmmreg {
	uint8_t xmm_bytes[16];
};

struct xmm {
	struct xmmreg xmmreg[16];
};

#define	X2C(r) 	asm("movdqu %0, %%xmm" #r : "=m" (xmm->xmmreg[r]))
#define	C2X(r)	asm("movdqu %%xmm" #r ", %0" : : "m" (xmm->xmmreg[r]) : "xmm" #r)

static void
cpu_to_xmm(struct xmm *xmm)
{
	C2X(0);	C2X(1);	C2X(2);	C2X(3);	C2X(4);	C2X(5);	C2X(6);	C2X(7);
	C2X(8);	C2X(9);	C2X(10); C2X(11); C2X(12); C2X(13); C2X(14); C2X(15);
}

static void
xmm_to_cpu(struct xmm *xmm)
{
	X2C(0);	X2C(1);	X2C(2);	X2C(3);	X2C(4);	X2C(5);	X2C(6);	X2C(7);
	X2C(8);	X2C(9);	X2C(10); X2C(11); X2C(12); X2C(13); X2C(14); X2C(15);
}

#undef C2X
#undef X2C

static atomic_uint sigs;

static void
sigusr1_handler(int sig __unused, siginfo_t *si __unused, void *m __unused)
{
	atomic_fetch_add_explicit(&sigs, 1, memory_order_relaxed);
}

static void
sigalrm_handler(int sig __unused)
{
	struct rusage r;

	if (getrusage(RUSAGE_SELF, &r) == 0) {
		printf("%lu vctx %lu nvctx %lu nsigs %u SIGUSR1\n",
		    r.ru_nvcsw, r.ru_nivcsw, r.ru_nsignals, sigs);
	}
	alarm(TIMO);
}

static struct xmm zero_xmm = {};

static void
fill_xmm(struct xmm *xmm)
{
	arc4random_buf(xmm, sizeof(*xmm));
}

static void
dump_xmm(const struct xmmreg *r)
{
	unsigned k;

	for (k = 0; k < nitems(r->xmm_bytes); k++) {
		if (k != 0)
			printf(" ");
		printf("%02x", r->xmm_bytes[k]);
	}
	printf("\n");
}

static pthread_mutex_t show_lock;

static void
show_diff(const struct xmm *xmm1, const struct xmm *xmm2)
{
	const struct xmmreg *r1, *r2;
	unsigned i, j;

#if defined(__FreeBSD__)
	printf("thr %d\n", pthread_getthreadid_np());
#elif defined(__linux__)
	printf("thr %ld\n", syscall(SYS_gettid));
#endif
	for (i = 0; i < nitems(xmm1->xmmreg); i++) {
		r1 = &xmm1->xmmreg[i];
		r2 = &xmm2->xmmreg[i];
		for (j = 0; j < nitems(r1->xmm_bytes); j++) {
			if (r1->xmm_bytes[j] != r2->xmm_bytes[j]) {
				printf("xmm%u\n", i);
				dump_xmm(r1);
				dump_xmm(r2);
				break;
			}
		}
	}
}

static void
my_pause(void)
{
	usleep(0);
}

static void *
worker_thread(void *arg __unused)
{
	struct xmm xmm, xmm_cpu;

	fill_xmm(&xmm);
	for (;;) {
		xmm_to_cpu(&xmm);
		my_pause();
		cpu_to_xmm(&xmm_cpu);
		if (memcmp(&xmm, &xmm_cpu, sizeof(struct xmm)) != 0) {
			pthread_mutex_lock(&show_lock);
			show_diff(&xmm, &xmm_cpu);
			abort();
			pthread_mutex_unlock(&show_lock);
		}

		xmm_to_cpu(&zero_xmm);
		my_pause();
		cpu_to_xmm(&xmm_cpu);
		if (memcmp(&zero_xmm, &xmm_cpu, sizeof(struct xmm)) != 0) {
			pthread_mutex_lock(&show_lock);
			show_diff(&zero_xmm, &xmm_cpu);
			abort();
			pthread_mutex_unlock(&show_lock);
		}
	}
	return (NULL);
}

int
main(void)
{
	struct sigaction sa;
	int error, i, ncpu;

	bzero(&sa, sizeof(sa));
	sa.sa_handler = sigalrm_handler;
	if (sigaction(SIGALRM, &sa, NULL) == -1) {
		fprintf(stderr, "sigaction SIGALRM %s\n", strerror(errno));
		exit(1);
	}

	bzero(&sa, sizeof(sa));
	sa.sa_sigaction = sigusr1_handler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		fprintf(stderr, "sigaction SIGUSR1 %s\n", strerror(errno));
		exit(1);
	}

	error = pthread_mutex_init(&show_lock, NULL);
	if (error != 0) {
		fprintf(stderr, "pthread_mutex_init %s\n", strerror(error));
		exit(1);
	}

	ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	ncpu *= 2;
	pthread_t wt[ncpu];
	for (i = 0; i < ncpu; i++) {
		error = pthread_create(&wt[i], NULL, worker_thread, NULL);
		if (error != 0) {
			fprintf(stderr, "pthread_create %s\n", strerror(error));
		}
	}

	alarm(TIMO);
	for (;;) {
		for (i = 0; i < ncpu; i++) {
			my_pause();
			pthread_kill(wt[i], SIGUSR1);
		}
	}
}
