/*-
 * Copyright (c) 2003, 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/ia64/ia64/unwind.c,v 1.14 2005/04/16 05:38:59 marcel Exp $");

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/unwind.h>

#include <uwx.h>

MALLOC_DEFINE(M_UNWIND, "Unwind", "Unwind information");

struct unw_entry {
	uint64_t	ue_start;	/* procedure start */
	uint64_t	ue_end;		/* procedure end */
	uint64_t	ue_info;	/* offset to procedure descriptors */
};

struct unw_table {
	LIST_ENTRY(unw_table) ut_link;
	uint64_t	ut_base;
	uint64_t	ut_limit;
	struct unw_entry *ut_start;
	struct unw_entry *ut_end;
};

LIST_HEAD(unw_table_list, unw_table);

static struct unw_table_list unw_tables;

#ifdef KDB
#define	KDBHEAPSZ	8192

struct mhdr {
	uint32_t	sig;
#define	MSIG_FREE	0x65657246	/* "Free". */
#define	MSIG_USED	0x64657355	/* "Used". */
	uint32_t	size;
	int32_t		next;
	int32_t		prev;
};

static struct mhdr *kdbheap;
#endif /* KDB */

static void *
unw_alloc(size_t sz)
{
#ifdef KDB
	struct mhdr *hdr, *hfree;

	if (kdb_active) {
		sz = (sz + 15) >> 4;
		hdr = kdbheap;
		while (hdr->sig != MSIG_FREE || hdr->size < sz) {
			if (hdr->next == -1)
				return (NULL);
			hdr = kdbheap + hdr->next;
		}
		if (hdr->size > sz + 1) {
			hfree = hdr + sz + 1;
			hfree->sig = MSIG_FREE;
			hfree->size = hdr->size - sz - 1;
			hfree->prev = hdr - kdbheap;
			hfree->next = hdr->next;
			hdr->size = sz;
			hdr->next = hfree - kdbheap;
			if (hfree->next >= 0) {
				hfree = kdbheap + hfree->next;
				hfree->prev = hdr->next;
			}
		}
		hdr->sig = MSIG_USED;
		return (void*)(hdr + 1);
	}
#endif
	return (malloc(sz, M_UNWIND, M_NOWAIT));
}

static void
unw_free(void *p)
{
#ifdef KDB
	struct mhdr *hdr, *hfree;

	if (kdb_active) {
		hdr = (struct mhdr*)p - 1;
		if (hdr->sig != MSIG_USED)
			return;
		hdr->sig = MSIG_FREE;
		if (hdr->prev >= 0 && kdbheap[hdr->prev].sig == MSIG_FREE) {
			hfree = kdbheap + hdr->prev;
			hfree->size += hdr->size + 1;
			hfree->next = hdr->next;
			if (hdr->next >= 0) {
				hfree = kdbheap + hdr->next;
				hfree->prev = hdr->prev;
			}
		} else if (hdr->next >= 0 &&
		    kdbheap[hdr->next].sig == MSIG_FREE) {
			hfree = kdbheap + hdr->next;
			hdr->size += hfree->size + 1;
			hdr->next = hfree->next;
			if (hdr->next >= 0) {
				hfree = kdbheap + hdr->next;
				hfree->prev = hdr - kdbheap;
			}
		}
		return;
	}
#endif
	free(p, M_UNWIND);
}

static struct unw_table *
unw_table_lookup(uint64_t ip)
{
	struct unw_table *ut;

	LIST_FOREACH(ut, &unw_tables, ut_link) {
		if (ip >= ut->ut_base && ip < ut->ut_limit)
			return (ut);
	}
	return (NULL);
}

static uint64_t
unw_copyin_from_frame(struct trapframe *tf, uint64_t from)
{
	uint64_t val;
	int reg;

	if (from == UWX_REG_AR_PFS)
		val = tf->tf_special.pfs;
	else if (from == UWX_REG_PREDS)
		val = tf->tf_special.pr;
	else if (from == UWX_REG_AR_RNAT)
		val = tf->tf_special.rnat;
	else if (from == UWX_REG_AR_UNAT)
		val = tf->tf_special.unat;
	else if (from >= UWX_REG_GR(0) && from <= UWX_REG_GR(127)) {
		reg = from - UWX_REG_GR(0);
		if (reg == 1)
			val = tf->tf_special.gp;
		else if (reg == 12)
			val = tf->tf_special.sp;
		else if (reg == 13)
			val = tf->tf_special.tp;
		else if (reg >= 2 && reg <= 3)
			val = (&tf->tf_scratch.gr2)[reg - 2];
		else if (reg >= 8 && reg <= 11)
			val = (&tf->tf_scratch.gr8)[reg - 8];
		else if (reg >= 14 && reg <= 31)
			val = (&tf->tf_scratch.gr14)[reg - 14];
		else
			goto oops;
	} else if (from >= UWX_REG_BR(0) && from <= UWX_REG_BR(7)) {
		reg = from - UWX_REG_BR(0);
		if (reg == 0)
			val = tf->tf_special.rp;
		else if (reg >= 6 && reg <= 7)
			val = (&tf->tf_scratch.br6)[reg - 6];
		else
			goto oops;
	} else
		goto oops;
	return (val);

 oops:
	printf("UNW: %s(%p, %lx)\n", __func__, tf, from);
	return (0UL);
}

