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
#include <machine/rse.h>

#ifdef UNWIND_DEBUG
#define DPF(x)	printf x
#else
#define DPF(x)
#endif

MALLOC_DEFINE(M_UNWIND, "Unwind table", "Unwind table information");

struct ia64_unwind_table_entry {
	u_int64_t	ue_start;	/* procedure start */
	u_int64_t	ue_end;		/* procedure end */
	u_int64_t	ue_info;	/* offset to procedure descriptors */
};

struct ia64_unwind_info {
	u_int64_t	ui_length	: 32; /* length in 64bit units */
	u_int64_t	ui_flags	: 16;
	u_int64_t	ui_version	: 16;
};

LIST_HEAD(ia64_unwind_table_list, ia64_unwind_table);

struct ia64_unwind_table {
	LIST_ENTRY(ia64_unwind_table) ut_link;
	u_int64_t	ut_base;
	u_int64_t	ut_limit;
	struct ia64_unwind_table_entry *ut_start;
	struct ia64_unwind_table_entry *ut_end;
};

struct unwind_reg {
	u_int64_t	ur_value;	/* current value */
	u_int64_t	*ur_save;	/* save location */
	int		ur_when;	/* when the save happened */
};

struct unwind_fpreg {
	struct ia64_fpreg ur_value;	/* current value */
	struct ia64_fpreg *ur_save;	/* save location */
	int		ur_when;	/* when the save happened */
};

struct ia64_unwind_state {
	LIST_ENTRY(ia64_unwind_state) us_link; /* free list */

	/*
	 * Current register state and location of saved register state
	 */
	struct register_state {
		struct unwind_reg rs_psp;
		struct unwind_reg rs_pfs;
		struct unwind_reg rs_preds;
		struct unwind_reg rs_unat;
		struct unwind_reg rs_lc;
		struct unwind_reg rs_rnat;
		struct unwind_reg rs_bsp;
		struct unwind_reg rs_bspstore;
		struct unwind_reg rs_fpsr;
		struct unwind_reg rs_priunat;
		struct unwind_reg rs_br[8];
		struct unwind_reg rs_gr[32];
		struct unwind_fpreg rs_fr[32];
		u_int64_t	rs_stack_size;
	}		us_regs;

	/*
	 * Variables used while parsing unwind records.
	 */
	u_int64_t	us_ip;		/* value of IP for this frame */
	int		us_ri;		/* RI field from cr.ipsr */
	u_int64_t	us_pc;		/* slot offset in procedure */
	u_int64_t	*us_bsp;	/* backing store for frame */
	u_int64_t	us_cfm;		/* CFM value for frame */
	u_int64_t	*us_spill;	/* spill_base location */
	int		us_spilloff;	/* offset into spill area */
	int		us_grmask;	/* mask of grs being spilled */
	int		us_frmask;	/* mask of frs being spilled */
	int		us_brmask;	/* mask of brs being spilled */
};

static int ia64_unwind_initialised;
static struct ia64_unwind_table_list ia64_unwind_tables;
#define MAX_UNWIND_STATES 4
static struct ia64_unwind_state ia64_unwind_state_static[MAX_UNWIND_STATES];
static LIST_HEAD(ia64_unwind_state_list, ia64_unwind_state) ia64_unwind_states;

static void
ia64_initialise_unwind(void *arg __unused)
{
	int i;

	KASSERT(!ia64_unwind_initialised, ("foo"));

	LIST_INIT(&ia64_unwind_tables);
	LIST_INIT(&ia64_unwind_states);
	for (i = 0; i < MAX_UNWIND_STATES; i++) {
		LIST_INSERT_HEAD(&ia64_unwind_states,
		    &ia64_unwind_state_static[i], us_link);
	}

	ia64_unwind_initialised = 1;
}
SYSINIT(unwind, SI_SUB_KMEM, SI_ORDER_ANY, ia64_initialise_unwind, 0);

static struct ia64_unwind_table *
find_table(u_int64_t ip)
{
	struct ia64_unwind_table *ut;

	LIST_FOREACH(ut, &ia64_unwind_tables, ut_link) {
		if (ip >= ut->ut_base && ip < ut->ut_limit)
			return ut;
	}
	return 0;
}

static struct ia64_unwind_table_entry *
find_entry(struct ia64_unwind_table *ut, u_int64_t ip)
{
	struct ia64_unwind_table_entry *start;
	struct ia64_unwind_table_entry *end;
	struct ia64_unwind_table_entry *mid;

	ip -= ut->ut_base;
	start = ut->ut_start;
	end = ut->ut_end - 1;
	while (start < end) {
		mid = start + (end - start) / 2;
		if (ip < mid->ue_start) {
			if (end == mid)
				break;
			end = mid;
		} else if (ip >= mid->ue_end) {
			if (start == mid)
				break;
			start = mid;
		} else
			return mid;
	}

	return 0;
}

int
ia64_add_unwind_table(vm_offset_t base, vm_offset_t start, vm_offset_t end)
{
	struct ia64_unwind_table *ut;

	KASSERT(ia64_unwind_initialised, ("foo"));

	ut = malloc(sizeof(struct ia64_unwind_table), M_UNWIND, M_NOWAIT);
	if (ut == NULL)
		return (ENOMEM);

	ut->ut_base = base;
	ut->ut_start = (struct ia64_unwind_table_entry*)start;
	ut->ut_end = (struct ia64_unwind_table_entry*)end;
	ut->ut_limit = base + ut->ut_end[-1].ue_end;
	LIST_INSERT_HEAD(&ia64_unwind_tables, ut, ut_link);

	if (bootverbose)
		printf("UNWIND: table added: base=%lx, start=%lx, end=%lx\n",
		    base, start, end);

	return (0);
}

