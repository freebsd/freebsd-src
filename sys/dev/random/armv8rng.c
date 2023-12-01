/*-
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * This software was developed by Andrew Turner under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/random.h>

#include <machine/armreg.h>

#include <dev/random/randomdev.h>

static u_int random_rndr_read(void *, u_int);

static bool has_rndr;
static struct random_source random_armv8_rndr = {
	.rs_ident = "Armv8 rndr RNG",
	.rs_source = RANDOM_PURE_ARMV8,
	.rs_read = random_rndr_read,
};

static inline int
random_rndr_read_one(u_long *buf)
{
	u_long val;
	int loop, ret;

	loop = 10;
	do {
		__asm __volatile(
		    /* Read the random number */
		    "mrs	%0, " __XSTRING(RNDRRS_REG) "\n"
		    /* 1 on success, 0 on failure */
		    "cset	%w1, ne\n"
		    : "=&r" (val), "=&r"(ret) :: "cc");
	} while (ret != 0 && --loop > 0);

	if (ret != 0)
		*buf = val;

	return (ret);
}

static u_int
random_rndr_read(void *buf, u_int c)
{
	u_long *b;
	u_int count;

	b = buf;
	for (count = 0; count < c; count += sizeof(*b)) {
		if (!random_rndr_read_one(b))
			break;

		b++;
	}

	return (count);
}

static int
rndr_modevent(module_t mod, int type, void *unused)
{
	uint64_t reg;
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		has_rndr = false;
		if (get_kernel_reg(ID_AA64ISAR0_EL1, &reg) &&
		    ID_AA64ISAR0_RNDR_VAL(reg) != ID_AA64ISAR0_RNDR_NONE) {
			has_rndr = true;
			random_source_register(&random_armv8_rndr);
			printf("random: fast provider: \"%s\"\n",
			    random_armv8_rndr.rs_ident);
		}
		break;

	case MOD_UNLOAD:
		if (has_rndr)
			random_source_deregister(&random_armv8_rndr);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}

	return (error);
}

static moduledata_t rndr_mod = {
	"rndr",
	rndr_modevent,
	0
};

DECLARE_MODULE(rndr, rndr_mod, SI_SUB_RANDOM, SI_ORDER_FOURTH);
MODULE_VERSION(rndr, 1);
MODULE_DEPEND(rndr, random_harvestq, 1, 1, 1);
