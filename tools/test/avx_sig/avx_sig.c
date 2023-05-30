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

#if defined(__amd64__)
#define	SIMDRNAM	"xmm"
#define	NREGS		16
#elif defined(__aarch64__)
#define	SIMDRNAM	"q"
#define	NREGS		32
#endif

struct simdreg {
	uint8_t simd_bytes[16];
};

struct simd {
	struct simdreg simdreg[NREGS];
};

void cpu_to_simd(struct simd *simd);
void simd_to_cpu(struct simd *simd);

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

static struct simd zero_simd = {};

static void
fill_simd(struct simd *simd)
{
	arc4random_buf(simd, sizeof(*simd));
}

static void
dump_simd(const struct simdreg *r)
{
	unsigned k;

	for (k = 0; k < nitems(r->simd_bytes); k++) {
		if (k != 0)
			printf(" ");
		printf("%02x", r->simd_bytes[k]);
	}
	printf("\n");
}

static pthread_mutex_t show_lock;

static void
show_diff(const struct simd *simd1, const struct simd *simd2)
{
	const struct simdreg *r1, *r2;
	unsigned i, j;

#if defined(__FreeBSD__)
	printf("thr %d\n", pthread_getthreadid_np());
#elif defined(__linux__)
	printf("thr %ld\n", syscall(SYS_gettid));
#endif
	for (i = 0; i < nitems(simd1->simdreg); i++) {
		r1 = &simd1->simdreg[i];
		r2 = &simd2->simdreg[i];
		for (j = 0; j < nitems(r1->simd_bytes); j++) {
			if (r1->simd_bytes[j] != r2->simd_bytes[j]) {
				printf("%%%s%u\n", SIMDRNAM, i);
				dump_simd(r1);
				dump_simd(r2);
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
	struct simd simd, simd_cpu;

	fill_simd(&simd);
	for (;;) {
		simd_to_cpu(&simd);
		my_pause();
		cpu_to_simd(&simd_cpu);
		if (memcmp(&simd, &simd_cpu, sizeof(struct simd)) != 0) {
			pthread_mutex_lock(&show_lock);
			show_diff(&simd, &simd_cpu);
			abort();
			pthread_mutex_unlock(&show_lock);
		}

		simd_to_cpu(&zero_simd);
		my_pause();
		cpu_to_simd(&simd_cpu);
		if (memcmp(&zero_simd, &simd_cpu, sizeof(struct simd)) != 0) {
			pthread_mutex_lock(&show_lock);
			show_diff(&zero_simd, &simd_cpu);
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