static uint64_t
unw_copyin_from_pcb(struct pcb *pcb, uint64_t from)
{
	uint64_t val;
	int reg;

	if (from == UWX_REG_AR_PFS)
		val = pcb->pcb_special.pfs;
	else if (from == UWX_REG_PREDS)
		val = pcb->pcb_special.pr;
	else if (from == UWX_REG_AR_RNAT)
		val = pcb->pcb_special.rnat;
	else if (from == UWX_REG_AR_UNAT)
		val = pcb->pcb_special.unat;
	else if (from >= UWX_REG_GR(0) && from <= UWX_REG_GR(127)) {
		reg = from - UWX_REG_GR(0);
		if (reg == 1)
			val = pcb->pcb_special.gp;
		else if (reg == 12)
			val = pcb->pcb_special.sp;
		else if (reg == 13)
			val = pcb->pcb_special.tp;
		else if (reg >= 4 && reg <= 7)
			val = (&pcb->pcb_preserved.gr4)[reg - 4];
		else
			goto oops;
	} else if (from >= UWX_REG_BR(0) && from <= UWX_REG_BR(7)) {
		reg = from - UWX_REG_BR(0);
		if (reg == 0)
			val = pcb->pcb_special.rp;
		else if (reg >= 1 && reg <= 5)
			val = (&pcb->pcb_preserved.br1)[reg - 1];
		else
			goto oops;
	} else
		goto oops;
	return (val);

 oops:
	printf("UNW: %s(%p, %lx)\n", __func__, pcb, from);
	return (0UL);
}

static int
unw_cb_copyin(int req, char *to, uint64_t from, int len, intptr_t tok)
{
	struct unw_regstate *rs = (void*)tok;
	uint64_t val;

	switch (req) {
	case UWX_COPYIN_UINFO:
		break;
	case UWX_COPYIN_MSTACK:
		*((uint64_t*)to) = *((uint64_t*)from);
		return (8);
	case UWX_COPYIN_RSTACK:
		*((uint64_t*)to) = *((uint64_t*)from);
		return (8);
	case UWX_COPYIN_REG:
		if (rs->frame != NULL)
			val = unw_copyin_from_frame(rs->frame, from);
		else if (rs->pcb != NULL)
			val = unw_copyin_from_pcb(rs->pcb, from);
		else
			goto oops;
		*((uint64_t*)to) = val;
		return (len);
	}

 oops:
	printf("UNW: %s(%d, %p, %lx, %d, %lx)\n", __func__, req, to, from,
	    len, tok);
	return (0);
}

static int
unw_cb_lookup(int req, uint64_t ip, intptr_t tok, uint64_t **vec)
{
	struct unw_regstate *rs = (void*)tok;
	struct unw_table *ut;

	switch (req) {
	case UWX_LKUP_LOOKUP:
		ut = unw_table_lookup(ip);
		if (ut == NULL)
			return (UWX_LKUP_NOTFOUND);
		rs->keyval[0] = UWX_KEY_TBASE;
		rs->keyval[1] = ut->ut_base;
		rs->keyval[2] = UWX_KEY_USTART;
		rs->keyval[3] = (intptr_t)ut->ut_start;
		rs->keyval[4] = UWX_KEY_UEND;
		rs->keyval[5] = (intptr_t)ut->ut_end;
		rs->keyval[6] = 0;
		rs->keyval[7] = 0;
		*vec = rs->keyval;
		return (UWX_LKUP_UTABLE);
	case UWX_LKUP_FREE:
		return (0);
	}

	return (UWX_LKUP_ERR);
}

int
unw_create_from_frame(struct unw_regstate *rs, struct trapframe *tf)
{
	uint64_t bsp, ip;
	int uwxerr;

	rs->frame = tf;
	rs->pcb = NULL;
	rs->env = uwx_init();
	if (rs->env == NULL)
		return (ENOMEM);

	uwxerr = uwx_register_callbacks(rs->env, (intptr_t)rs,
	    unw_cb_copyin, unw_cb_lookup);
	if (uwxerr)
		return (EINVAL);		/* XXX */

	bsp = tf->tf_special.bspstore + tf->tf_special.ndirty;
	bsp = ia64_bsp_adjust(bsp, -IA64_CFM_SOF(tf->tf_special.cfm));
	ip = tf->tf_special.iip + ((tf->tf_special.psr >> 41) & 3);

	uwxerr = uwx_init_context(rs->env, ip, tf->tf_special.sp, bsp,
	    tf->tf_special.cfm);

	return ((uwxerr) ? EINVAL : 0);		/* XXX */
}

