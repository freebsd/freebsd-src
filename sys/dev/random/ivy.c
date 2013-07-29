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

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/selinfo.h>
#include <sys/systm.h>

#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <dev/random/random_adaptors.h>
#include <dev/random/randomdev.h>

#define	RETRY_COUNT	10

static void random_ivy_init(void);
static void random_ivy_deinit(void);
static int random_ivy_read(void *, int);

struct random_adaptor random_ivy = {
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
	    ".byte\t0x48,0x0f,0xc7,0xf0\n\t" /* rdrand %rax */
	    "jnc\t1f\n\t"
	    "movq\t%%rax,%1\n\t"
	    "movl\t$8,%%eax\n"
#else /* i386 */
	    ".byte\t0x0f,0xc7,0xf0\n\t" /* rdrand %eax */
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

static int
rdrand_modevent(module_t mod, int type, void *unused)
{

	switch (type) {
	case MOD_LOAD:
		if (cpu_feature2 & CPUID2_RDRAND) {
			random_adaptor_register("rdrand", &random_ivy);
			EVENTHANDLER_INVOKE(random_adaptor_attach, &random_ivy);
			return (0);
		} else {
#ifndef KLD_MODULE
			if (bootverbose)
#endif
				printf(
			    "%s: RDRAND feature is not present on this CPU\n",
				    random_ivy.ident);
#ifdef KLD_MODULE
			return (ENXIO);
#else
			return (0);
#endif
		}
	}

	return (EINVAL);
}

RANDOM_ADAPTOR_MODULE(random_rdrand, rdrand_modevent, 1);
