/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/param.h>
#include <sys/types.h>
#include <a.out.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <fcntl.h>
#include <stab.h>
#include <string.h>

#include "dynamic.h"

/*
 * Relocation masks and sizes for the Sparc architecture.
 *
 * Note that these are very dependent on the order of the enums in
 * enum reloc_type (in a.out.h); if they change the following must be
 * changed.
 * Also, note that RELOC_RELATIVE is handled as if it were a RELOC_HI22.
 * This should work provided that relocations values have zeroes in their
 * least significant 10 bits. As RELOC_RELATIVE is used only to relocate
 * with load address values - which are page aligned - this condition is
 * fulfilled as long as the system's page size is > 1024 (and a power of 2).
 */
static int reloc_target_rightshift[] = {
	0, 0, 0,	/* RELOC_8, _16, _32 */
	0, 0, 0, 2, 2,	/* DISP8, DISP16, DISP32, WDISP30, WDISP22 */
	10, 0,		/* HI22, _22 */
	0, 0,		/* RELOC_13, _LO10 */
	0, 0,		/* _SFA_BASE, _SFA_OFF13 */
	0, 0, 10,	/* _BASE10, _BASE13, _BASE22 */
	0, 10,		/* _PC10, _PC22 */
	2, 0,		/* _JMP_TBL, _SEGOFF16 */
	0, 0, 0		/* _GLOB_DAT, JMP_SLOT, _RELATIVE */
};
static int reloc_target_size[] = {
	0, 1, 2,	/* RELOC_8, _16, _32 */
	0, 1, 2, 2, 2,	/* DISP8, DISP16, DISP32, WDISP30, WDISP22 */
	2, 2,		/* HI22, _22 */
	2, 2,		/* RELOC_13, _LO10 */
	2, 2,		/* _SFA_BASE, _SFA_OFF13 */
	2, 2, 2,	/* _BASE10, _BASE13, _BASE22 */
	2, 2,		/* _PC10, _PC22 */
	2, 0,		/* _JMP_TBL, _SEGOFF16 */
	2, 0, 2		/* _GLOB_DAT, JMP_SLOT, _RELATIVE */
};
static int reloc_target_bitsize[] = {
	8, 16, 32,	/* RELOC_8, _16, _32 */
	8, 16, 32, 30, 22,	/* DISP8, DISP16, DISP32, WDISP30, WDISP22 */
	22, 22,		/* HI22, _22 */
	13, 10,		/* RELOC_13, _LO10 */
	32, 32,		/* _SFA_BASE, _SFA_OFF13 */
	10, 13, 22,	/* _BASE10, _BASE13, _BASE22 */
	10, 22,		/* _PC10, _PC22 */
	30, 0,		/* _JMP_TBL, _SEGOFF16 */
	32, 0, 22	/* _GLOB_DAT, JMP_SLOT, _RELATIVE */
};


/*
 * Get relocation addend corresponding to relocation record RP
 * ADDR unused by SPARC impl.
 */
long
md_get_addend(r, addr)
struct relocation_info	*r;
unsigned char		*addr;
{
	return r->r_addend;
}

void
md_relocate(r, relocation, addr, relocatable_output)
struct relocation_info	*r;
long			relocation;
unsigned char		*addr;
int			relocatable_output;
{
	register unsigned long	mask;

#ifndef RTLD
	if (relocatable_output) {
		/*
		 * Non-PC relative relocations which are absolute or
		 * which have become non-external now have fixed
		 * relocations.  Set the ADD_EXTRA of this relocation
		 * to be the relocation we have now determined.
		 */
		if (!RELOC_PCREL_P(r)) {
			if ((int) r->r_type <= RELOC_32
					    || RELOC_EXTERN_P(r) == 0)
				RELOC_ADD_EXTRA(r) = relocation;
		} else if (RELOC_EXTERN_P(r))
			/*
			 * External PC-relative relocations continue
			 * to move around; update their relocations
			 * by the amount they have moved so far.
			 */
			RELOC_ADD_EXTRA(r) -= pc_relocation;
		return;
	}
#endif

	relocation >>= RELOC_VALUE_RIGHTSHIFT(r);

	/* Unshifted mask for relocation */
	mask = 1 << RELOC_TARGET_BITSIZE(r) - 1;
	mask |= mask - 1;
	relocation &= mask;

	/* Shift everything up to where it's going to be used */
	relocation <<= RELOC_TARGET_BITPOS(r);
	mask <<= RELOC_TARGET_BITPOS(r);

	switch (RELOC_TARGET_SIZE(r)) {
	case 0:
		if (RELOC_MEMORY_ADD_P(r))
			relocation += (mask & *(u_char *) (addr));
		*(u_char *) (addr) &= ~mask;
		*(u_char *) (addr) |= relocation;
		break;

	case 1:
		if (RELOC_MEMORY_ADD_P(r))
			relocation += (mask & *(u_short *) (addr));
		*(u_short *) (addr) &= ~mask;
		*(u_short *) (addr) |= relocation;
		break;

	case 2:
		if (RELOC_MEMORY_ADD_P(r))
			relocation += (mask & *(u_long *) (addr));
		*(u_long *) (addr) &= ~mask;
		*(u_long *) (addr) |= relocation;
		break;
	default:
		errx(1, "Unimplemented relocation field length: %d",
			RELOC_TARGET_SIZE(r));
	}
}

