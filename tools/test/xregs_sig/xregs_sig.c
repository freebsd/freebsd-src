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

struct xregs_bank {
	const char	*b_name;
	const char	*r_name;
	uint32_t	regs;
	uint32_t	bytes;
	void		(*x2c)(uint8_t *);
	void		(*c2x)(uint8_t *);
};

int xregs_banks_max(void);

#if defined(__amd64__)
void cpu_to_xmm(uint8_t *);
void xmm_to_cpu(uint8_t *);
void cpu_to_avx(uint8_t *);
void avx_to_cpu(uint8_t *);

static const struct xregs_bank xregs_banks[] = {
	{
		.b_name	= "SSE",
		.r_name	= "xmm",
		.regs	= 16,
		.bytes	= 16,
		.x2c	= xmm_to_cpu,
		.c2x	= cpu_to_xmm,
	},
	{
		.b_name	= "AVX",
		.r_name	= "ymm",
		.regs	= 16,
		.bytes	= 32,
		.x2c	= avx_to_cpu,
		.c2x	= cpu_to_avx,
	},
};
#elif defined(__aarch64__)
void cpu_to_vfp(uint8_t *);
void vfp_to_cpu(uint8_t *);

static const struct xregs_bank xregs_banks[] = {
	{
		.b_name	= "VFP",
		.r_name	= "q",
		.regs	= 32,
		.bytes	= 16,
		.x2c	= vfp_to_cpu,
		.c2x	= cpu_to_vfp,
	},
};
#endif

static atomic_uint sigs;
static int max_bank_idx;


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


static void
fill_xregs(uint8_t *xregs, int bank)
{
	arc4random_buf(xregs, xregs_banks[bank].regs * xregs_banks[bank].bytes);
}

static void
dump_xregs(const uint8_t *r, int bank)
{
	unsigned k;

	for (k = 0; k < xregs_banks[bank].bytes; k++) {
		if (k != 0)
			printf(" ");
		printf("%02x", r[k]);
	}
	printf("\n");
}

static pthread_mutex_t show_lock;

static void
show_diff(const uint8_t *xregs1, const uint8_t *xregs2, int bank)
{
	const uint8_t *r1, *r2;
	unsigned i, j;

#if defined(__FreeBSD__)
	printf("thr %d\n", pthread_getthreadid_np());
#elif defined(__linux__)
	printf("thr %ld\n", syscall(SYS_gettid));
#endif
	for (i = 0; i < xregs_banks[bank].regs; i++) {
		r1 = xregs1 + i * xregs_banks[bank].bytes;
		r2 = xregs2 + i * xregs_banks[bank].bytes;
		for (j = 0; j < xregs_banks[bank].bytes; j++) {
			if (r1[j] != r2[j]) {
				printf("%%%s%u\n", xregs_banks[bank].r_name, i);
				dump_xregs(r1, bank);
				dump_xregs(r2, bank);
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
worker_thread(void *arg)
{
	int bank = (uintptr_t)arg;
	int sz = xregs_banks[bank].regs * xregs_banks[bank].bytes;
	uint8_t xregs[sz], xregs_cpu[sz], zero_xregs[sz];

	memset(zero_xregs, 0, sz);

	fill_xregs(xregs, bank);
	for (;;) {
		xregs_banks[bank].x2c(xregs);
		my_pause();
		xregs_banks[bank].c2x(xregs_cpu);
		if (memcmp(xregs, xregs_cpu, sz) != 0) {
			pthread_mutex_lock(&show_lock);
			show_diff(xregs, xregs_cpu, bank);
			abort();
			pthread_mutex_unlock(&show_lock);
		}

		xregs_banks[bank].x2c(zero_xregs);
		my_pause();
		xregs_banks[bank].c2x(xregs_cpu);
		if (memcmp(zero_xregs, xregs_cpu, sz) != 0) {
			pthread_mutex_lock(&show_lock);
			show_diff(zero_xregs, xregs_cpu, bank);
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
	int error, i, ncpu, bank;

	max_bank_idx = xregs_banks_max();

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
	if (max_bank_idx == 0)
		ncpu *= 2;
	bank = 0;
	pthread_t wt[ncpu];
nextbank:
	printf("Starting %d threads for registers bank %s sized [%d][%d]\n", ncpu,
	    xregs_banks[bank].b_name, xregs_banks[bank].regs, xregs_banks[bank].bytes);
	for (i = 0; i < ncpu; i++) {
		error = pthread_create(&wt[i], NULL, worker_thread,
		    (void *)(uintptr_t)bank);
		if (error != 0) {
			fprintf(stderr, "pthread_create %s\n", strerror(error));
		}
	}
	if (++bank <= max_bank_idx)
		goto nextbank;

	alarm(TIMO);
	for (;;) {
		for (i = 0; i < ncpu; i++) {
			my_pause();
			pthread_kill(wt[i], SIGUSR1);
		}
	}
}
