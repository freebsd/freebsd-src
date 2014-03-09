/*-
 * Copyright (c) 2013 Mark R V Murray
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

#include <machine/segments.h>
#include <machine/pcb.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <dev/random/randomdev.h>
#include <dev/random/randomdev_soft.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/live_entropy_sources.h>
#include <dev/random/random_adaptors.h>

static void random_nehemiah_init(void);
static void random_nehemiah_deinit(void);
static int random_nehemiah_read(void *, int);

static struct random_hardware_source random_nehemiah = {
	.ident = "Hardware, VIA Nehemiah Padlock RNG",
	.source = RANDOM_PURE_NEHEMIAH,
	.read = random_nehemiah_read
};

/* TODO: now that the Davies-Meyer hash is gone and we only use
 * the 'xstore' instruction, do we still need to preserve the
 * FPU state with fpu_kern_(enter|leave)() ?
 */
static struct fpu_kern_ctx *fpu_ctx_save;

/* This H/W source never stores more than 8 bytes in one go */
/* ARGSUSED */
static __inline size_t
VIA_RNG_store(void *buf)
{
	uint32_t retval = 0;
	uint32_t rate = 0;

#ifdef __GNUCLIKE_ASM
	__asm __volatile(
		"movl	$0,%%edx\n\t"
		".byte	0x0f, 0xa7, 0xc0" /* xstore */
			: "=a" (retval), "+d" (rate), "+D" (buf)
			:
			: "memory"
	);
#endif
	if (rate == 0)
		return (retval&0x1f);
	return (0);
}

static void
random_nehemiah_init(void)
{

	fpu_ctx_save = fpu_kern_alloc_ctx(FPU_KERN_NORMAL);
}

static void
random_nehemiah_deinit(void)
{

	fpu_kern_free_ctx(fpu_ctx_save);
}

static int
random_nehemiah_read(void *buf, int c)
{
	uint8_t *b;
	size_t count, ret;
	uint64_t tmp;

	if ((fpu_kern_enter(curthread, fpu_ctx_save, FPU_KERN_NORMAL) == 0)) {
		b = buf;
		for (count = c; count > 0; count -= ret) {
			ret = MIN(VIA_RNG_store(&tmp), count);
			memcpy(b, &tmp, ret);
			b += ret;
		}
		fpu_kern_leave(curthread, fpu_ctx_save);
	}
	else
		c = 0;

	return (c);
}

static int
nehemiah_modevent(module_t mod, int type, void *unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		if (via_feature_rng & VIA_HAS_RNG) {
			live_entropy_source_register(&random_nehemiah);
			random_nehemiah_init();
		} else
#ifndef KLD_MODULE
			if (bootverbose)
#endif
				printf("%s: VIA Padlock RNG not present\n",
				    random_nehemiah.ident);
		break;

	case MOD_UNLOAD:
		if (via_feature_rng & VIA_HAS_RNG)
			random_nehemiah_deinit();
			live_entropy_source_deregister(&random_nehemiah);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}

	return (error);
}

LIVE_ENTROPY_SRC_MODULE(nehemiah, nehemiah_modevent, 1);
