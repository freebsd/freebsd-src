/*-
 * Copyright (c) 2000-2006 Marcel Moolenaar
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
__FBSDID("$FreeBSD: src/sys/ia64/disasm/disasm_format.c,v 1.4 2006/06/24 19:21:11 marcel Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <ia64/disasm/disasm_int.h>
#include <ia64/disasm/disasm.h>

/*
 * Mnemonics (keep in sync with enum asm_op).
 */
static const char *asm_mnemonics[] = {
	NULL,
	"add", "addl", "addp4", "adds", "alloc", "and", "andcm",
	"br", "break", "brl", "brp", "bsw",
	"chk", "clrrrb", "cmp", "cmp4", "cmp8xchg16", "cmpxchg1", "cmpxchg2",
	"cmpxchg4", "cmpxchg8", "cover", "czx1", "czx2",
	"dep",
	"epc", "extr",
	"famax", "famin", "fand", "fandcm", "fc", "fchkf", "fclass", "fclrf",
	"fcmp", "fcvt", "fetchadd4", "fetchadd8", "flushrs", "fma", "fmax",
	"fmerge", "fmin", "fmix", "fms", "fnma", "for", "fpack", "fpamax",
	"fpamin", "fpcmp", "fpcvt", "fpma", "fpmax", "fpmerge", "fpmin",
	"fpms", "fpnma", "fprcpa", "fprsqrta", "frcpa", "frsqrta", "fselect",
	"fsetc", "fswap", "fsxt", "fwb", "fxor",
	"getf",
	"hint",
	"invala", "itc", "itr",
	"ld1", "ld16", "ld2", "ld4", "ld8", "ldf", "ldf8", "ldfd", "ldfe",
	"ldfp8", "ldfpd", "ldfps", "ldfs", "lfetch", "loadrs",
	"mf", "mix1", "mix2", "mix4", "mov", "movl", "mux1", "mux2",
	"nop",
	"or",
	"pack2", "pack4", "padd1", "padd2", "padd4", "pavg1", "pavg2",
	"pavgsub1", "pavgsub2", "pcmp1", "pcmp2", "pcmp4", "pmax1", "pmax2",
	"pmin1", "pmin2", "pmpy2", "pmpyshr2", "popcnt", "probe", "psad1",
	"pshl2", "pshl4", "pshladd2", "pshr2", "pshr4", "pshradd2", "psub1",
	"psub2", "psub4", "ptc", "ptr",
	"rfi", "rsm", "rum",
	"setf", "shl", "shladd", "shladdp4", "shr", "shrp", "srlz", "ssm",
	"st1", "st16", "st2", "st4", "st8", "stf", "stf8", "stfd", "stfe",
	"stfs", "sub", "sum", "sxt1", "sxt2", "sxt4", "sync",
	"tak", "tbit", "tf", "thash", "tnat", "tpa", "ttag",
	"unpack1", "unpack2", "unpack4",
	"vmsw",
	"xchg1", "xchg2", "xchg4", "xchg8", "xma", "xor",
	"zxt1", "zxt2", "zxt4"
};

/*
 * Completers (keep in sync with enum asm_cmpltr_type).
 */
static const char *asm_completers[] = {
	"",
	".0", ".1",
	".a", ".acq", ".and",
	".b", ".bias",
	".c.clr", ".c.clr.acq", ".c.nc", ".call", ".cexit", ".cloop", ".clr",
	".ctop",
	".d", ".dc.dc", ".dc.nt", ".dpnt", ".dptk",
	".e", ".eq", ".excl", ".exit", ".exp",
	".f", ".fault", ".few", ".fill", ".fx", ".fxu",
	".g", ".ga", ".ge", ".gt",
	".h", ".hu",
	".i", ".ia", ".imp",
	".l", ".le", ".loop", ".lr", ".lt", ".ltu",
	".m", ".many",
	".nc", ".ne", ".neq", ".nl", ".nle", ".nlt", ".nm", ".nr", ".ns",
	".nt.dc", ".nt.nt", ".nt.tk", ".nt1", ".nt2", ".nta", ".nz",
	".or", ".or.andcm", ".ord",
	".pr",
	".r", ".raz", ".rel", ".ret", ".rw",
	".s", ".s0", ".s1", ".s2", ".s3", ".sa", ".se", ".sig", ".spill",
	".spnt", ".sptk", ".sss",
	".tk.dc", ".tk.nt", ".tk.tk", ".trunc",
	".u", ".unc", ".unord", ".uss", ".uus", ".uuu",
	".w", ".wexit", ".wtop",
	".x", ".xf",
	".z"
};

