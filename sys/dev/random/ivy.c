/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * Copyright (c) 2013 David E. O'Brien <obrien@NUXI.org>
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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
	.ident = "Hardware, Intel Secure Key RNG",
	.source = RANDOM_PURE_RDRAND,
	.read = random_ivy_read
};

static inline int
ivy_rng_store(long *buf)
{
#ifdef __GNUCLIKE_ASM
	long tmp;
	int retry;

	retry = RETRY_COUNT;
	__asm __volatile(
	    "1:\n\t"
	    "rdrand	%2\n\t"	/* read randomness into tmp */
	    "jb		2f\n\t" /* CF is set on success, exit retry loop */
	    "dec	%0\n\t" /* otherwise, retry-- */
	    "jne	1b\n\t" /* and loop if retries are not exhausted */
	    "jmp	3f\n"	/* failure, retry is 0, used as return value */
	    "2:\n\t"
	    "mov	%2,%1\n\t" /* *buf = tmp */
	    "3:"
	    : "+q" (retry), "=m" (*buf), "=q" (tmp) : : "cc");
	return (retry);
#else /* __GNUCLIKE_ASM */
	return (0);
#endif
}

static int
random_ivy_read(void *buf, int c)
{
	long *b;
	int count;

	KASSERT(c % sizeof(long) == 0, ("partial read %d", c));
	for (b = buf, count = c; count > 0; count -= sizeof(long), b++) {
		if (ivy_rng_store(b) == 0)
			break;
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
