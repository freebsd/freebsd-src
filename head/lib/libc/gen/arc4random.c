/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Arc4 random number generator for OpenBSD.
 *
 * This code is derived from section 17.1 of Applied Cryptography,
 * second edition, which describes a stream cipher allegedly
 * compatible with RSA Labs "RC4" cipher (the actual description of
 * which is a trade secret).  The same algorithm is used as a stream
 * cipher called "arcfour" in Tatu Ylonen's ssh package.
 *
 * Here the stream cipher has been modified always to include the time
 * when initializing the state.  That makes it impossible to
 * regenerate the same random sequence twice, so this can't be used
 * for encryption, but will generate good random numbers.
 *
 * RC4 is a registered trademark of RSA Laboratories.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "libc_private.h"
#include "un-namespace.h"

struct arc4_stream {
	u_int8_t i;
	u_int8_t j;
	u_int8_t s[256];
};

static pthread_mutex_t	arc4random_mtx = PTHREAD_MUTEX_INITIALIZER;

#define	RANDOMDEV	"/dev/random"
#define KEYSIZE		128
#define	THREAD_LOCK()						\
	do {							\
		if (__isthreaded)				\
			_pthread_mutex_lock(&arc4random_mtx);	\
	} while (0)

#define	THREAD_UNLOCK()						\
	do {							\
		if (__isthreaded)				\
			_pthread_mutex_unlock(&arc4random_mtx);	\
	} while (0)

static struct arc4_stream rs;
static int rs_initialized;
static int rs_stired;
static int arc4_count;

static inline u_int8_t arc4_getbyte(void);
static void arc4_stir(void);

static inline void
arc4_init(void)
{
	int     n;

	for (n = 0; n < 256; n++)
		rs.s[n] = n;
	rs.i = 0;
	rs.j = 0;
}

static inline void
arc4_addrandom(u_char *dat, int datlen)
{
	int     n;
	u_int8_t si;

	rs.i--;
	for (n = 0; n < 256; n++) {
		rs.i = (rs.i + 1);
		si = rs.s[rs.i];
		rs.j = (rs.j + si + dat[n % datlen]);
		rs.s[rs.i] = rs.s[rs.j];
		rs.s[rs.j] = si;
	}
	rs.j = rs.i;
}

static void
arc4_stir(void)
{
	int done, fd, n;
	struct {
		struct timeval	tv;
		pid_t 		pid;
		u_int8_t 	rnd[KEYSIZE];
	} rdat;

	fd = _open(RANDOMDEV, O_RDONLY, 0);
	done = 0;
	if (fd >= 0) {
		if (_read(fd, &rdat, KEYSIZE) == KEYSIZE)
			done = 1;
		(void)_close(fd);
	} 
	if (!done) {
		(void)gettimeofday(&rdat.tv, NULL);
		rdat.pid = getpid();
		/* We'll just take whatever was on the stack too... */
	}

	arc4_addrandom((u_char *)&rdat, KEYSIZE);

	/*
	 * Throw away the first N bytes of output, as suggested in the
	 * paper "Weaknesses in the Key Scheduling Algorithm of RC4"
	 * by Fluher, Mantin, and Shamir.  N=1024 is based on
	 * suggestions in the paper "(Not So) Random Shuffles of RC4"
	 * by Ilya Mironov.
	 */
	for (n = 0; n < 1024; n++)
		(void) arc4_getbyte();
	arc4_count = 1600000;
}

static inline u_int8_t
arc4_getbyte(void)
{
	u_int8_t si, sj;

	rs.i = (rs.i + 1);
	si = rs.s[rs.i];
	rs.j = (rs.j + si);
	sj = rs.s[rs.j];
	rs.s[rs.i] = sj;
	rs.s[rs.j] = si;

	return (rs.s[(si + sj) & 0xff]);
}

static inline u_int32_t
arc4_getword(void)
{
	u_int32_t val;

	val = arc4_getbyte() << 24;
	val |= arc4_getbyte() << 16;
	val |= arc4_getbyte() << 8;
	val |= arc4_getbyte();

	return (val);
}

static void
arc4_check_init(void)
{
	if (!rs_initialized) {
		arc4_init();
		rs_initialized = 1;
	}
}

static inline void
arc4_check_stir(void)
{
	if (!rs_stired || arc4_count <= 0) {
		arc4_stir();
		rs_stired = 1;
	}
}

void
arc4random_stir(void)
{
	THREAD_LOCK();
	arc4_check_init();
	arc4_stir();
	rs_stired = 1;
	THREAD_UNLOCK();
}

void
arc4random_addrandom(u_char *dat, int datlen)
{
	THREAD_LOCK();
	arc4_check_init();
	arc4_check_stir();
	arc4_addrandom(dat, datlen);
	THREAD_UNLOCK();
}

u_int32_t
arc4random(void)
{
	u_int32_t rnd;

	THREAD_LOCK();
	arc4_check_init();
	arc4_check_stir();
	rnd = arc4_getword();
	arc4_count -= 4;
	THREAD_UNLOCK();

	return (rnd);
}

void
arc4random_buf(void *_buf, size_t n)
{
	u_char *buf = (u_char *)_buf;

	THREAD_LOCK();
	arc4_check_init();
	while (n--) {
		arc4_check_stir();
		buf[n] = arc4_getbyte();
		arc4_count--;
	}
	THREAD_UNLOCK();
}

/*
 * Calculate a uniformly distributed random number less than upper_bound
 * avoiding "modulo bias".
 *
 * Uniformity is achieved by generating new random numbers until the one
 * returned is outside the range [0, 2**32 % upper_bound).  This
 * guarantees the selected random number will be inside
 * [2**32 % upper_bound, 2**32) which maps back to [0, upper_bound)
 * after reduction modulo upper_bound.
 */
u_int32_t
arc4random_uniform(u_int32_t upper_bound)
{
	u_int32_t r, min;

	if (upper_bound < 2)
		return (0);

#if (ULONG_MAX > 0xffffffffUL)
	min = 0x100000000UL % upper_bound;
#else
	/* Calculate (2**32 % upper_bound) avoiding 64-bit math */
	if (upper_bound > 0x80000000)
		min = 1 + ~upper_bound;		/* 2**32 - upper_bound */
	else {
		/* (2**32 - (x * 2)) % x == 2**32 % x when x <= 2**31 */
		min = ((0xffffffff - (upper_bound * 2)) + 1) % upper_bound;
	}
#endif

	/*
	 * This could theoretically loop forever but each retry has
	 * p > 0.5 (worst case, usually far better) of selecting a
	 * number inside the range we need, so it should rarely need
	 * to re-roll.
	 */
	for (;;) {
		r = arc4random();
		if (r >= min)
			break;
	}

	return (r % upper_bound);
}

#if 0
/*-------- Test code for i386 --------*/
#include <stdio.h>
#include <machine/pctr.h>
int
main(int argc, char **argv)
{
	const int iter = 1000000;
	int     i;
	pctrval v;

	v = rdtsc();
	for (i = 0; i < iter; i++)
		arc4random();
	v = rdtsc() - v;
	v /= iter;

	printf("%qd cycles\n", v);
}
#endif