void
asm_completer(const struct asm_cmpltr *c, char *buf)
{
	strcpy(buf, asm_completers[c->c_type]);
}

void
asm_mnemonic(enum asm_op op, char *buf)
{
	strcpy(buf, asm_mnemonics[(op < ASM_OP_INTERNAL_OPCODES) ? op : 0]);
}

void
asm_operand(const struct asm_oper *o, char *buf, uint64_t ip)
{
	const char *n;

	n = NULL;
	switch (o->o_type) {
	case ASM_OPER_AREG:
		switch ((int)o->o_value) {
		case AR_K0: n = "k0"; break;
		case AR_K1: n = "k1"; break;
		case AR_K2: n = "k2"; break;
		case AR_K3: n = "k3"; break;
		case AR_K4: n = "k4"; break;
		case AR_K5: n = "k5"; break;
		case AR_K6: n = "k6"; break;
		case AR_K7: n = "k7"; break;
		case AR_RSC: n = "rsc"; break;
		case AR_BSP: n = "bsp"; break;
		case AR_BSPSTORE: n = "bspstore"; break;
		case AR_RNAT: n = "rnat"; break;
		case AR_FCR: n = "fcr"; break;
		case AR_EFLAG: n = "eflag"; break;
		case AR_CSD: n = "csd"; break;
		case AR_SSD: n = "ssd"; break;
		case AR_CFLG: n = "cflg"; break;
		case AR_FSR: n = "fsr"; break;
		case AR_FIR: n = "fir"; break;
		case AR_FDR: n = "fdr"; break;
		case AR_CCV: n = "ccv"; break;
		case AR_UNAT: n = "unat"; break;
		case AR_FPSR: n = "fpsr"; break;
		case AR_ITC: n = "itc"; break;
		case AR_PFS: n = "pfs"; break;
		case AR_LC: n = "lc"; break;
		case AR_EC: n = "ec"; break;
		default:
			sprintf(buf, "ar%d", (int)o->o_value);
			return;
		}
		sprintf(buf, "ar.%s", n);
		return;
	case ASM_OPER_BREG:
		if (o->o_value != 0)
			sprintf(buf, "b%d", (int)o->o_value);
		else
			strcpy(buf, "rp");
		return;
	case ASM_OPER_CPUID:
		n = "cpuid";
		break;
	case ASM_OPER_CREG:
		switch ((int)o->o_value) {
		case CR_DCR: n = "dcr"; break;
		case CR_ITM: n = "itm"; break;
		case CR_IVA: n = "iva"; break;
		case CR_PTA: n = "pta"; break;
		case CR_IPSR: n = "ipsr"; break;
		case CR_ISR: n = "isr"; break;
		case CR_IIP: n = "iip"; break;
		case CR_IFA: n = "ifa"; break;
		case CR_ITIR: n = "itir"; break;
		case CR_IIPA: n = "iipa"; break;
		case CR_IFS: n = "ifs"; break;
		case CR_IIM: n = "iim"; break;
		case CR_IHA: n = "iha"; break;
		case CR_LID: n = "lid"; break;
		case CR_IVR: n = "ivr"; break;
		case CR_TPR: n = "tpr"; break;
		case CR_EOI: n = "eoi"; break;
		case CR_IRR0: n = "irr0"; break;
		case CR_IRR1: n = "irr1"; break;
		case CR_IRR2: n = "irr2"; break;
		case CR_IRR3: n = "irr3"; break;
		case CR_ITV: n = "itv"; break;
		case CR_PMV: n = "pmv"; break;
		case CR_CMCV: n = "cmcv"; break;
		case CR_LRR0: n = "lrr0"; break;
		case CR_LRR1: n = "lrr1"; break;
		default:
			sprintf(buf, "cr%d", (int)o->o_value);
			return;
		}
		sprintf(buf, "cr.%s", n);
		return;
	case ASM_OPER_DBR:
		n = "dbr";
		break;
	case ASM_OPER_DISP:
		sprintf(buf, "%lx", ip + o->o_value);
		return;
	case ASM_OPER_DTR:
		n = "dtr";
		break;
	case ASM_OPER_FREG:
		sprintf(buf, "f%d", (int)o->o_value);
		return;
	case ASM_OPER_GREG:
		break;
	case ASM_OPER_IBR:
		n = "ibr";
		break;
	case ASM_OPER_IMM:
		sprintf(buf, "0x%lx", o->o_value);
		return;
	case ASM_OPER_IP:
		strcpy(buf, "ip");
		return;
	case ASM_OPER_ITR:
		n = "itr";
		break;
	case ASM_OPER_MEM:
		n = "";
		break;
	case ASM_OPER_MSR:
		n = "msr";
		break;
	case ASM_OPER_PKR:
		n = "pkr";
		break;
	case ASM_OPER_PMC:
		n = "pmc";
		break;
	case ASM_OPER_PMD:
		n = "pmd";
		break;
	case ASM_OPER_PR:
		strcpy(buf, "pr");
                return;
	case ASM_OPER_PR_ROT:
		strcpy(buf, "pr.rot");
		return;
	case ASM_OPER_PREG:
		sprintf(buf, "p%d", (int)o->o_value);
		return;
	case ASM_OPER_PSR:
		strcpy(buf, "psr");
		return;
	case ASM_OPER_PSR_L:
		strcpy(buf, "psr.l");
		return;
	case ASM_OPER_PSR_UM:
		strcpy(buf, "psr.um");
		return;
	case ASM_OPER_RR:
		n = "rr";
		break;
	case ASM_OPER_NONE:
		KASSERT(0, ("foo"));
		break;
	}
	if (n != NULL)
		buf += sprintf(buf, "%s[", n);
	switch ((int)o->o_value) {
	case 1:	strcpy(buf, "gp"); buf += 2; break;
	case 12: strcpy(buf, "sp"); buf += 2; break;
	case 13: strcpy(buf, "tp"); buf += 2; break;
	default: buf += sprintf(buf, "r%d", (int)o->o_value); break;
	}
	if (n != NULL)
		strcpy(buf, "]");
}

