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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <machine/frame.h>
#include <machine/inst.h>

#define sign_extend(imm, w) (((int64_t)(imm) << (64 - (w))) >> (64 - (w)))

extern int	ia64_unaligned_print, ia64_unaligned_fix;
extern int	ia64_unaligned_sigbus;

int unaligned_fixup(struct trapframe *framep, struct thread *td);

enum type {
	LD_SA,
	LD_S,
	LD_A,
	LD_C_CLR,
	LD_C_NC,
	LD
};

struct decoding {
	int isload;		/* non-zero if load */
	enum type type;		/* type of load or store */
	int basereg;		/* address to load or store */
	int reg;		/* register number to load or store */
	int width;		/* number of bytes */
	int update;		/* update value for basereg */
	int updateisreg;	/* non-zero if update is a register */
	int fence;		/* non-zero if fence needed */
};

static int
unaligned_decode_M1(union ia64_instruction ins, struct decoding *d)
{
	static enum type types[] = {
		LD, LD_S, LD_A,	LD_SA, LD,
		LD, LD, LD_C_CLR, LD_C_NC, LD_C_CLR
	};
	d->isload = 1;
	d->type = types[ins.M1.x6 >> 2];
	d->basereg = ins.M1.r3;
	d->reg = ins.M1.r1;
	d->width = (1 << (ins.M1.x6 & 3));
	if ((ins.M1.x6 >= 0x14 && ins.M1.x6 <= 0x17)
	    || (ins.M1.x6 >= 0x28 && ins.M1.x6 <= 0x2b))
	    d->fence = 1;
	return 1;
}

static int
unaligned_decode_M2(union ia64_instruction ins, struct decoding *d)
{
	static enum type types[] = {
		LD, LD_S, LD_A,	LD_SA, LD,
		LD, LD, LD_C_CLR, LD_C_NC, LD_C_CLR
	};
	d->isload = 1;
	d->type = types[ins.M1.x6 >> 2];
	d->basereg = ins.M2.r3;
	d->reg = ins.M2.r1;
	d->width = (1 << (ins.M2.x6 & 3));
	d->update = ins.M2.r2;
	d->updateisreg = 1;
	if ((ins.M2.x6 >= 0x14 && ins.M2.x6 <= 0x17)
	    || (ins.M2.x6 >= 0x28 && ins.M2.x6 <= 0x2b))
	    d->fence = 1;
	return 1;
}

static int
unaligned_decode_M3(union ia64_instruction ins, struct decoding *d)
{
	static enum type types[] = {
		LD, LD_S, LD_A,	LD_SA, LD,
		LD, LD, LD_C_CLR, LD_C_NC, LD_C_CLR
	};
	d->isload = 1;
	d->type = types[ins.M1.x6 >> 2];
	d->basereg = ins.M3.r3;
	d->reg = ins.M3.r1;
	d->width = (1 << (ins.M3.x6 & 3));
	d->update = sign_extend((ins.M3.s << 8)
				| (ins.M3.i << 7)
				| ins.M3.imm7b, 9);
	if ((ins.M3.x6 >= 0x14 && ins.M3.x6 <= 0x17)
	    || (ins.M3.x6 >= 0x28 && ins.M3.x6 <= 0x2b))
	    d->fence = 1;
	return 1;
}

static int
unaligned_decode_M4(union ia64_instruction ins, struct decoding *d)
{
	d->isload = 0;
	d->basereg = ins.M4.r3;
	d->reg = ins.M4.r2;
	d->width = (1 << (ins.M4.x6 & 3));
	if (ins.M4.x6 >= 0x34 && ins.M4.x6 <= 0x37)
	    d->fence = 1;
	return 1;
}

static int
unaligned_decode_M5(union ia64_instruction ins, struct decoding *d)
{
	d->isload = 0;
	d->basereg = ins.M5.r3;
	d->reg = ins.M5.r2;
	d->width = (1 << (ins.M5.x6 & 3));
	d->update = sign_extend((ins.M5.s << 8)
				| (ins.M5.i << 7)
				| ins.M5.imm7a, 9);
	if (ins.M5.x6 >= 0x34 && ins.M5.x6 <= 0x37)
	    d->fence = 1;
	return 1;
}