void
ia64_delete_unwind_table(vm_offset_t base)
{
	struct ia64_unwind_table *ut;

	KASSERT(ia64_unwind_initialised, ("foo"));

	ut = find_table(base);
	if (ut != NULL) {
		LIST_REMOVE(ut, ut_link);
		free(ut, M_UNWIND);
		if (bootverbose)
			printf("UNWIND: table removed: base=%lx\n", base);
	}
}

struct ia64_unwind_state *
ia64_create_unwind_state(struct trapframe *framep)
{
	struct ia64_unwind_state *us;
	int i;

	if (!ia64_unwind_initialised)
		return 0;

	us = LIST_FIRST(&ia64_unwind_states);
	if (us) {
		LIST_REMOVE(us, us_link);
	} else {
		us = malloc(sizeof(struct ia64_unwind_state),
			    M_UNWIND, M_NOWAIT);
		if (!us)
			return 0;
	}

	bzero(us, sizeof(*us));
	us->us_regs.rs_psp.ur_value = framep->tf_r[FRAME_SP];
	us->us_regs.rs_pfs.ur_value = framep->tf_ar_pfs;
	us->us_regs.rs_preds.ur_value = framep->tf_pr;
	us->us_regs.rs_unat.ur_value = framep->tf_ar_unat;
	us->us_regs.rs_rnat.ur_value = framep->tf_ar_rnat;
	us->us_regs.rs_bsp.ur_value =
		(u_int64_t) (framep->tf_ar_bspstore + framep->tf_ndirty);
	us->us_regs.rs_bspstore.ur_value = framep->tf_ar_bspstore;
	us->us_regs.rs_fpsr.ur_value = framep->tf_ar_fpsr;
	for (i = 0; i < 8; i++) {
		us->us_regs.rs_br[i].ur_value = framep->tf_b[i];
	}
	us->us_regs.rs_gr[0].ur_value = 0;
	for (i = 1; i < 32; i++) {
		us->us_regs.rs_gr[i].ur_value = framep->tf_r[i-1];
	}
	for (i = 6; i < 16; i++) {
		us->us_regs.rs_fr[i].ur_value = framep->tf_f[i-6];
	}

	us->us_ip = framep->tf_cr_iip;
	us->us_ri = (framep->tf_cr_ipsr & IA64_PSR_RI) >> 41;
	us->us_spill = (u_int64_t *) us->us_regs.rs_gr[12].ur_value;
	us->us_cfm = framep->tf_cr_ifs;
	us->us_bsp = ia64_rse_previous_frame
		((u_int64_t *) us->us_regs.rs_bsp.ur_value, us->us_cfm & 0x7f);

	return us;
}

void
ia64_free_unwind_state(struct ia64_unwind_state *us)
{
	LIST_INSERT_HEAD(&ia64_unwind_states, us, us_link);
}

u_int64_t
ia64_unwind_state_get_ip(struct ia64_unwind_state *us)
{
	return us->us_ip + us->us_ri;
}

u_int64_t
ia64_unwind_state_get_sp(struct ia64_unwind_state *us)
{
	return us->us_regs.rs_gr[12].ur_value;
}

u_int64_t
ia64_unwind_state_get_cfm(struct ia64_unwind_state *us)
{
	return us->us_cfm;
}

u_int64_t *
ia64_unwind_state_get_bsp(struct ia64_unwind_state *us)
{
	return us->us_bsp;
}

static u_int64_t
read_uleb128(u_int8_t **pp)
{
	u_int8_t *p = *pp;
	u_int8_t b;
	u_int64_t res;

	res = 0;
	do {
		b = *p++;
		res = (res << 7) | (b & 0x7f);
	} while (b & (1 << 7));

	*pp = p;
	return res;
}