void
asm_print_bundle(const struct asm_bundle *b, uint64_t ip)
{
	asm_print_inst(b, 0, ip);
	asm_print_inst(b, 1, ip);
	asm_print_inst(b, 2, ip);
}

void
asm_print_inst(const struct asm_bundle *b, int slot, uint64_t ip)
{
	char buf[32];
	const struct asm_inst *i;
	const char *tmpl;
	int n, w;

	tmpl = b->b_templ + slot;
	if (*tmpl == ';' || (slot == 2 && b->b_templ[1] == ';'))
		tmpl++;
	i = b->b_inst + slot;
	if (*tmpl == 'L' || i->i_op == ASM_OP_NONE)
		return;

	/* Address + slot. */
	printf("%lx[%c] ", ip + slot, *tmpl);

	/* Predicate. */
	if (i->i_oper[0].o_value != 0) {
		asm_operand(i->i_oper+0, buf, ip);
		w = printf("(%s)", buf);
	} else
		w = 0;
	while (w++ < 8)
		printf(" ");

	/* Mnemonic & completers. */
	asm_mnemonic(i->i_op, buf);
	w = printf(buf);
	n = 0;
	while (n < i->i_ncmpltrs) {
		asm_completer(i->i_cmpltr + n, buf);
		w += printf(buf);
		n++;
	}
	while (w++ < 15)
		printf(" ");
	printf(" ");

	/* Operands. */
	n = 1;
	while (n < 7 && i->i_oper[n].o_type != ASM_OPER_NONE) {
		if (n > 1) {
			if (n == i->i_srcidx)
				printf(" = ");
			else
				printf(", ");
		}
		asm_operand(i->i_oper + n, buf, ip);
		printf(buf);
		n++;
	}
	printf("\n");
}