static int
read_register(struct trapframe *framep, struct thread *td,
	      int reg, u_int64_t *valuep)
{

	if (reg < 32) {
		switch (reg) {
		case 0:  *valuep = 0; break;
		case 1:  *valuep = framep->tf_special.gp; break;
		case 2:	 *valuep = framep->tf_scratch.gr2; break;
		case 3:  *valuep = framep->tf_scratch.gr3; break;
		case 8:  *valuep = framep->tf_scratch.gr8; break;
		case 9:  *valuep = framep->tf_scratch.gr9; break;
		case 10: *valuep = framep->tf_scratch.gr10; break;
		case 11: *valuep = framep->tf_scratch.gr11; break;
		case 12: *valuep = framep->tf_special.sp; break;
		case 13: *valuep = framep->tf_special.tp; break;
		case 14: *valuep = framep->tf_scratch.gr14; break;
		case 15: *valuep = framep->tf_scratch.gr15; break;
		case 16: *valuep = framep->tf_scratch.gr16; break;
		case 17: *valuep = framep->tf_scratch.gr17; break;
		case 18: *valuep = framep->tf_scratch.gr18; break;
		case 19: *valuep = framep->tf_scratch.gr19; break;
		case 20: *valuep = framep->tf_scratch.gr20; break;
		case 21: *valuep = framep->tf_scratch.gr21; break;
		case 22: *valuep = framep->tf_scratch.gr22; break;
		case 23: *valuep = framep->tf_scratch.gr23; break;
		case 24: *valuep = framep->tf_scratch.gr24; break;
		case 25: *valuep = framep->tf_scratch.gr25; break;
		case 26: *valuep = framep->tf_scratch.gr26; break;
		case 27: *valuep = framep->tf_scratch.gr27; break;
		case 28: *valuep = framep->tf_scratch.gr28; break;
		case 29: *valuep = framep->tf_scratch.gr29; break;
		case 30: *valuep = framep->tf_scratch.gr30; break;
		case 31: *valuep = framep->tf_scratch.gr31; break;
		default:
			return (EINVAL);
		}
	} else {
#if 0
		u_int64_t cfm = framep->tf_special.cfm;
		u_int64_t *bsp = (u_int64_t *)(td->td_kstack +
		    framep->tf_ndirty);
		int sof = cfm & 0x7f;
		int sor = 8*((cfm >> 14) & 15);
		int rrb_gr = (cfm >> 18) & 0x7f;

		/*
		 * Skip back to the start of the interrupted frame.
		 */
		bsp = ia64_rse_previous_frame(bsp, sof);

		if (reg - 32 > sof)
			return EINVAL;
		if (reg - 32 < sor) {
			if (reg - 32 + rrb_gr >= sor)
				reg = reg + rrb_gr - sor;
			else
				reg = reg + rrb_gr;
		}

		*valuep = *ia64_rse_register_address(bsp, reg);
		return (0);
#else
		return (EINVAL);
#endif
	}
	return (0);
}