int
unw_create_from_pcb(struct unw_regstate *rs, struct pcb *pcb)
{
	uint64_t bsp, cfm, ip;
	int uwxerr;

	rs->frame = NULL;
	rs->pcb = pcb;
	rs->env = uwx_init();
	if (rs->env == NULL)
		return (ENOMEM);

	uwxerr = uwx_register_callbacks(rs->env, (intptr_t)rs,
	    unw_cb_copyin, unw_cb_lookup);
	if (uwxerr)
		return (EINVAL);		/* XXX */

	bsp = pcb->pcb_special.bspstore;
	if (pcb->pcb_special.__spare == ~0UL) {
		ip = pcb->pcb_special.iip + ((pcb->pcb_special.psr >> 41) & 3);
		cfm = pcb->pcb_special.cfm;
		bsp += pcb->pcb_special.ndirty;
		bsp = ia64_bsp_adjust(bsp, -IA64_CFM_SOF(cfm));
	} else {
		ip = pcb->pcb_special.rp;
		cfm = pcb->pcb_special.pfs;
		bsp = ia64_bsp_adjust(bsp, -IA64_CFM_SOL(cfm));
	}
	uwxerr = uwx_init_context(rs->env, ip, pcb->pcb_special.sp, bsp, cfm);

	return ((uwxerr) ? EINVAL : 0);		/* XXX */
}

void
unw_delete(struct unw_regstate *rs)
{

	if (rs->env != NULL)
		uwx_free(rs->env);
}

int
unw_step(struct unw_regstate *rs)
{
	int err;

	switch (uwx_step(rs->env)) {
	case UWX_ABI_FRAME:
		err = ERESTART;
		break;
	case UWX_BOTTOM:
		err = EJUSTRETURN;
		break;
	case UWX_OK:
		err = 0;
		break;
	default:
		err = EINVAL;		/* XXX */
		break;
	}
	return (err);
}

int
unw_get_bsp(struct unw_regstate *s, uint64_t *r)
{
	int uwxerr;

	uwxerr = uwx_get_reg(s->env, UWX_REG_BSP, r);
	return ((uwxerr) ? EINVAL : 0); 	/* XXX */
}

int
unw_get_cfm(struct unw_regstate *s, uint64_t *r)
{
	int uwxerr;

	uwxerr = uwx_get_reg(s->env, UWX_REG_CFM, r);
	return ((uwxerr) ? EINVAL : 0); 	/* XXX */
}

int
unw_get_ip(struct unw_regstate *s, uint64_t *r)
{
	int uwxerr;

	uwxerr = uwx_get_reg(s->env, UWX_REG_IP, r);
	return ((uwxerr) ? EINVAL : 0); 	/* XXX */
}

int
unw_get_sp(struct unw_regstate *s, uint64_t *r)
{
	int uwxerr;

	uwxerr = uwx_get_reg(s->env, UWX_REG_SP, r);
	return ((uwxerr) ? EINVAL : 0); 	/* XXX */
}

int
unw_table_add(uint64_t base, uint64_t start, uint64_t end)
{
	struct unw_table *ut;

	ut = malloc(sizeof(struct unw_table), M_UNWIND, M_WAITOK);
	ut->ut_base = base;
	ut->ut_start = (struct unw_entry*)start;
	ut->ut_end = (struct unw_entry*)end;
	ut->ut_limit = base + ut->ut_end[-1].ue_end;
	LIST_INSERT_HEAD(&unw_tables, ut, ut_link);

	if (bootverbose)
		printf("UNWIND: table added: base=%lx, start=%lx, end=%lx\n",
		    base, start, end);

	return (0);
}

void
unw_table_remove(uint64_t base)
{
	struct unw_table *ut;

	ut = unw_table_lookup(base);
	if (ut != NULL) {
		LIST_REMOVE(ut, ut_link);
		free(ut, M_UNWIND);
		if (bootverbose)
			printf("UNWIND: table removed: base=%lx\n", base);
	}
}

static void
unw_initialize(void *dummy __unused)
{

	LIST_INIT(&unw_tables);
	uwx_register_alloc_cb(unw_alloc, unw_free);
#ifdef KDB
	kdbheap = malloc(KDBHEAPSZ, M_UNWIND, M_WAITOK);
	kdbheap->sig = MSIG_FREE;
	kdbheap->size = (KDBHEAPSZ - sizeof(struct mhdr)) >> 4;
	kdbheap->next = -1;
	kdbheap->prev = -1;
#endif
}
SYSINIT(unwind, SI_SUB_KMEM, SI_ORDER_ANY, unw_initialize, 0);
