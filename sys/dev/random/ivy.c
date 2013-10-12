/*-
 * Copyright (c) 2013 David E. O'Brien <obrien@NUXI.org>
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
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/random.h>
#include <sys/selinfo.h>
#include <sys/systm.h>

#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <dev/random/randomdev.h>
#include <dev/random/randomdev_soft.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/live_entropy_sources.h>
#include <dev/random/random_adaptors.h>

#define	RETRY_COUNT	10

static int random_ivy_read(void *, int);

static struct random_hardware_source random_ivy = {
	.ident = "Hardware, Intel IvyBridge+ RNG",
	.source = RANDOM_PURE_RDRAND,
	.read = random_ivy_read
};

static inline int
ivy_rng_store(uint64_t *tmp)
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

static int
random_ivy_read(void *buf, int c)
{
	uint8_t *b;
	int count, ret, retry;
	uint64_t tmp;

	b = buf;
	for (count = c; count > 0; count -= ret) {
		for (retry = 0; retry < RETRY_COUNT; retry++) {
			ret = ivy_rng_store(&tmp);
			if (ret != 0)
				break;
		}
		if (ret == 0)
			break;
		if (ret > count)
			ret = count;
		memcpy(b, &tmp, ret);
		b += ret;
	}
	return (c - count);
}

static int
rdrand_modevent(module_t mod, int type, void *unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		if (cpu_feature2 & CPUID2_RDRAND)
			live_entropy_source_register(&random_ivy);
		else
#ifndef KLD_MODULE
			if (bootverbose)
#endif
				printf("%s: RDRAND is not present\n",
				    random_ivy.ident);
		break;

	case MOD_UNLOAD:
		if (cpu_feature2 & CPUID2_RDRAND)
			live_entropy_source_deregister(&random_ivy);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}

	return (error);
}

LIVE_ENTROPY_SRC_MODULE(random_rdrand, rdrand_modevent, 1);