static int
write_register(struct trapframe *framep, struct thread *td,
	      int reg, u_int64_t value)
{

	if (reg < 32) {
		switch (reg) {
		case 1:  framep->tf_special.gp = value; break;
		case 2:	 framep->tf_scratch.gr2 = value; break;
		case 3:  framep->tf_scratch.gr3 = value; break;
		case 8:  framep->tf_scratch.gr8 = value; break;
		case 9:  framep->tf_scratch.gr9 = value; break;
		case 10: framep->tf_scratch.gr10 = value; break;
		case 11: framep->tf_scratch.gr11 = value; break;
		case 12: framep->tf_special.sp = value; break;
		case 13: framep->tf_special.tp = value; break;
		case 14: framep->tf_scratch.gr14 = value; break;
		case 15: framep->tf_scratch.gr15 = value; break;
		case 16: framep->tf_scratch.gr16 = value; break;
		case 17: framep->tf_scratch.gr17 = value; break;
		case 18: framep->tf_scratch.gr18 = value; break;
		case 19: framep->tf_scratch.gr19 = value; break;
		case 20: framep->tf_scratch.gr20 = value; break;
		case 21: framep->tf_scratch.gr21 = value; break;
		case 22: framep->tf_scratch.gr22 = value; break;
		case 23: framep->tf_scratch.gr23 = value; break;
		case 24: framep->tf_scratch.gr24 = value; break;
		case 25: framep->tf_scratch.gr25 = value; break;
		case 26: framep->tf_scratch.gr26 = value; break;
		case 27: framep->tf_scratch.gr27 = value; break;
		case 28: framep->tf_scratch.gr28 = value; break;
		case 29: framep->tf_scratch.gr29 = value; break;
		case 30: framep->tf_scratch.gr30 = value; break;
		case 31: framep->tf_scratch.gr31 = value; break;
		default:
			return (EINVAL);
		}
	} else {
#if 0
		u_int64_t cfm = framep->tf_special.cfm;
		u_int64_t *bsp = (u_int64_t *) (td->td_kstack
						+ framep->tf_ndirty);
		int sof = cfm & 0x7f;
		int sor = 8*((cfm >> 14) & 15);
		int rrb_gr = (cfm >> 18) & 0x7f;

		/*
		 * Skip back to the start of the interrupted frame.
		 */
		bsp = ia64_rse_previous_frame(bsp, sof);

		if (reg - 32 > sof)
			return EINVAL;
		if (reg - 32 < sor) {
			if (reg - 32 + rrb_gr >= sor)
				reg = reg + rrb_gr - sor;
			else
				reg = reg + rrb_gr;
		}

		*ia64_rse_register_address(bsp, reg) = value;
		return 0;
#else
		return (EINVAL);
#endif
	}
	return (0);
}

/*
 * Messy.
 */
