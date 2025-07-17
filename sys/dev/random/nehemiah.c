/*-
 * Copyright (c) 2013-2015 Mark R V Murray
 * Copyright (c) 2013 David E. O'Brien <obrien@NUXI.org>
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/random.h>
#include <sys/systm.h>

#include <machine/segments.h>
#include <machine/pcb.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <dev/random/randomdev.h>

static u_int random_nehemiah_read(void *, u_int);

static struct random_source random_nehemiah = {
	.rs_ident = "VIA Nehemiah Padlock RNG",
	.rs_source = RANDOM_PURE_NEHEMIAH,
	.rs_read = random_nehemiah_read
};

/* This H/W source never stores more than 8 bytes in one go */
/* ARGSUSED */
static __inline size_t
VIA_RNG_store(void *buf)
{
	uint32_t retval = 0;
	uint32_t rate = 0;

	__asm __volatile(
		"movl	$0,%%edx\n\t"
		".byte 0x0f, 0xa7, 0xc0"
			: "=a" (retval), "+d" (rate), "+D" (buf)
			:
			: "memory"
	);
	if (rate == 0)
		return (retval&0x1f);
	return (0);
}

/* It is specifically allowed that buf is a multiple of sizeof(long) */
static u_int
random_nehemiah_read(void *buf, u_int c)
{
	uint8_t *b;
	size_t count, ret;
	uint64_t tmp;

	fpu_kern_enter(curthread, NULL, FPU_KERN_NORMAL | FPU_KERN_NOCTX);
	b = buf;
	for (count = c; count > 0; count -= ret) {
		ret = MIN(VIA_RNG_store(&tmp), count);
		memcpy(b, &tmp, ret);
		b += ret;
	}
	fpu_kern_leave(curthread, NULL);

	return (c);
}

static int
nehemiah_modevent(module_t mod, int type, void *unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		if (via_feature_rng & VIA_HAS_RNG) {
			random_source_register(&random_nehemiah);
			printf("random: fast provider: \"%s\"\n", random_nehemiah.rs_ident);
		}
		break;

	case MOD_UNLOAD:
		if (via_feature_rng & VIA_HAS_RNG) {
			random_source_deregister(&random_nehemiah);
		}
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}

	return (error);
}

static moduledata_t nehemiah_mod = {
	"nehemiah",
	nehemiah_modevent,
	0
};

DECLARE_MODULE(nehemiah, nehemiah_mod, SI_SUB_RANDOM, SI_ORDER_FOURTH);
MODULE_VERSION(nehemiah, 1);
MODULE_DEPEND(nehemiah, random_harvestq, 1, 1, 1);
