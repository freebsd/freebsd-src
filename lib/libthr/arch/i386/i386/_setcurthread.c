/*
 * Copyright (c) 2003, Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/ucontext.h>

#include <stdio.h>

#include <machine/sysarch.h>
#include <machine/segments.h>

#define	MAXTHR	128

#define	LDT_INDEX(x)	(((long)(x) - (long)ldt_entries) / sizeof(ldt_entries[0]))

void **ldt_free = NULL;
static int ldt_inited = 0;
void *ldt_entries[MAXTHR];

static void ldt_init(void);

static void
ldt_init(void)
{
	int i;

	ldt_free = &ldt_entries[NLDT];

	for (i = 0; i < MAXTHR - 1; i++)
		ldt_entries[i] = (void *)&ldt_entries[i + 1];

	ldt_entries[MAXTHR - 1] = NULL;

	ldt_inited = 1;
}

void
_retire_thread(void *entry)
{
	if (ldt_free == NULL)
		*(void **)entry = NULL;
	else
		*(void **)entry = *ldt_free;
	ldt_free = entry;
}

void *
_set_curthread(ucontext_t *uc, void *thr)
{
	union descriptor desc;
	void **ldt_entry;
	int ldt_index;
	int error;

	if (ldt_inited == NULL)
		ldt_init();

	if (ldt_free == NULL)
		abort();

	/*
	 * Pull one off of the free list and update the free list pointer.
	 */
	ldt_entry = ldt_free;
	ldt_free = (void **)*ldt_entry;

	/*
	 * Cache the address of the thread structure here.  This is
	 * what the gs register will point to.
	 */
	*ldt_entry = thr;
	ldt_index = LDT_INDEX(ldt_entry);

	bzero(&desc, sizeof(desc));

	/*
	 * Set up the descriptor to point into the ldt table which contains
	 * only a pointer to the thread.
	 */
	desc.sd.sd_lolimit = sizeof(*ldt_entry);
	desc.sd.sd_lobase = (unsigned int)ldt_entry & 0xFFFFFF;
	desc.sd.sd_type = SDT_MEMRO;
	desc.sd.sd_dpl = SEL_UPL;
	desc.sd.sd_p = 1;
	desc.sd.sd_hilimit = 0;
	desc.sd.sd_xx = 0;
	desc.sd.sd_def32 = 1;
	desc.sd.sd_gran = 0;
	desc.sd.sd_hibase = (unsigned int)ldt_entry >> 24;

	error = i386_set_ldt(ldt_index, &desc, 1);
	if (error == -1)
		abort(); 

	/*
	 * Set up our gs with the index into the ldt for this entry.
	 */
	if (uc != NULL)
		uc->uc_mcontext.mc_gs = LSEL(ldt_index, SEL_UPL);
	else
		_set_gs(LSEL(ldt_index, SEL_UPL));

	return (ldt_entry);
}
