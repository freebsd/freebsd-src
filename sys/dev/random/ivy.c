/*-
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_cpu.h"

#ifdef RDRAND_RNG

#include <sys/param.h>
#include <sys/time.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/selinfo.h>
#include <sys/systm.h>
#include <dev/random/randomdev.h>

#define	RETRY_COUNT	10

static void random_ivy_init(void);
static void random_ivy_deinit(void);
static int random_ivy_read(void *, int);

struct random_systat random_ivy = {
	.ident = "Hardware, Intel IvyBridge+ RNG",
	.init = random_ivy_init,
	.deinit = random_ivy_deinit,
	.read = random_ivy_read,
	.write = (random_write_func_t *)random_null_func,
	.reseed = (random_reseed_func_t *)random_null_func,
	.seeded = 1,
};

static inline int
ivy_rng_store(long *tmp)
{
#ifdef __GNUCLIKE_ASM
	uint32_t count;

	__asm __volatile(
#ifdef __amd64__
	    "rdrand\t%%rax\n\t"
	    "jnc\t1f\n\t"
	    "movq\t%%rax,%1\n\t"
	    "movl\t$8,%%eax\n"
#else /* i386 */
	    "rdrand\t%%eax\n\t"
	    "jnc\t1f\n\t"
	    "movl\t%%eax,%1\n\t"
	    "movl\t$4,%%eax\n"
#endif
	    "1:\n"	/* %eax is cleared by processor on failure */
	    : "=a" (count), "=g" (*tmp) : "a" (0) : "cc");
	return (count);
#else /* __GNUCLIKE_ASM */
	return (0);
#endif
}

static void
random_ivy_init(void)
{
}

void
random_ivy_deinit(void)
{
}

static int
random_ivy_read(void *buf, int c)
{
	char *b;
	long tmp;
	int count, res, retry;

	for (count = c, b = buf; count > 0; count -= res, b += res) {
		for (retry = 0; retry < RETRY_COUNT; retry++) {
			res = ivy_rng_store(&tmp);
			if (res != 0)
				break;
		}
		if (res == 0)
			break;
		if (res > count)
			res = count;
		memcpy(b, &tmp, res);
	}
	return (c - count);
}

#endif
