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
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/md_var.h>
#include <machine/specialreg.h>
#include <x86/ifunc.h>

#include <dev/random/randomdev.h>

#define	RETRY_COUNT	10

static bool has_rdrand, has_rdseed;
static u_int random_ivy_read(void *, u_int);

static struct random_source random_ivy = {
	.rs_ident = "Intel Secure Key RNG",
	.rs_source = RANDOM_PURE_RDRAND,
	.rs_read = random_ivy_read
};

SYSCTL_NODE(_kern_random, OID_AUTO, rdrand, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "rdrand (ivy) entropy source");
static bool acquire_independent_seed_samples = false;
SYSCTL_BOOL(_kern_random_rdrand, OID_AUTO, rdrand_independent_seed,
    CTLFLAG_RWTUN, &acquire_independent_seed_samples, 0,
    "If non-zero, use more expensive and slow, but safer, seeded samples "
    "where RDSEED is not present.");

static bool
x86_rdrand_store(u_long *buf)
{
	u_long rndval, seed_iterations, i;
	int retry;

	/* Per [1], "§ 5.2.6 Generating Seeds from RDRAND,"
	 * machines lacking RDSEED will guarantee RDRAND is reseeded every 8kB
	 * of generated output.
	 *
	 * [1]: https://software.intel.com/en-us/articles/intel-digital-random-number-generator-drng-software-implementation-guide#inpage-nav-6-8
	 */
	if (acquire_independent_seed_samples)
		seed_iterations = 8 * 1024 / sizeof(*buf);
	else
		seed_iterations = 1;

	for (i = 0; i < seed_iterations; i++) {
		retry = RETRY_COUNT;
		__asm __volatile(
		    "1:\n\t"
		    "rdrand	%1\n\t"	/* read randomness into rndval */
		    "jc		2f\n\t" /* CF is set on success, exit retry loop */
		    "dec	%0\n\t" /* otherwise, retry-- */
		    "jne	1b\n\t" /* and loop if retries are not exhausted */
		    "2:"
		    : "+r" (retry), "=r" (rndval) : : "cc");
		if (retry == 0)
			return (false);
	}
	*buf = rndval;
	return (true);
}

static bool
x86_rdseed_store(u_long *buf)
{
	u_long rndval;
	int retry;

	retry = RETRY_COUNT;
	__asm __volatile(
	    "1:\n\t"
	    "rdseed	%1\n\t"	/* read randomness into rndval */
	    "jc		2f\n\t" /* CF is set on success, exit retry loop */
	    "dec	%0\n\t" /* otherwise, retry-- */
	    "jne	1b\n\t" /* and loop if retries are not exhausted */
	    "2:"
	    : "+r" (retry), "=r" (rndval) : : "cc");
	*buf = rndval;
	return (retry != 0);
}

static bool
x86_unimpl_store(u_long *buf __unused)
{

	panic("%s called", __func__);
}

DEFINE_IFUNC(static, bool, x86_rng_store, (u_long *buf))
{
	has_rdrand = (cpu_feature2 & CPUID2_RDRAND);
	has_rdseed = (cpu_stdext_feature & CPUID_STDEXT_RDSEED);

	if (has_rdseed)
		return (x86_rdseed_store);
	else if (has_rdrand)
		return (x86_rdrand_store);
	else
		return (x86_unimpl_store);
}

/* It is required that buf length is a multiple of sizeof(u_long). */
static u_int
random_ivy_read(void *buf, u_int c)
{
	u_long *b, rndval;
	u_int count;

	KASSERT(c % sizeof(*b) == 0, ("partial read %d", c));
	b = buf;
	for (count = c; count > 0; count -= sizeof(*b)) {
		if (!x86_rng_store(&rndval))
			break;
		*b++ = rndval;
	}
	return (c - count);
}

static int
rdrand_modevent(module_t mod, int type, void *unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		if (has_rdrand || has_rdseed) {
			random_source_register(&random_ivy);
			printf("random: fast provider: \"%s\"\n", random_ivy.rs_ident);
		}
		break;

	case MOD_UNLOAD:
		if (has_rdrand || has_rdseed)
			random_source_deregister(&random_ivy);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}

	return (error);
}

static moduledata_t rdrand_mod = {
	"rdrand",
	rdrand_modevent,
	0
};

DECLARE_MODULE(rdrand, rdrand_mod, SI_SUB_RANDOM, SI_ORDER_FOURTH);
MODULE_VERSION(rdrand, 1);
MODULE_DEPEND(rdrand, random_harvestq, 1, 1, 1);
