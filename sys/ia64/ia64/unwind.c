/*-
 * Copyright (c) 2001 Doug Rabson
 * All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <machine/frame.h>
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

static void *
unw_alloc(size_t sz)
{

	return (malloc(sz, M_UNWIND, M_WAITOK));
}

static void
unw_free(void *p)
{

	free(p, M_UNWIND);
}

#if 0
static struct unw_entry *
unw_entry_lookup(struct unw_table *ut, uint64_t ip)
{
	struct unw_entry *end, *mid, *start;

	ip -= ut->ut_base;
	start = ut->ut_start;
	end = ut->ut_end - 1;
	while (start < end) {
		mid = start + ((end - start) >> 1);
		if (ip < mid->ue_start)
			end = mid;
		else if (ip >= mid->ue_end)
			start = mid + 1;
		else
			break;
	}
	return ((start < end) ? mid : NULL);
}
#endif

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

static int
unw_cb_copyin(int req, char *to, uint64_t from, int len, intptr_t tok)
{
	struct unw_regstate *rs = (void*)tok;
	int reg;

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
		if (from == UWX_REG_PFS)
			from = rs->frame->tf_special.pfs;
		else if (from == UWX_REG_PREDS)
			from = rs->frame->tf_special.pr;
		else if (from == UWX_REG_RNAT)
			from = rs->frame->tf_special.rnat;
		else if (from == UWX_REG_UNAT)
			from = rs->frame->tf_special.unat;
		else if (from >= UWX_REG_GR(0) && from <= UWX_REG_GR(127)) {
			reg = from - UWX_REG_GR(0);
			if (reg == 1)
				from = rs->frame->tf_special.gp;
			else if (reg == 12)
				from = rs->frame->tf_special.sp;
			else if (reg == 13)
				from = rs->frame->tf_special.tp;
			else if (reg >= 2 && reg <= 3)
				from = (&rs->frame->tf_scratch.gr2)[reg - 2];
			else if (reg >= 8 && reg <= 11)
				from = (&rs->frame->tf_scratch.gr8)[reg - 8];
			else if (reg >= 14 && reg <= 31)
				from = (&rs->frame->tf_scratch.gr14)[reg - 14];
			else
				goto oops;
		} else if (from >= UWX_REG_BR(0) && from <= UWX_REG_BR(7)) {
			reg = from - UWX_REG_BR(0);
			if (reg == 0)
				from = rs->frame->tf_special.rp;
			else if (reg >= 6 && reg <= 7)
				from = (&rs->frame->tf_scratch.br6)[reg - 6];
			else
				goto oops;
		} else
			goto oops;

		*((uint64_t*)to) = from;
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
unw_create(struct unw_regstate *rs, struct trapframe *tf)
{
	struct unw_table *ut;
	uint64_t bsp;
	int nats, sof, uwxerr;

	ut = unw_table_lookup(tf->tf_special.iip);
	if (ut == NULL)
		return (ENOENT);

	rs->frame = tf;
	rs->env = uwx_init();
	if (rs->env == NULL)
		return (ENOMEM);

	uwxerr = uwx_register_callbacks(rs->env, (intptr_t)rs,
	    unw_cb_copyin, unw_cb_lookup);
	if (uwxerr)
		return (EINVAL);		/* XXX */

	bsp = tf->tf_special.bspstore + tf->tf_special.ndirty;
	sof = (int)(tf->tf_special.cfm & 0x7f);
	nats = (sof + 63 - ((int)(bsp >> 3) & 0x3f)) / 63;
	uwxerr = uwx_init_context(rs->env, tf->tf_special.iip,
	    tf->tf_special.sp, bsp - ((sof + nats) << 3), tf->tf_special.cfm);

	return ((uwxerr) ? EINVAL : 0);		/* XXX */
}

int
unw_step(struct unw_regstate *rs)
{
	int uwxerr;

	uwxerr = uwx_step(rs->env);
	return ((uwxerr) ? EINVAL : 0);		/* XXX */
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
unw_table_add(uint64_t base, uint64_t start, uint64_t end)
{
	struct unw_table *ut;

	ut = malloc(sizeof(struct unw_table), M_UNWIND, M_NOWAIT);
	if (ut == NULL)
		return (ENOMEM);

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
}
SYSINIT(unwind, SI_SUB_KMEM, SI_ORDER_ANY, unw_initialize, 0);
