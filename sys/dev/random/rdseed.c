/*-
 * Copyright (c) 2013, 2025, David E. O'Brien <deo@NUXI.org>
 * Copyright (c) 2013 The FreeBSD Foundation
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

static u_int random_rdseed_read(void *, u_int);

static struct random_source random_rdseed = {
	.rs_ident = "Intel Secure Key Seed",
	.rs_source = RANDOM_PURE_RDSEED,
	.rs_read = random_rdseed_read
};

SYSCTL_NODE(_kern_random, OID_AUTO, rdseed, CTLFLAG_RW, 0,
    "rdseed (x86) entropy source");
/* XXX: kern.random.rdseed.enabled=0 also disables RDRAND */
static bool enabled = true;
SYSCTL_BOOL(_kern_random_rdseed, OID_AUTO, enabled, CTLFLAG_RDTUN, &enabled, 0,
    "If zero, disable the use of RDSEED.");

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

/* It is required that buf length is a multiple of sizeof(u_long). */
static u_int
random_rdseed_read(void *buf, u_int c)
{
	u_long *b, rndval;
	u_int count;

	KASSERT(c % sizeof(*b) == 0, ("partial read %d", c));
	b = buf;
	for (count = c; count > 0; count -= sizeof(*b)) {
		if (!x86_rdseed_store(&rndval))
			break;
		*b++ = rndval;
	}
	return (c - count);
}

static int
rdseed_modevent(module_t mod, int type, void *unused)
{
	bool has_rdseed;
	int error = 0;

	has_rdseed = (cpu_stdext_feature & CPUID_STDEXT_RDSEED);

	switch (type) {
	case MOD_LOAD:
		if (has_rdseed && enabled) {
			random_source_register(&random_rdseed);
			printf("random: fast provider: \"%s\"\n", random_rdseed.rs_ident);
		}
		break;

	case MOD_UNLOAD:
		if (has_rdseed)
			random_source_deregister(&random_rdseed);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}

	return (error);
}

static moduledata_t rdseed_mod = {
	"rdseed",
	rdseed_modevent,
	0
};

DECLARE_MODULE(rdseed, rdseed_mod, SI_SUB_RANDOM, SI_ORDER_FOURTH);
MODULE_VERSION(rdseed, 1);
MODULE_DEPEND(rdseed, random_harvestq, 1, 1, 1);

/*
 * Intel's RDSEED Entropy Assessment Report min-entropy claim is 0.6 Shannons
 * per bit of data output.  Rrefer to the following Entropy Source Validation
 * (ESV) certificates:
 *
 *	E#87:	Junos OS Physical Entropy Source - Broadwell EP 10-Core Die
 *		Broadwell-EP-10 FCLGA2011 Intel(R) Xeon(R) E5-2620 V4 Processor
 *		https://csrc.nist.gov/projects/cryptographic-module-validation-program/entropy-validations/certificate/87
 *		(URLs below omitted for brevity but follow same format.)
 *
 *	E#121:	Junos OS Physical Entropy Source - Intel Atom C3000 Series
 *		(Denverton) 16 Core Die with FCBGA1310 Package
 *
 *	E#122:	Junos OS Physical Entropy Source - Intel Xeon D-1500 Family
 *		(Broadwell) 8 Core Die with FCBGA1667 Package
 *
 *	E#123:	Junos OS Physical Entropy Source - Intel Xeon D-2100 Series
 *		(Skylake) 18 Core Die with FCBGA2518 Package
 *
 *	E#141:	Junos OS Physical Entropy Source - Intel Xeon D-10 Series
 *		(Ice Lake-D-10) Die with FCBGA2227 Package
 *
 *	E#169:	Junos OS Physical Entropy Source - Intel Xeon AWS-1000 v4 and
 *		E5 v4 (Broadwell EP) 15 Core Die with FCLGA2011 Package
 */