#define PROCESS_WHEN(us, reg, t)				\
do {								\
	DPF(("register %s was saved at offset %d\n",		\
	     #reg, t));						\
	us->us_regs.rs_##reg.ur_when = t;			\
} while (0)

#define PROCESS_GR(us, reg, gr)					\
do {								\
	DPF(("save location for %s at r%d\n", #reg, gr));	\
	us->us_regs.rs_##reg.ur_save = find_gr(us, gr);		\
} while (0)							\

#define PROCESS_BR(us, reg, br)					\
do {								\
	DPF(("save location for %s at b%d\n", #reg, br));	\
	us->us_regs.rs_##reg.ur_save =				\
		&us->us_regs.rs_br[br].ur_value;		\
} while (0)

#define PROCESS_GRMEM(us, reg)					\
do {								\
	DPF(("save location for %s at spill+%d\n",		\
	     #reg, us->us_spilloff));				\
	us->us_regs.rs_##reg.ur_save =				\
		&us->us_spill[us->us_spilloff];			\
	us->us_spilloff += 8;					\
} while (0)

#define PROCESS_FRMEM(us, reg)					\
do {								\
	DPF(("save location for %s at spill+%d\n",		\
	     #reg, us->us_spilloff));				\
	us->us_regs.rs_##reg.ur_save =				\
		(struct ia64_fpreg *)				\
		&us->us_spill[us->us_spilloff];			\
	us->us_spilloff += 16;					\
} while (0)

#define PROCESS_SPREL(us, reg, spoff)				\
do {								\
	DPF(("save location for %s at sp+%d\n",			\
	     #reg, 4*spoff));					\
	us->us_regs.rs_##reg.ur_save = (u_int64_t *)		\
		(us->us_regs.rs_gr[12].ur_value + 4*spoff);	\
} while (0)

#define PROCESS_SPREL_WHEN(us, reg, spoff, t)			\
do {								\
	PROCESS_SPREL(us, reg, spoff);				\
	PROCESS_WHEN(us, reg, t);				\
} while (0)

#define PROCESS_PSPREL(us, reg, pspoff)				\
do {								\
	DPF(("save location for %s at psp+%d\n",		\
	     #reg, 16-4*pspoff));				\
	us->us_regs.rs_##reg.ur_save = (u_int64_t *)		\
		(us->us_regs.rs_psp.ur_value + 16-4*pspoff);	\
} while (0)

#define PROCESS_PSPREL_WHEN(us, reg, pspoff, t)			\
do {								\
	PROCESS_PSPREL(us, reg, pspoff);			\
	PROCESS_WHEN(us, reg, t);				\
} while (0)

static u_int64_t *
find_gr(struct ia64_unwind_state *us, int gr)
{
	if (gr < 32)
		return &us->us_regs.rs_gr[gr].ur_value;
	else
		return ia64_rse_register_address(us->us_bsp, gr);
}

static void
parse_prologue(struct ia64_unwind_state *us, int rlen)
{
}

static void
parse_prologue_gr(struct ia64_unwind_state *us, int rlen,
		  int mask, int grsave)
{
	if (mask & 8) {
		PROCESS_GR(us, br[0], grsave);
		grsave++;
	}
	if (mask & 4) {
		PROCESS_GR(us, pfs, grsave);
		grsave++;
	}
	if (mask & 2) {
		PROCESS_GR(us, psp, grsave);
		grsave++;
	}
	if (mask & 1) {
		PROCESS_GR(us, preds, grsave);
		grsave++;
	}
}

static void
parse_mem_stack_f(struct ia64_unwind_state *us, int t, int size)
{
	DPF(("restore value for psp is sp+%d at offset %d\n",
	     16*size, t));
	us->us_regs.rs_psp.ur_when = t;
	us->us_regs.rs_stack_size = 16*size;
}

static void
parse_mem_stack_v(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, psp, t);
}

static void
parse_psp_gr(struct ia64_unwind_state *us, int gr)
{
	PROCESS_GR(us, psp, gr);
}

static void
parse_psp_sprel(struct ia64_unwind_state *us, int spoff)
{
	PROCESS_SPREL(us, psp, spoff);
}

static void
parse_rp_when(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, br[0], t);
}

static void
parse_rp_gr(struct ia64_unwind_state *us, int gr)
{
	PROCESS_GR(us, br[0], gr);
}

static void
parse_rp_br(struct ia64_unwind_state *us, int br)
{
	PROCESS_BR(us, br[0], br);
}

static void
parse_rp_psprel(struct ia64_unwind_state *us, int pspoff)
{
	PROCESS_PSPREL(us, br[0], pspoff);
}

static void
parse_rp_sprel(struct ia64_unwind_state *us, int spoff)
{
	PROCESS_SPREL(us, br[0], spoff);
}

static void
parse_pfs_when(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, pfs, t);
}

static void
parse_pfs_gr(struct ia64_unwind_state *us, int gr)
{
	PROCESS_GR(us, pfs, gr);
}

static void
parse_pfs_psprel(struct ia64_unwind_state *us, int pspoff)
{
	PROCESS_PSPREL(us, pfs, pspoff);
}

static void
parse_pfs_sprel(struct ia64_unwind_state *us, int spoff)
{
	PROCESS_SPREL(us, pfs, spoff);
}

static void
parse_preds_when(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, preds, t);
}

static void
parse_preds_gr(struct ia64_unwind_state *us, int gr)
{
	PROCESS_GR(us, preds, gr);
}

static void
parse_preds_psprel(struct ia64_unwind_state *us, int pspoff)
{
	PROCESS_PSPREL(us, preds, pspoff);
}

static void
parse_preds_sprel(struct ia64_unwind_state *us, int spoff)
{
	PROCESS_SPREL(us, preds, spoff);
}

static void
parse_fr_mem(struct ia64_unwind_state *us, int frmask)
{
	us->us_frmask = frmask;
}

static void
parse_frgr_mem(struct ia64_unwind_state *us, int grmask, int frmask)
{
	us->us_grmask = grmask;
	if (grmask & 1)
		PROCESS_GRMEM(us, gr[4]);
	if (grmask & 2)
		PROCESS_GRMEM(us, gr[5]);
	if (grmask & 4)
		PROCESS_GRMEM(us, gr[6]);
	if (grmask & 8)
		PROCESS_GRMEM(us, gr[7]);

	us->us_frmask = frmask;
	if (frmask & 1)
		PROCESS_FRMEM(us, fr[2]);
	if (frmask & 2)
		PROCESS_FRMEM(us, fr[3]);
	if (frmask & 4)
		PROCESS_FRMEM(us, fr[4]);
	if (frmask & 8)
		PROCESS_FRMEM(us, fr[5]);
	if (frmask & 16)
		PROCESS_FRMEM(us, fr[16]);
	if (frmask & 32)
		PROCESS_FRMEM(us, fr[17]);
	if (frmask & 64)
		PROCESS_FRMEM(us, fr[18]);
	if (frmask & 128)
		PROCESS_FRMEM(us, fr[19]);
	if (frmask & 256)
		PROCESS_FRMEM(us, fr[20]);
	if (frmask & 512)
		PROCESS_FRMEM(us, fr[21]);
	if (frmask & 1024)
		PROCESS_FRMEM(us, fr[22]);
	if (frmask & 2048)
		PROCESS_FRMEM(us, fr[24]);
	if (frmask & 4096)
		PROCESS_FRMEM(us, fr[25]);
	if (frmask & 8192)
		PROCESS_FRMEM(us, fr[26]);
	if (frmask & 16384)
		PROCESS_FRMEM(us, fr[27]);
	if (frmask & 32768)
		PROCESS_FRMEM(us, fr[28]);
	if (frmask & 65536)
		PROCESS_FRMEM(us, fr[29]);
	if (frmask & 131072)
		PROCESS_FRMEM(us, fr[30]);
	if (frmask & 262144)
		PROCESS_FRMEM(us, fr[31]);
}

static void
parse_gr_gr(struct ia64_unwind_state *us, int grmask, int gr)
{
	us->us_grmask = grmask;
	if (grmask & 1) {
		PROCESS_GR(us, gr[4], gr);
		gr++;
	}
	if (grmask & 2) {
		PROCESS_GR(us, gr[5], gr);
		gr++;
	}
	if (grmask & 4) {
		PROCESS_GR(us, gr[6], gr);
		gr++;
	}
	if (grmask & 8) {
		PROCESS_GR(us, gr[7], gr);
		gr++;
	}
}

static void
parse_gr_mem(struct ia64_unwind_state *us, int grmask)
{
	us->us_grmask = grmask;
	if (grmask & 1)
		PROCESS_GRMEM(us, gr[4]);
	if (grmask & 2)
		PROCESS_GRMEM(us, gr[5]);
	if (grmask & 4)
		PROCESS_GRMEM(us, gr[6]);
	if (grmask & 8)
		PROCESS_GRMEM(us, gr[7]);
}

static void
parse_br_mem(struct ia64_unwind_state *us, int brmask)
{
	us->us_brmask = brmask;
	if (brmask & 1)
		PROCESS_GRMEM(us, br[1]);
	if (brmask & 2)
		PROCESS_GRMEM(us, br[2]);
	if (brmask & 4)
		PROCESS_GRMEM(us, br[3]);
	if (brmask & 8)
		PROCESS_GRMEM(us, br[4]);
	if (brmask & 16)
		PROCESS_GRMEM(us, br[5]);
}

static void
parse_br_gr(struct ia64_unwind_state *us, int brmask, int gr)
{
	us->us_brmask = brmask;
	if (brmask & 1) {
		PROCESS_GR(us, br[1], gr);
		gr++;
	}
	if (brmask & 2) {
		PROCESS_GR(us, br[2], gr);
		gr++;
	}
	if (brmask & 4) {
		PROCESS_GR(us, br[3], gr);
		gr++;
	}
	if (brmask & 8) {
		PROCESS_GR(us, br[4], gr);
		gr++;
	}
	if (brmask & 16) {
		PROCESS_GR(us, br[5], gr);
		gr++;
	}
}

static void
parse_spill_base(struct ia64_unwind_state *us, int pspoff)
{
	DPF(("base of spill area at psp+%d\n", 16 - 4*pspoff));
	us->us_spill = (u_int64_t *)
		(us->us_regs.rs_psp.ur_value + 16 - 4*pspoff);
}

static void
parse_spill_mask(struct ia64_unwind_state *us, int rlen, u_int8_t *imask)
{
	int i, reg;
	u_int8_t b;
	static int frno[] = {
		2, 3, 4, 5, 16, 17, 18, 19, 20, 21, 22,
		23, 24, 25, 26, 27, 28, 29, 30, 31
	};

	for (i = 0; i < rlen; i++) {
		b = imask[i / 4];
		b = (b >> (2 * (3-(i & 3)))) & 3;
		switch (b) {
		case 0:
			break;
		case 1:
			reg = frno[ffs(us->us_frmask) - 1];
			DPF(("restoring fr[%d] at offset %d\n", reg, i));
			us->us_regs.rs_fr[reg].ur_when = i;
			break;
		case 2:
			reg = ffs(us->us_grmask) - 1 + 4;
			DPF(("restoring gr[%d] at offset %d\n", reg, i));
			us->us_regs.rs_gr[reg].ur_when = i;
			break;
		case 3:
			reg = ffs(us->us_brmask) - 1 + 1;
			DPF(("restoring br[%d] at offset %d\n", reg, i));
			us->us_regs.rs_gr[reg].ur_when = i;
			break;
		}
	}
}

static void
parse_unat_when(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, unat, t);
}

static void
parse_unat_gr(struct ia64_unwind_state *us, int gr)
{
	PROCESS_GR(us, unat, gr);
}

static void
parse_unat_psprel(struct ia64_unwind_state *us, int pspoff)
{
	PROCESS_PSPREL(us, unat, pspoff);
}

static void
parse_unat_sprel(struct ia64_unwind_state *us, int spoff)
{
	PROCESS_SPREL(us, unat, spoff);
}

static void
parse_lc_when(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, lc, t);
}

static void
parse_lc_gr(struct ia64_unwind_state *us, int gr)
{
	PROCESS_GR(us, lc, gr);
}

static void
parse_lc_psprel(struct ia64_unwind_state *us, int pspoff)
{
	PROCESS_PSPREL(us, lc, pspoff);
}

static void
parse_lc_sprel(struct ia64_unwind_state *us, int spoff)
{
	PROCESS_SPREL(us, lc, spoff);
}

static void
parse_fpsr_when(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, fpsr, t);
}

static void
parse_fpsr_gr(struct ia64_unwind_state *us, int gr)
{
	PROCESS_GR(us, fpsr, gr);
}

static void
parse_fpsr_psprel(struct ia64_unwind_state *us, int pspoff)
{
	PROCESS_PSPREL(us, fpsr, pspoff);
}

static void
parse_fpsr_sprel(struct ia64_unwind_state *us, int spoff)
{
	PROCESS_SPREL(us, fpsr, spoff);
}

static void
parse_priunat_when_gr(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, priunat, t);
}

static void
parse_priunat_when_mem(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, priunat, t);
}

static void
parse_priunat_gr(struct ia64_unwind_state *us, int gr)
{
	PROCESS_GR(us, priunat, gr);
}

static void
parse_priunat_psprel(struct ia64_unwind_state *us, int pspoff)
{
	PROCESS_PSPREL(us, priunat, pspoff);
}

static void
parse_priunat_sprel(struct ia64_unwind_state *us, int spoff)
{
	PROCESS_SPREL(us, priunat, spoff);
}

static void
parse_bsp_when(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, bsp, t);
}

static void
parse_bsp_gr(struct ia64_unwind_state *us, int gr)
{
	PROCESS_GR(us, bsp, gr);
}

static void
parse_bsp_psprel(struct ia64_unwind_state *us, int pspoff)
{
	PROCESS_PSPREL(us, bsp, pspoff);
}

static void
parse_bsp_sprel(struct ia64_unwind_state *us, int spoff)
{
	PROCESS_SPREL(us, bsp, spoff);
}

static void
parse_bspstore_when(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, bspstore, t);
}

static void
parse_bspstore_gr(struct ia64_unwind_state *us, int gr)
{
	PROCESS_GR(us, bspstore, gr);
}

static void
parse_bspstore_psprel(struct ia64_unwind_state *us, int pspoff)
{
	PROCESS_PSPREL(us, bspstore, pspoff);
}

static void
parse_bspstore_sprel(struct ia64_unwind_state *us, int spoff)
{
	PROCESS_SPREL(us, bspstore, spoff);
}

static void
parse_rnat_when(struct ia64_unwind_state *us, int t)
{
	PROCESS_WHEN(us, rnat, t);
}

static void
parse_rnat_gr(struct ia64_unwind_state *us, int gr)
{
	PROCESS_GR(us, rnat, gr);
}

static void
parse_rnat_psprel(struct ia64_unwind_state *us, int pspoff)
{
	PROCESS_PSPREL(us, rnat, pspoff);
}

static void
parse_rnat_sprel(struct ia64_unwind_state *us, int spoff)
{
	PROCESS_SPREL(us, rnat, spoff);
}

static void
parse_epilogue(struct ia64_unwind_state *us, int t, int ecount)
{
}

static void
parse_label_state(struct ia64_unwind_state *us, int label)
{
}

static void
parse_copy_state(struct ia64_unwind_state *us, int label)
{
}

static void
parse_spill_psprel(struct ia64_unwind_state *us, int t,
		   int reg, int pspoff)
{
	int type;

	type = reg >> 5;
	reg &= 0x1f;
	switch (type) {
	case 0:
		DPF(("save location for gr[%d] at psp+%d at offset %d\n",
		       reg, 16-4*pspoff, t));
		us->us_regs.rs_gr[reg].ur_save = (u_int64_t *)
			(us->us_regs.rs_psp.ur_value + 16-4*pspoff);
		us->us_regs.rs_gr[reg].ur_when = t;
		break;
	case 1:
		DPF(("save location for fr[%d] at psp+%d at offset %d\n",
		       reg, 16-4*pspoff, t));
		us->us_regs.rs_fr[reg].ur_save = (struct ia64_fpreg *)
			(us->us_regs.rs_psp.ur_value + 16-4*pspoff);
		us->us_regs.rs_fr[reg].ur_when = t;
		break;
	case 2:
		DPF(("save location for br[%d] at psp+%d at offset %d\n",
		       reg, 16-4*pspoff, t));
		us->us_regs.rs_br[reg].ur_save = (u_int64_t *)
			(us->us_regs.rs_psp.ur_value + 16-4*pspoff);
		us->us_regs.rs_br[reg].ur_when = t;
		break;
	case 3:
		switch (reg) {
		case 0:
			PROCESS_PSPREL_WHEN(us, preds, pspoff, t);
			break;
		case 1:
			PROCESS_PSPREL_WHEN(us, psp, pspoff, t);
			break;
		case 2:
			PROCESS_PSPREL_WHEN(us, priunat, pspoff, t);
			break;
		case 3:
			PROCESS_PSPREL_WHEN(us, br[0], pspoff, t);
			break;
		case 4:
			PROCESS_PSPREL_WHEN(us, bsp, pspoff, t);
			break;
		case 5:
			PROCESS_PSPREL_WHEN(us, bspstore, pspoff, t);
			break;
		case 6:
			PROCESS_PSPREL_WHEN(us, rnat, pspoff, t);
			break;
		case 7:
			PROCESS_PSPREL_WHEN(us, unat, pspoff, t);
			break;
		case 8:
			PROCESS_PSPREL_WHEN(us, fpsr, pspoff, t);
			break;
		case 9:
			PROCESS_PSPREL_WHEN(us, pfs, pspoff, t);
			break;
		case 10:
			PROCESS_PSPREL_WHEN(us, lc, pspoff, t);
			break;
		}
	}
}

static void
parse_spill_sprel(struct ia64_unwind_state *us, int t,
		  int reg, int spoff)
{
	int type;

	type = reg >> 5;
	reg &= 0x1f;
	switch (type) {
	case 0:
		DPF(("save location for gr[%d] at sp+%d at offset %d\n",
		       reg, 4*spoff, t));
		us->us_regs.rs_gr[reg].ur_save = (u_int64_t *)
			(us->us_regs.rs_gr[12].ur_value + 4*spoff);
		us->us_regs.rs_gr[reg].ur_when = t;
		break;
	case 1:
		DPF(("save location for fr[%d] at sp+%d at offset %d\n",
		       reg, 4*spoff, t));
		us->us_regs.rs_fr[reg].ur_save = (struct ia64_fpreg *)
			(us->us_regs.rs_gr[12].ur_value + 4*spoff);
		us->us_regs.rs_fr[reg].ur_when = t;
		break;
	case 2:
		DPF(("save location for br[%d] at sp+%d at offset %d\n",
		       reg, 4*spoff, t));
		us->us_regs.rs_br[reg].ur_save = (u_int64_t *)
			(us->us_regs.rs_gr[12].ur_value + 4*spoff);
		us->us_regs.rs_br[reg].ur_when = t;
		break;
	case 3:
		switch (reg) {
		case 0:
			PROCESS_SPREL_WHEN(us, preds, spoff, t);
			break;
		case 1:
			PROCESS_SPREL_WHEN(us, psp, spoff, t);
			break;
		case 2:
			PROCESS_SPREL_WHEN(us, priunat, spoff, t);
			break;
		case 3:
			PROCESS_SPREL_WHEN(us, br[0], spoff, t);
			break;
		case 4:
			PROCESS_SPREL_WHEN(us, bsp, spoff, t);
			break;
		case 5:
			PROCESS_SPREL_WHEN(us, bspstore, spoff, t);
			break;
		case 6:
			PROCESS_SPREL_WHEN(us, rnat, spoff, t);
			break;
		case 7:
			PROCESS_SPREL_WHEN(us, unat, spoff, t);
			break;
		case 8:
			PROCESS_SPREL_WHEN(us, fpsr, spoff, t);
			break;
		case 9:
			PROCESS_SPREL_WHEN(us, pfs, spoff, t);
			break;
		case 10:
			PROCESS_SPREL_WHEN(us, lc, spoff, t);
			break;
		}
	}
}

static void
parse_spill_reg(struct ia64_unwind_state *us, int t,
		int reg, int treg)
{
	/* not done yet  */
}

static void
parse_spill_psprel_p(struct ia64_unwind_state *us, int t, int qp,
		     int reg, int pspoff)
{
	/* not done yet  */
}

static void
parse_spill_sprel_p(struct ia64_unwind_state *us, int t, int qp,
		    int reg, int spoff)
{
	/* not done yet  */
}

static void
parse_spill_reg_p(struct ia64_unwind_state *us, int t, int qp,
		  int reg, int treg)
{
	/* not done yet  */
}

static void
unwind_region(struct register_state *regs, int pc)
{
	int i;

#define RESTORE(x)							\
	do {								\
		if (regs->rs_##x.ur_save				\
		    && pc > regs->rs_##x.ur_when) {			\
			DPF(("restoring %s\n", #x));			\
			regs->rs_##x.ur_value = *regs->rs_##x.ur_save;	\
		}							\
		regs->rs_##x.ur_save = 0;				\
		regs->rs_##x.ur_when = 0;				\
	} while (0)

#define RESTORE_INDEX(x, i)						\
	do {								\
		if (regs->rs_##x[i].ur_save				\
		    && pc > regs->rs_##x[i].ur_when) {			\
			DPF(("restoring %s[%d]\n", #x, i));		\
			regs->rs_##x[i].ur_value =			\
				*regs->rs_##x[i].ur_save;		\
		}							\
		regs->rs_##x[i].ur_save = 0;				\
		regs->rs_##x[i].ur_when = 0;				\
	} while (0)

	if (regs->rs_stack_size) {
		DPF(("restoring psp\n"));
		regs->rs_psp.ur_value =
			regs->rs_gr[12].ur_value + regs->rs_stack_size;
		regs->rs_stack_size = 0;
	} else {
		RESTORE(psp);
	}
	RESTORE(pfs);
	RESTORE(preds);
	RESTORE(unat);
	RESTORE(lc);
	RESTORE(rnat);
	RESTORE(bsp);
	RESTORE(bspstore);
	RESTORE(fpsr);
	RESTORE(priunat);
	for (i = 0; i < 8; i++)
		RESTORE_INDEX(br, i);
	for (i = 0; i < 32; i++)
		RESTORE_INDEX(gr, i);
	for (i = 0; i < 32; i++)
		RESTORE_INDEX(fr, i);

#undef RESTORE
}

int
ia64_unwind_state_previous_frame(struct ia64_unwind_state *us)
{
	struct ia64_unwind_table *ut;
	struct ia64_unwind_table_entry *ute;
	struct ia64_unwind_info *ui;
	u_int8_t *p;
	u_int8_t *end;
	int region = 0;	/* 0 for prologue, 1 for body */
	int rlen = 0;

	/*
	 * Find the entry which describes this procedure.
	 */
	ut = find_table(us->us_ip);
	if (!ut)
		return ENOENT;
	ute = find_entry(ut, us->us_ip);
	if (!ute) {
		/*
		 * If there is no entry for this procedure, we assumes
		 * its a leaf (i.e. rp and ar.pfs is enough to restore
		 * the previous frame.
		 */
		goto noentry;
	}

	/*
	 * Calculate 'pc' as the number of instructions from the start
	 * of the procedure.
	 */
	us->us_pc = ((us->us_ip - (ute->ue_start + ut->ut_base)) / 16) * 3
		+ us->us_ri;

	/*
	 * Process unwind records until we find the record which
	 * contains the pc.
	 */
	ui = (struct ia64_unwind_info *) (ute->ue_info + ut->ut_base);
	p = (u_int8_t *) ui + 8;
	end = p + 8 * ui->ui_length;

	while (us->us_pc > 0 && p < end) {
		u_int8_t b = *p;

		/*
		 * Is this a header or a region descriptor?
		 */
		if ((b >> 7) == 0) {
			/*
			 * Header.
			 *
			 * Complete processing of previous region (if
			 * any) by restoring the appropriate registers
			 * and ajust pc to be relative to next region.
			 */
			unwind_region(&us->us_regs, us->us_pc);
			us->us_pc -= rlen;
			if (us->us_pc <= 0)
				break;

			if ((b >> 6) == 0) {
				/* R1 */
				region = (b >> 5) & 1;
				rlen = b & 0x1f;
				parse_prologue(us, rlen);
				p++;
			} else if ((b >> 3) == 8) {
				/* R2 */
				int mask, grsave;
				mask = ((b & 7) << 1) | (p[1] >> 7);
				grsave = p[1] & 0x7f;
				p += 2;
				rlen = read_uleb128(&p);
				parse_prologue_gr(us, rlen, mask, grsave);
			} else if ((b >> 2) == 24) {
				/* R3 */
				region = b & 3;
				p += 2;
				rlen = read_uleb128(&p);
				parse_prologue(us, rlen);
			} 
		} else {
			if (region == 0) {
				/*
				 * Prologue
				 */
				if ((b >> 5) == 4) {
					/* P1 */
					parse_br_mem(us, b & 0x1f);
					p++;
				} else if ((b >> 4) == 10) {
					/* P2 - br_gr */
					parse_br_gr(us,
						    (((b & 0xf) << 1)
						     | (p[1] >> 7)),
						    p[1] & 0x7f);
					p += 2;
				} else if ((b >> 3) == 22) {
					/* P3 */
					int which, r;
					which = ((b & 7) << 1) | (p[1] >> 7);
					r = p[1] & 0x7f;
					switch (which) {
					case 0:
						parse_psp_gr(us, r);
						break;
					case 1:
						parse_rp_gr(us, r);
						break;
					case 2:
						parse_pfs_gr(us, r);
						break;
					case 3:
						parse_preds_gr(us, r);
						break;
					case 4:
						parse_unat_gr(us, r);
						break;
					case 5:
						parse_lc_gr(us, r);
						break;
					case 6:
						parse_rp_br(us, r);
						break;
					case 7:
						parse_rnat_gr(us, r);
						break;
					case 8:
						parse_bsp_gr(us, r);
						break;
					case 9:
						parse_bspstore_gr(us, r);
						break;
					case 10:
						parse_fpsr_gr(us, r);
						break;
					case 11:
						parse_priunat_gr(us, r);
						break;
					}
					p += 2;
				} else if ((b >> 0) == 184) {
					/* P4 */
					parse_spill_mask(us, rlen, p+1);
					p += 1 + (rlen + 3) / 4;
				} else if ((b >> 0) == 185) {
					/* P5 - frgr_mem */
					parse_frgr_mem(us,
						       p[1] >> 4,
						       ((p[1] & 0xf) << 16)
						       | (p[2] << 8) | p[3]);
					p += 3;
				} else if ((b >> 5) == 6) {
					/* P6 */
					if (b & 0x10)
						parse_gr_mem(us, b & 0xf);
					else
						parse_fr_mem(us, b & 0xf);
					p++;
				} else if ((b >> 4) == 14) {
					/* P7 */
					int x, y;
					p++;
					x = read_uleb128(&p);
					switch (b & 0xf) {
					case 0:
						y = read_uleb128(&p);
						parse_mem_stack_f(us, x, y);
						break;
					case 1:
						parse_mem_stack_v(us, x);
						break;
					case 2:
						parse_spill_base(us, x);
						break;
					case 3:
						parse_psp_sprel(us, x);
						break;
					case 4:
						parse_rp_when(us, x);
						break;
					case 5:
						parse_rp_psprel(us, x);
						break;
					case 6:
						parse_pfs_when(us, x);
						break;
					case 7:
						parse_pfs_psprel(us, x);
						break;
					case 8:
						parse_preds_when(us, x);
						break;
					case 9:
						parse_preds_psprel(us, x);
						break;
					case 10:
						parse_lc_when(us, x);
						break;
					case 11:
						parse_lc_psprel(us, x);
						break;
					case 12:
						parse_unat_when(us, x);
						break;
					case 13:
						parse_unat_psprel(us, x);
						break;
					case 14:
						parse_fpsr_when(us, x);
						break;
					case 15:
						parse_fpsr_psprel(us, x);
						break;
					}
				} else if ((b >> 0) == 240) {
					/* P8 */
					int which = p[1];
					int x;
					p += 2;
					x = read_uleb128(&p);
					switch (which) {
					case 1:
						parse_rp_sprel(us, x);
						break;
					case 2:
						parse_pfs_sprel(us, x);
						break;
					case 3:
						parse_preds_sprel(us, x);
						break;
					case 4:
						parse_lc_sprel(us, x);
						break;
					case 5:
						parse_unat_sprel(us, x);
						break;
					case 6:
						parse_fpsr_sprel(us, x);
						break;
					case 7:
						parse_bsp_when(us, x);
						break;
					case 8:
						parse_bsp_psprel(us, x);
						break;
					case 9:
						parse_bsp_sprel(us, x);
						break;
					case 10:
						parse_bspstore_when(us, x);
						break;
					case 11:
						parse_bspstore_psprel(us, x);
						break;
					case 12:
						parse_bspstore_sprel(us, x);
						break;
					case 13:
						parse_rnat_when(us, x);
						break;
					case 14:
						parse_rnat_psprel(us, x);
						break;
					case 15:
						parse_rnat_sprel(us, x);
						break;
					case 16:
						parse_priunat_when_gr(us, x);
						break;
					case 17:
						parse_priunat_psprel(us, x);
						break;
					case 18:
						parse_priunat_sprel(us, x);
						break;
					case 19:
						parse_priunat_when_mem(us, x);
						break;
					}
				} else if ((b >> 0) == 241) {
					/* P9 */
					parse_gr_gr(us, p[1], p[2]);
					p += 3;
				} else if ((b >> 0) == 255) {
					/* P10 (ignored) */
					p += 3;
				}
			} else {
				if ((b >> 6) == 2) {
					/* B1 */
					if ((b & (1 << 5)) == 0)
						parse_label_state(us,
								  b & 0x1f);
					else
						parse_copy_state(us,
								 b & 0x1f);
				} else if ((b >> 5) == 6) {
					/* B2 */
					int ecount = b & 0x1f;
					int t;
					p++;
					t = read_uleb128(&p);
					parse_epilogue(us, t, ecount);
				} else if ((b >> 0) == 224) {
					/* B3 */
					int t, ecount;
					p++;
					t = read_uleb128(&p);
					ecount = read_uleb128(&p);
					parse_epilogue(us, t, ecount);
				} else if ((b >> 4) == 15) {
					/* B4 */
					int label;
					p++;
					label = read_uleb128(&p);
					if ((b & (1 << 3)) == 0)
						parse_label_state(us, label);
					else
						parse_copy_state(us, label);
				}
			}
			/*
			 * X records can appear in both prologue and
			 * body.
			 */
			if ((b >> 0) == 249) {
				/* X1 */
				int r, reg, t, off;
				r = (p[1] >> 7) & 1;
				reg = p[1] & 0x7f;
				p += 2;
				t = read_uleb128(&p);
				off = read_uleb128(&p);
				if (r == 0)
					parse_spill_psprel(us, t, reg, off);
				else
					parse_spill_sprel(us, t, reg, off);
			} else if ((b >> 0) == 250) {
				/* X2 */
				int reg, treg, t;
				reg = p[1] & 0x7f;
				treg = p[2] | ((p[1] & 0x80) << 1);
				p += 3;
				t = read_uleb128(&p);
				parse_spill_reg(us, t, reg, treg);
			} else if ((b >> 0) == 251) {
				/* X3 */
				int r, qp, reg, t, off;
				r = (p[1] >> 7) & 1;
				qp = p[1] & 0x3f;
				reg = p[2];
				p += 3;
				t = read_uleb128(&p);
				off = read_uleb128(&p);
				if (r == 0)
					parse_spill_psprel_p(us, t, qp,
							     reg, off);
				else
					parse_spill_sprel_p(us, t, qp,
							    reg, off);
			} else if ((b >> 0) == 252) {
				/* X4 */
				int qp, reg, treg, t;
				qp = p[1] & 0x3f;
				reg = p[2] & 0x7f;
				treg = p[3] | ((p[2] & 0x80) << 1);
				p += 4;
				t = read_uleb128(&p);
				parse_spill_reg_p(us, t, qp, reg, treg);
			}
		}
	}

 noentry:
	/*
	 * Now that we have worked out suitable values for rp, ar.pfs
	 * and sp, we can shift state to the previous function. If we
	 * haven't managed to figure out a new value for ip, then we
	 * assume that the unwinding didn't work and return an error.
	 */
	if (us->us_ip == us->us_regs.rs_br[0].ur_value)
		return EINVAL;
	us->us_ip = us->us_regs.rs_br[0].ur_value;
	us->us_ri = 0;
	us->us_cfm = us->us_regs.rs_pfs.ur_value;
	DPF(("new cfm is 0x%lx\n", us->us_cfm));
	us->us_bsp = ia64_rse_previous_frame(us->us_bsp,
					     (us->us_cfm >> 7) & 0x7f);
	DPF(("new bsp is %p\n", us->us_bsp));
	us->us_regs.rs_gr[12] = us->us_regs.rs_psp;

	return 0;
}
