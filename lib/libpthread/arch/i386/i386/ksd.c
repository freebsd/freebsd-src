/*-
 * Copyright (C) 2003 David Xu <davidxu@freebsd.org>
 * Copyright (c) 2001,2003 Daniel Eischen <deischen@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <machine/cpufunc.h>
#include <machine/segments.h>
#include <machine/sysarch.h>

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

#include "pthread_md.h"
#include "ksd.h"

#define LDT_ENTRIES 8192
#define LDT_WORDS   (8192/sizeof(unsigned int))
#define LDT_RESERVED NLDT

static unsigned int ldt_mask[LDT_WORDS];
static int initialized = 0;

static void	initialize(void);

static void
initialize(void)
{
	int i, j;

	memset(ldt_mask, 0xFF, sizeof(ldt_mask));
	/* Reserve system predefined LDT entries */
	for (i = 0; i < LDT_RESERVED; ++i) {
		j = i / 32;
		ldt_mask[j] &= ~(1 << (i % 32));
	}
	initialized = 1;
}

static u_int
alloc_ldt_entry(void)
{
	u_int i, j, index;
	
	index = 0;
	for (i = 0; i < LDT_WORDS; ++i) {
		if (ldt_mask[i] != 0) {
			j = bsfl(ldt_mask[i]);
			ldt_mask[i] &= ~(1 << j);
			index = i * 32 + j;
			break;
		}
	}
	return (index);
}

static void
free_ldt_entry(u_int index)
{
	u_int i, j;

	if (index < LDT_RESERVED || index >= LDT_ENTRIES)
		return;
	i = index / 32;
	j = index % 32;
	ldt_mask[i] |= (1 << j);
}

/*
 * Initialize KSD.  This also includes setting up the LDT.
 */
int
_ksd_create(struct ksd *ksd, void *base, int size)
{
	union descriptor ldt;

	if (initialized == 0)
		initialize();
	ksd->ldt = alloc_ldt_entry();
	if (ksd->ldt == 0)
		return (-1);
	ksd->base = base;
	ksd->size = size;
	ldt.sd.sd_hibase = (unsigned int)ksd->base >> 24;
	ldt.sd.sd_lobase = (unsigned int)ksd->base & 0xFFFFFF;
	ldt.sd.sd_hilimit = (size >> 16) & 0xF;
	ldt.sd.sd_lolimit = ksd->size & 0xFFFF;
	ldt.sd.sd_type = SDT_MEMRWA;
	ldt.sd.sd_dpl = SEL_UPL;
	ldt.sd.sd_p = 1;
	ldt.sd.sd_xx = 0;
	ldt.sd.sd_def32 = 1;
	ldt.sd.sd_gran = 0;	/* no more than 1M */
	if (i386_set_ldt(ksd->ldt, &ldt, 1) < 0) {
		free_ldt_entry(ksd->ldt);
		return (-1);
	}
	ksd->flags = KSDF_INITIALIZED;
	return (0);
}

void
_ksd_destroy(struct ksd *ksd)
{
	if ((ksd->flags & KSDF_INITIALIZED) != 0) {
		free_ldt_entry(ksd->ldt);
	}
}

int
_ksd_getprivate(struct ksd *ksd, void **base, int *size)
{

	if ((ksd == NULL) || ((ksd->flags & KSDF_INITIALIZED) == 0))
		return (-1);
	else {
		*base = ksd->base;
		*size = ksd->size;
		return (0);
	}
}

/*
 * This assumes that the LDT is already setup.  Just set %gs to
 * reference it.
 */
int
_ksd_setprivate(struct ksd *ksd)
{
	int val;
	int ret;

	if ((ksd->flags & KSDF_INITIALIZED) == 0)
		ret = -1;
	else {
		val = (ksd->ldt << 3) | 7;
		__asm __volatile("movl %0, %%gs" : : "r" (val));
		ret = 0;
	}
	return (ret);
}