static void
invala_e(int reg)
{
	switch (reg) {
	case   0:	__asm __volatile("invala.e r0"); break;
	case   1:	__asm __volatile("invala.e r1"); break;
	case   2:	__asm __volatile("invala.e r2"); break;
	case   3:	__asm __volatile("invala.e r3"); break;
	case   4:	__asm __volatile("invala.e r4"); break;
	case   5:	__asm __volatile("invala.e r5"); break;
	case   6:	__asm __volatile("invala.e r6"); break;
	case   7:	__asm __volatile("invala.e r7"); break;
	case   8:	__asm __volatile("invala.e r8"); break;
	case   9:	__asm __volatile("invala.e r9"); break;
	case  10:	__asm __volatile("invala.e r10"); break;
	case  11:	__asm __volatile("invala.e r11"); break;
	case  12:	__asm __volatile("invala.e r12"); break;
	case  13:	__asm __volatile("invala.e r13"); break;
	case  14:	__asm __volatile("invala.e r14"); break;
	case  15:	__asm __volatile("invala.e r15"); break;
	case  16:	__asm __volatile("invala.e r16"); break;
	case  17:	__asm __volatile("invala.e r17"); break;
	case  18:	__asm __volatile("invala.e r18"); break;
	case  19:	__asm __volatile("invala.e r19"); break;
	case  20:	__asm __volatile("invala.e r20"); break;
	case  21:	__asm __volatile("invala.e r21"); break;
	case  22:	__asm __volatile("invala.e r22"); break;
	case  23:	__asm __volatile("invala.e r23"); break;
	case  24:	__asm __volatile("invala.e r24"); break;
	case  25:	__asm __volatile("invala.e r25"); break;
	case  26:	__asm __volatile("invala.e r26"); break;
	case  27:	__asm __volatile("invala.e r27"); break;
	case  28:	__asm __volatile("invala.e r28"); break;
	case  29:	__asm __volatile("invala.e r29"); break;
	case  30:	__asm __volatile("invala.e r30"); break;
	case  31:	__asm __volatile("invala.e r31"); break;
	case  32:	__asm __volatile("invala.e r32"); break;
	case  33:	__asm __volatile("invala.e r33"); break;
	case  34:	__asm __volatile("invala.e r34"); break;
	case  35:	__asm __volatile("invala.e r35"); break;
	case  36:	__asm __volatile("invala.e r36"); break;
	case  37:	__asm __volatile("invala.e r37"); break;
	case  38:	__asm __volatile("invala.e r38"); break;
	case  39:	__asm __volatile("invala.e r39"); break;
	case  40:	__asm __volatile("invala.e r40"); break;
	case  41:	__asm __volatile("invala.e r41"); break;
	case  42:	__asm __volatile("invala.e r42"); break;
	case  43:	__asm __volatile("invala.e r43"); break;
	case  44:	__asm __volatile("invala.e r44"); break;
	case  45:	__asm __volatile("invala.e r45"); break;
	case  46:	__asm __volatile("invala.e r46"); break;
	case  47:	__asm __volatile("invala.e r47"); break;
	case  48:	__asm __volatile("invala.e r48"); break;
	case  49:	__asm __volatile("invala.e r49"); break;
	case  50:	__asm __volatile("invala.e r50"); break;
	case  51:	__asm __volatile("invala.e r51"); break;
	case  52:	__asm __volatile("invala.e r52"); break;
	case  53:	__asm __volatile("invala.e r53"); break;
	case  54:	__asm __volatile("invala.e r54"); break;
	case  55:	__asm __volatile("invala.e r55"); break;
	case  56:	__asm __volatile("invala.e r56"); break;
	case  57:	__asm __volatile("invala.e r57"); break;
	case  58:	__asm __volatile("invala.e r58"); break;
	case  59:	__asm __volatile("invala.e r59"); break;
	case  60:	__asm __volatile("invala.e r60"); break;
	case  61:	__asm __volatile("invala.e r61"); break;
	case  62:	__asm __volatile("invala.e r62"); break;
	case  63:	__asm __volatile("invala.e r63"); break;
	case  64:	__asm __volatile("invala.e r64"); break;
	case  65:	__asm __volatile("invala.e r65"); break;
	case  66:	__asm __volatile("invala.e r66"); break;
	case  67:	__asm __volatile("invala.e r67"); break;
	case  68:	__asm __volatile("invala.e r68"); break;
	case  69:	__asm __volatile("invala.e r69"); break;
	case  70:	__asm __volatile("invala.e r70"); break;
	case  71:	__asm __volatile("invala.e r71"); break;
	case  72:	__asm __volatile("invala.e r72"); break;
	case  73:	__asm __volatile("invala.e r73"); break;
	case  74:	__asm __volatile("invala.e r74"); break;
	case  75:	__asm __volatile("invala.e r75"); break;
	case  76:	__asm __volatile("invala.e r76"); break;
	case  77:	__asm __volatile("invala.e r77"); break;
	case  78:	__asm __volatile("invala.e r78"); break;
	case  79:	__asm __volatile("invala.e r79"); break;
	case  80:	__asm __volatile("invala.e r80"); break;
	case  81:	__asm __volatile("invala.e r81"); break;
	case  82:	__asm __volatile("invala.e r82"); break;
	case  83:	__asm __volatile("invala.e r83"); break;
	case  84:	__asm __volatile("invala.e r84"); break;
	case  85:	__asm __volatile("invala.e r85"); break;
	case  86:	__asm __volatile("invala.e r86"); break;
	case  87:	__asm __volatile("invala.e r87"); break;
	case  88:	__asm __volatile("invala.e r88"); break;
	case  89:	__asm __volatile("invala.e r89"); break;
	case  90:	__asm __volatile("invala.e r90"); break;
	case  91:	__asm __volatile("invala.e r91"); break;
	case  92:	__asm __volatile("invala.e r92"); break;
	case  93:	__asm __volatile("invala.e r93"); break;
	case  94:	__asm __volatile("invala.e r94"); break;
	case  95:	__asm __volatile("invala.e r95"); break;
	case  96:	__asm __volatile("invala.e r96"); break;
	case  97:	__asm __volatile("invala.e r97"); break;
	case  98:	__asm __volatile("invala.e r98"); break;
	case  99:	__asm __volatile("invala.e r99"); break;
	case 100:	__asm __volatile("invala.e r100"); break;
	case 101:	__asm __volatile("invala.e r101"); break;
	case 102:	__asm __volatile("invala.e r102"); break;
	case 103:	__asm __volatile("invala.e r103"); break;
	case 104:	__asm __volatile("invala.e r104"); break;
	case 105:	__asm __volatile("invala.e r105"); break;
	case 106:	__asm __volatile("invala.e r106"); break;
	case 107:	__asm __volatile("invala.e r107"); break;
	case 108:	__asm __volatile("invala.e r108"); break;
	case 109:	__asm __volatile("invala.e r109"); break;
	case 110:	__asm __volatile("invala.e r110"); break;
	case 111:	__asm __volatile("invala.e r111"); break;
	case 112:	__asm __volatile("invala.e r112"); break;
	case 113:	__asm __volatile("invala.e r113"); break;
	case 114:	__asm __volatile("invala.e r114"); break;
	case 115:	__asm __volatile("invala.e r115"); break;
	case 116:	__asm __volatile("invala.e r116"); break;
	case 117:	__asm __volatile("invala.e r117"); break;
	case 118:	__asm __volatile("invala.e r118"); break;
	case 119:	__asm __volatile("invala.e r119"); break;
	case 120:	__asm __volatile("invala.e r120"); break;
	case 121:	__asm __volatile("invala.e r121"); break;
	case 122:	__asm __volatile("invala.e r122"); break;
	case 123:	__asm __volatile("invala.e r123"); break;
	case 124:	__asm __volatile("invala.e r124"); break;
	case 125:	__asm __volatile("invala.e r125"); break;
	case 126:	__asm __volatile("invala.e r126"); break;
	case 127:	__asm __volatile("invala.e r127"); break;
	}
}