#ifndef RTLD
/*
 * Machine dependent part of claim_rrs_reloc().
 * On the Sparc the relocation offsets are stored in the r_addend member.
 */
int
md_make_reloc(rp, r, type)
struct relocation_info	*rp, *r;
int			type;
{
	r->r_type = rp->r_type;
	r->r_addend = rp->r_addend;

#if 1
	/*
	 * This wouldn't be strictly necessary - we could record the
	 * relocation value "in situ" in stead of in the r_addend field -
	 * but we are being Sun compatible here. Besides, Sun's ld.so
	 * has a bug that prevents it from handling this alternate method.
	 *
	 * IT WOULD BE REALLY NICE TO HAVE CONSISTENCY THROUGHOUT THE ENTIRE
	 * RELOCATION PROCESS, ie. using `r_addend' for storing all partially
	 * completed relocations, in stead of mixing them in both relocation
	 * records and in the segment data.
	 */
	if (RELOC_PCREL_P(rp))
		r->r_addend -= pc_relocation;
#endif

	return 1;
}
#endif

/*
 * Set up a transfer from jmpslot at OFFSET (relative to the PLT table)
 * to the binder slot (which is at offset 0 of the PLT).
 */
void
md_make_jmpslot(sp, offset, index)
jmpslot_t		*sp;
long			offset;
long			index;
{
	u_long	fudge = (u_long) -(sizeof(sp->opcode1) + offset);
	sp->opcode1 = SAVE;
	/* The following is a RELOC_WDISP30 relocation */
	sp->opcode2 = CALL | ((fudge >> 2) & 0x3fffffff);
	sp->reloc_index = NOP | index;
}

/*
 * Set up a "direct" transfer (ie. not through the run-time binder) from
 * jmpslot at OFFSET to ADDR. Used by `ld' when the SYMBOLIC flag is on,
 * and by `ld.so' after resolving the symbol.
 * On the i386, we use the JMP instruction which is PC relative, so no
 * further RRS relocations will be necessary for such a jmpslot.
 *
 * OFFSET unused on Sparc.
 */
void
md_fix_jmpslot(sp, offset, addr)
jmpslot_t	*sp;
long		offset;
u_long		addr;
{
	/*
	 * Here comes a RELOC_{LO10,HI22} relocation pair
	 * The resulting code is:
	 *	sethi	%hi(addr), %g1
	 *	jmp	%g1+%lo(addr)
	 *	nop	! delay slot
	 */
	sp->opcode1 = SETHI | ((addr >> 10) & 0x003fffff);
	sp->opcode2 = JMP | (addr & 0x000003ff);
	sp->reloc_index = NOP;
}

/*
 * Update the relocation record for a jmpslot.
 */
void
md_make_jmpreloc(rp, r, type)
struct relocation_info	*rp, *r;
int			type;
{
	if (type & RELTYPE_RELATIVE)
		r->r_type = RELOC_RELATIVE;
	else
		r->r_type = RELOC_JMP_SLOT;

	r->r_addend = rp->r_addend;
}

/*
 * Set relocation type for a GOT RRS relocation.
 */
void
md_make_gotreloc(rp, r, type)
struct relocation_info	*rp, *r;
int			type;
{
	/*
	 * GOT value resolved (symbolic or entry point): R_32
	 * GOT not resolved: GLOB_DAT
	 *
	 * NOTE: I don't think it makes a difference.
	 */
	if (type & RELTYPE_RELATIVE)
		r->r_type = RELOC_32;
	else
		r->r_type = RELOC_GLOB_DAT;

	r->r_addend = 0;
}

/*
 * Set relocation type for a RRS copy operation.
 */
void
md_make_cpyreloc(rp, r)
struct relocation_info	*rp, *r;
{
	r->r_type = RELOC_COPY_DAT;
	r->r_addend = 0;
}

void
md_set_breakpoint(where, savep)
long	where;
long	*savep;
{
	*savep = *(long *)where;
	*(long *)where = TRAP;
}

#ifndef RTLD
/*
 * Initialize (output) exec header such that useful values are
 * obtained from subsequent N_*() macro evaluations.
 */
void
md_init_header(hp, magic, flags)
struct exec	*hp;
int		magic, flags;
{
#ifdef NetBSD
	N_SETMAGIC((*hp), magic, MID_MACHINE, flags);

	/* TEXT_START depends on the value of outheader.a_entry.  */
	if (!(link_mode & SHAREABLE)) /*WAS: if (entry_symbol) */
		hp->a_entry = PAGSIZ;
#else
	hp->a_magic = magic;
	hp->a_machtype = M_SPARC;
	hp->a_toolversion = 1;
	hp->a_dynamic = ((flags) & EX_DYNAMIC);

	/* SunOS 4.1 N_TXTADDR depends on the value of outheader.a_entry.  */
	if (!(link_mode & SHAREABLE)) /*WAS: if (entry_symbol) */
		hp->a_entry = N_PAGSIZ(*hp);
#endif
}

/*
 * Check for acceptable foreign machine Ids
 */
int
md_midcompat(hp)
struct exec *hp;
{
#ifdef NetBSD
#define SUN_M_SPARC	3
	return (((md_swap_long(hp->a_midmag)&0x00ff0000) >> 16) == SUN_M_SPARC);
#else
	return hp->a_machtype == M_SPARC;
#endif
}
#endif /* RTLD */