int
unaligned_fixup(struct trapframe *framep, struct thread *td)
{
	vm_offset_t va = framep->tf_special.ifa;
	int doprint, dofix, dosigbus;
	int signal, size = 0;
	unsigned long uac;
	struct proc *p;
	u_int64_t low, high;
	struct ia64_bundle b;
	int slot;
	union ia64_instruction ins;
	int decoded;
	struct decoding dec;

	/*
	 * Figure out what actions to take.
	 */

	if (td) {
		uac = td->td_md.md_flags & MDP_UAC_MASK;
		p = td->td_proc;
	} else {
		uac = 0;
		p = NULL;
	}

	doprint = ia64_unaligned_print && !(uac & MDP_UAC_NOPRINT);
	dofix = ia64_unaligned_fix && !(uac & MDP_UAC_NOFIX);
	dosigbus = ia64_unaligned_sigbus | (uac & MDP_UAC_SIGBUS);

	/*
	 * If psr.ac is set, then clearly the user program *wants* to
	 * fault.
	 */
	if (framep->tf_special.psr & IA64_PSR_AC) {
		dofix = 0;
		dosigbus = 1;
	}

	/*
	 * See if the user can access the memory in question.
	 * Even if it's an unknown opcode, SEGV if the access
	 * should have failed.
	 */
	if (!useracc((caddr_t)va, size ? size : 1, VM_PROT_WRITE)) {
		signal = SIGSEGV;
		goto out;
	}

	/*
	 * Read the instruction bundle and attempt to decode the
	 * offending instruction.
	 * XXX assume that the instruction is in an 'M' slot.
	 */
	copyin((const void *) framep->tf_special.iip, &low, 8);
	copyin((const void *) (framep->tf_special.iip + 8), &high, 8);
	ia64_unpack_bundle(low, high, &b);
	slot = (framep->tf_special.psr >> 41) & 3;
	ins.ins = b.slot[slot];

	decoded = 0;
	bzero(&dec, sizeof(dec));
	if (ins.M1.op == 4) {
		if (ins.M1.m == 0 && ins.M1.x == 0) {
			/* Table 4-29 */
			if (ins.M1.x6 < 0x30)
				decoded = unaligned_decode_M1(ins, &dec);
			else
				decoded = unaligned_decode_M4(ins, &dec);
		} else if (ins.M1.m == 1 && ins.M1.x == 0) {
			/* Table 4-30 */
			decoded = unaligned_decode_M2(ins, &dec);
		}
	} else if (ins.M1.op == 5) {
		/* Table 4-31 */
		if (ins.M1.x6 < 0x30)
			decoded = unaligned_decode_M3(ins, &dec);
		else
			decoded = unaligned_decode_M5(ins, &dec);
	}

	/*
	 * If we're supposed to be noisy, squawk now.
	 */
	if (doprint) {
		uprintf("pid %d (%s): unaligned access: va=0x%lx pc=0x%lx",
			p->p_pid, p->p_comm, va, framep->tf_special.iip);
		if (decoded) {
			uprintf(" op=");
			if (dec.isload) {
				static char *ldops[] = {
					"ld%d.sa", "ld%d.s", "ld%d.a",
					"ld%d.c.clr", "ld%d.c.nc", "ld%d"
				};
				uprintf(ldops[dec.type], dec.width);
				uprintf(" r%d=[r%d]", dec.reg, dec.basereg);
			} else {
				uprintf("st%d [r%d]=r%d", dec.width,
					dec.basereg, dec.reg);
			}
			if (dec.updateisreg)
				uprintf(",r%d\n", dec.update);
			else if (dec.update)
				uprintf(",%d\n", dec.update);
			else
				uprintf("\n");
		} else {
			uprintf("\n");
		}
	}

	/*
	 * If we should try to fix it and know how, give it a shot.
	 *
	 * We never allow bad data to be unknowingly used by the
	 * user process.  That is, if we decide not to fix up an
	 * access we cause a SIGBUS rather than letting the user
	 * process go on without warning.
	 *
	 * If we're trying to do a fixup, we assume that things
	 * will be botched.  If everything works out OK, 
	 * unaligned_{load,store}_* clears the signal flag.
	 */
	signal = 0;
	if (dofix && decoded) {
		u_int64_t addr, update, value, isr;
		int error = 0;

		/*
		 * We only really need this if the current bspstore
		 * hasn't advanced past the user's register frame. Its
		 * hardly worth trying to optimise though.
		 */
		__asm __volatile("flushrs");

		isr = framep->tf_special.isr;
		error = read_register(framep, td, dec.basereg, &addr);
		if (error) {
			signal = SIGBUS;
			goto out;
		}
		if (dec.updateisreg) {
			error = read_register(framep, td, dec.update, &update);
			if (error) {
				signal = SIGBUS;
				goto out;
			}
		} else {
			update = dec.update;
		}

		/* Assume little-endian */
		if (dec.isload) {
			/*
			 * Sanity checks.
			 */
			if (!(isr & IA64_ISR_R)
			    || (isr & (IA64_ISR_W|IA64_ISR_X|IA64_ISR_NA))) {
				printf("unaligned_fixup: unexpected cr.isr value\n");
				signal = SIGBUS;
				goto out;
			}

			if (dec.type == LD_SA || dec.type == LD_A) {
				invala_e(dec.reg);
				goto out;
			}
			if (dec.type == LD_C_CLR)
				invala_e(dec.reg);
			if (dec.type == LD_S)
				/* XXX not quite sure what to do here */;

			value = 0;
			if (!error && dec.fence)
				ia64_mf();
			error = copyin((const void *)addr, &value, dec.width);
			if (!error)
				error = write_register(framep, td, dec.reg,
						       value);
			if (!error && update)
				error = write_register(framep, td, dec.basereg,
						       addr + update);
		} else {
			error = read_register(framep, td, dec.reg, &value);
			if (!error)
				error = copyout(&value, (void *)addr,
						dec.width);
			if (!error && dec.fence)
				ia64_mf();
			if (!error && update)
				error = write_register(framep, td, dec.basereg,
						       addr + update);
		}
		if (error) {
			signal = SIGBUS;
		} else {
			/*
			 * Advance to the instruction following the
			 * one which faulted.
			 */
			if ((framep->tf_special.psr & IA64_PSR_RI)
			    == IA64_PSR_RI_2) {
				framep->tf_special.psr &= ~IA64_PSR_RI;
				framep->tf_special.iip += 16;
			} else {
				framep->tf_special.psr += IA64_PSR_RI_1;
			}
		}
	} else {
		signal = SIGBUS;
	} 

	/*
	 * Force SIGBUS if requested.
	 */
	if (dosigbus)
		signal = SIGBUS;

out:
	return (signal);
}
