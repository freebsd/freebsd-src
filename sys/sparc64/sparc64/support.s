/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/ktr.h>
#include <machine/pstate.h>

#include "assym.s"

#define	E	/* empty */

/*
 * Generate load and store instructions for the corresponding width and asi
 * (or not).  Note that we want to evaluate the macro args before
 * concatenating, so that E really turns into nothing.
 */
#define	_LD(w, a)	ld ## w ## a
#define	_ST(w, a)	st ## w ## a

#define	LD(w, a)	_LD(w, a)
#define	ST(w, a)	_ST(w, a)

/*
 * Common code for copy routines.
 *
 * We use large macros to generate functions for each of the copy routines.
 * This allows the load and store instructions to be generated for the right
 * operation, asi or not.  It is possible to write an asi independent function
 * but this would require 2 expensive wrs in the main loop to switch %asi.
 * It would also screw up profiling (if we ever get it), but may save some I$.
 * We assume that either one of dasi and sasi is empty, or that they are both
 * the same (empty or non-empty).  It is up to the caller to set %asi.
 */

/*
 * ASI independent implementation of copystr(9).
 * Used to implement copyinstr() and copystr().
 *
 * Return value is in %g1.
 */
#define	_COPYSTR(src, dst, len, done, sa, sasi, da, dasi) \
	brz	len, 4f ; \
	 mov	src, %g2 ; \
1:	deccc	1, len ; \
	bl,a,pn	%xcc, 3f ; \
	 nop ; \
	LD(ub, sa) [src] sasi, %g1 ; \
	ST(b, da) %g1, [dst] dasi ; \
	brz,pn	%g1, 3f ; \
	 inc	src ; \
	b	%xcc, 1b ; \
	 inc	dst ; \
2:	mov	ENAMETOOLONG, %g1 ; \
3:	sub	src, %g2, %g2 ; \
	brnz,a	done, 4f ; \
	 stx	%g2, [done] ; \
4:

/*
 * ASI independent implementation of memset(3).
 * Used to implement bzero(), memset() and physzero().
 *
 * If the pattern is non-zero, duplicate it to fill 64 bits.
 * Store bytes until dst is 8-byte aligned, then store 8 bytes.
 * It has yet to be determined how much unrolling is beneficial.
 * Could also read and compare before writing to minimize snoop traffic.
 *
 * XXX bzero() should be implemented as
 * #define bzero(dst, len) (void)memset((dst), 0, (len))
 * if at all.
 */
#define	_MEMSET(dst, pat, len, da, dasi) \
	brlez,pn len, 5f ; \
	 and	pat, 0xff, pat ; \
	brz,pt	pat, 1f ; \
	 sllx	pat, 8, %g1 ; \
	or	pat, %g1, pat ; \
	sllx	pat, 16, %g1 ; \
	or	pat, %g1, pat ; \
	sllx	pat, 32, %g1 ; \
	or	pat, %g1, pat ; \
	.align	16 ; \
1:	deccc	1, len ; \
	bl,pn	%xcc, 5f ; \
	 btst	7, dst ; \
	bz,a,pt	%xcc, 2f ; \
	 inc	1, len ; \
	ST(b, da) pat, [dst] dasi ; \
	b	%xcc, 1b ; \
	 inc	dst ; \
	.align	16 ; \
2:	deccc	32, len ; \
	bl,a,pn	%xcc, 3f ; \
	 inc	32, len ; \
	ST(x, da) pat, [dst] dasi ; \
	ST(x, da) pat, [dst + 8] dasi ; \
	ST(x, da) pat, [dst + 16] dasi ; \
	ST(x, da) pat, [dst + 24] dasi ; \
	b	%xcc, 2b ; \
	 inc	32, dst ; \
	.align	16 ; \
3:	deccc	8, len ; \
	bl,a,pn	%xcc, 4f ; \
	 inc	8, len ; \
	ST(x, da) pat, [dst] dasi ; \
	b	%xcc, 3b ; \
	 inc	8, dst ; \
	.align	16 ; \
4:	deccc	1, len ; \
	bl,a,pn	%xcc, 5f ; \
	 nop ; \
	ST(b, da) pat, [dst] dasi ; \
	b	%xcc, 4b ; \
	 inc	1, dst ; \
5:

/*
 * ASI independent implementation of memcpy(3).
 * Used to implement bcopy(), copyin(), copyout(), memcpy(), and physcopy().
 *
 * Transfer bytes until dst is 8-byte aligned.  If src is then also 8 byte
 * aligned, transfer 8 bytes, otherwise finish with bytes.  The unaligned
 * case could be optimized, but it is expected that this is the uncommon
 * case and of questionable value.  The code to do so is also rather large
 * and ugly.
 * It has yet to be determined how much unrolling is beneficial.
 *
 * XXX bcopy() must also check for overlap.  This is stupid.
 * XXX bcopy() should be implemented as
 * #define bcopy(src, dst, len) (void)memcpy((dst), (src), (len))
 * if at all.
 */
#define	_MEMCPY(dst, src, len, da, dasi, sa, sasi) \
1:	deccc	1, len ; \
	bl,pn	%xcc, 6f ; \
	 btst	7, dst ; \
	bz,a,pt	%xcc, 2f ; \
	 inc	1, len ; \
	LD(ub, sa) [src] sasi, %g1 ; \
	ST(b, da) %g1, [dst] dasi ; \
	inc	1, src ; \
	b	%xcc, 1b ; \
	 inc	1, dst ; \
	.align	16 ; \
2:	btst	7, src ; \
	bz,a,pt	%xcc, 3f ; \
	 nop ; \
	b,a	%xcc, 5f ; \
	.align	16 ; \
3:	deccc	32, len ; \
	bl,a,pn	%xcc, 4f ; \
	 inc	32, len ; \
	LD(x, sa) [src] sasi, %g1 ; \
	LD(x, sa) [src + 8] sasi, %g2 ; \
	LD(x, sa) [src + 16] sasi, %g3 ; \
	LD(x, sa) [src + 24] sasi, %g4 ; \
	ST(x, da) %g1, [dst] dasi ; \
	ST(x, da) %g2, [dst + 8] dasi ; \
	ST(x, da) %g3, [dst + 16] dasi ; \
	ST(x, da) %g4, [dst + 24] dasi ; \
	inc	32, src ; \
	b	%xcc, 3b ; \
	 inc	32, dst ; \
	.align	16 ; \
4:	deccc	8, len ; \
	bl,a,pn	%xcc, 5f ; \
	 inc	8, len ; \
	LD(x, sa) [src] sasi, %g1 ; \
	ST(x, da) %g1, [dst] dasi ; \
	inc	8, src ; \
	b	%xcc, 4b ; \
	 inc	8, dst ; \
	.align	16 ; \
5:	deccc	1, len ; \
	bl,a,pn	%xcc, 6f ; \
	 nop ; \
	LD(ub, sa) [src] sasi, %g1 ; \
	ST(b, da) %g1, [dst] dasi ; \
	inc	src ; \
	b	%xcc, 5b ; \
	 inc	dst ; \
6:

#define	CATCH_SETUP(label) \
	setx	label, %g2, %g1 ; \
	ldx	[PCPU(CURTHREAD)], %g6 ; \
	ldx	[%g6 + TD_PCB], %g6 ; \
	stx	%g1, [%g6 + PCB_ONFAULT] ;

#define	CATCH_END() \
	stx	%g0, [%g6 + PCB_ONFAULT] ;

#define	FU_ALIGNED(loader, label) \
	CATCH_SETUP(label) ; \
	loader	[%o0] ASI_AIUP, %o0 ; \
	retl ; \
	 CATCH_END()

#define	FU_BYTES(loader, size, label) \
	btst	(size) - 1, %o0 ; \
	bnz,pn	%xcc, .Lfsalign ; \
	 EMPTY ; \
	FU_ALIGNED(loader, label)

#define	SU_ALIGNED(storer, label) \
	CATCH_SETUP(label) ; \
	storer	%o1, [%o0] ASI_AIUP ; \
	CATCH_END() ; \
	retl ; \
	 clr	%o0

#define	SU_BYTES(storer, size, label) \
	btst	(size) - 1, %o0 ; \
	bnz,pn	%xcc, .Lfsalign ; \
	 EMPTY ; \
	SU_ALIGNED(storer, label)

/*
 * int bcmp(const void *b1, const void *b2, size_t len)
 */
ENTRY(bcmp)
	brz,pn	%o2, 2f
	 clr	%o3
1:	ldub	[%o0 + %o3], %o4
	ldub	[%o1 + %o3], %o5
	cmp	%o4, %o5
	bne,pn	%xcc, 2f
	 inc	%o3
	deccc	%o2
	bne,pt	%xcc, 1b
	 nop
2:	retl
	 mov	%o2, %o0
END(bcmp)

/*
 * void bcopy(const void *src, void *dst, size_t len)
 */
ENTRY(ovbcopy)
ENTRY(bcopy)
	/*
	 * Check for overlap, and copy backwards if so.
	 */
	sub	%o1, %o0, %g1
	cmp	%g1, %o2
	bgeu,a,pt %xcc, 3f
	 nop

	/*
	 * Copy backwards.
	 */
	add	%o0, %o2, %o0
	add	%o1, %o2, %o1
1:	deccc	1, %o2
	bl,a,pn	%xcc, 2f
	 nop
	dec	1, %o0
	ldub	[%o0], %g1
	dec	1, %o1
	b	%xcc, 1b
	 stb	%g1, [%o1]
2:	retl
	 nop

	/*
	 * Do the fast version.
	 */
3:	_MEMCPY(%o1, %o0, %o2, E, E, E, E)
	retl
	 nop
END(bcopy)

/*
 * void bzero(void *b, size_t len)
 */
ENTRY(bzero)
	_MEMSET(%o0, %g0, %o1, E, E)
	retl
	 nop
END(bzero)

/*
 * void physzero(vm_offset_t pa, size_t len)
 */
ENTRY(physzero)
	wr	%g0, ASI_PHYS_USE_EC, %asi
	_MEMSET(%o0, %g0, %o1, a, %asi)
	retl
	 nop
END(physzero)

/*
 * void physcopy(vm_offset_t src, vm_offset_t dst, size_t len)
 */
ENTRY(physcopy)
	wr	%g0, ASI_PHYS_USE_EC, %asi
	_MEMCPY(%o1, %o0, %o2, a, %asi, a, %asi)
	retl
	 nop
END(physcopy)

/*
 * void *memcpy(void *dst, const void *src, size_t len)
 */
ENTRY(memcpy)
	mov	%o0, %o3
	_MEMCPY(%o3, %o1, %o2, E, E, E, E)
	retl
	 nop
END(memcpy)

/*
 * void *memset(void *b, int c, size_t len)
 */
ENTRY(memset)
	mov	%o0, %o3
	_MEMSET(%o3, %o1, %o2, E, E)
	retl
	 nop
END(memset)

/*
 * int copyin(const void *uaddr, void *kaddr, size_t len)
 */
ENTRY(copyin)
	CATCH_SETUP(.Lefault)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "copyin: ua=%#lx ka=%#lx len=%ld"
	    , %o3, %o4, %o5, 7, 8, 9)
	stx	%o0, [%o3 + KTR_PARM1]
	stx	%o1, [%o3 + KTR_PARM2]
	stx	%o2, [%o3 + KTR_PARM3]
9:
#endif
	wr	%g0, ASI_AIUP, %asi
	_MEMCPY(%o1, %o0, %o2, E, E, a, %asi)
	CATCH_END()
	retl
	 clr	%o0
END(copyin)

/*
 * int copyinstr(const void *uaddr, void *kaddr, size_t len, size_t *done)
 */
ENTRY(copyinstr)
	CATCH_SETUP(.Lefault)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "copyinstr: ua=%#lx ka=%#lx len=%ld done=%p"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
	stx	%o1, [%g1 + KTR_PARM2]
	stx	%o2, [%g1 + KTR_PARM3]
	stx	%o3, [%g1 + KTR_PARM4]
9:
#endif
	wr	%g0, ASI_AIUP, %asi
	_COPYSTR(%o0, %o1, %o2, %o3, a, %asi, E, E)
	CATCH_END()
	retl
	 mov	%g1, %o0
END(copyinstr)

/*
 * int copyout(const void *kaddr, void *uaddr, size_t len)
 */
ENTRY(copyout)
	CATCH_SETUP(.Lefault)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "copyout: ka=%#lx ua=%#lx len=%ld"
	    , %o3, %o4, %o5, 7, 8, 9)
	stx	%o0, [%o3 + KTR_PARM1]
	stx	%o1, [%o3 + KTR_PARM2]
	stx	%o2, [%o3 + KTR_PARM3]
9:
#endif
	wr	%g0, ASI_AIUP, %asi
	_MEMCPY(%o1, %o0, %o2, a, %asi, E, E)
	CATCH_END()
	retl
	 clr	%o0
END(copyout)

.Lefault:
	CATCH_END()
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "copy{in,out}: return efault"
	    , %o0, %o1, %o2, 7, 8, 9)
9:
#endif
	retl
	 mov	EFAULT, %o0

/*
 * int copystr(const void *src, void *dst, size_t len, size_t *done)
 */
ENTRY(copystr)
	_COPYSTR(%o0, %o1, %o2, %o3, E, E, E, E)
	retl
	 mov	%g1, %o0
END(copystr)

/*
 * int fubyte(const void *base)
 */
ENTRY(fubyte)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "fubyte: base=%#lx", %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif
	FU_ALIGNED(lduba, .Lfsfault)
END(fubyte)

/*
 * int fusword(const void *base)
 */
ENTRY(fusword)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "fusword: base=%#lx", %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif
	FU_BYTES(lduwa, 2, .Lfsfault)
END(fusword)

/*
 * int fuswintr(const void *base)
 */
ENTRY(fuswintr)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "fuswintr: base=%#lx", %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif
	FU_BYTES(lduwa, 2, fsbail)
END(fuswintr)

/*
 * int fuword(const void *base)
 */
ENTRY(fuword)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "fuword: base=%#lx", %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif
	FU_BYTES(ldxa, 8, .Lfsfault)
END(fuword)

/*
 * int subyte(const void *base)
 */
ENTRY(subyte)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "subyte: base=%#lx", %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif
	SU_ALIGNED(stba, .Lfsfault)
END(subyte)

/*
 * int suibyte(const void *base)
 */
ENTRY(suibyte)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "suibyte: base=%#lx", %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif
	SU_ALIGNED(stba, fsbail)
END(suibyte)

/*
 * int susword(const void *base)
 */
ENTRY(susword)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "susword: base=%#lx", %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif
	SU_BYTES(stwa, 2, .Lfsfault)
END(susword)

/*
 * int suswintr(const void *base)
 */
ENTRY(suswintr)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "suswintr: base=%#lx", %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif
	SU_BYTES(stwa, 2, fsbail)
END(suswintr)

/*
 * int suword(const void *base)
 */
ENTRY(suword)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "suword: base=%#lx", %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif
	SU_BYTES(stxa, 8, .Lfsfault)
END(suword)

	.align 16
.Lfsalign:
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "{f,s}u*: alignment", %g1, %g2, %g3, 7, 8, 9)
9:
#endif
	retl
	 mov	-1, %o0

	.align 16
.Lfsfault:
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "{f,s}u*: fault", %g1, %g2, %g3, 7, 8, 9)
9:
#endif
	CATCH_END()
	retl
	 mov	-1, %o0

ENTRY(fsbail)
#if KTR_COMPILE & KTR_CT1
	CATR(KTR_CT1, "{f,s}uswintr: bail", %g1, %g2, %g3, 7, 8, 9)
9:
#endif
	CATCH_END()
	retl
	 mov	-1, %o0
END(fsbail)

ENTRY(longjmp)
	set	1, %g3
	movrz	%o1, %o1, %g3
	mov	%o0, %g1
	ldx	[%g1 + JB_FP], %g2
1:	cmp	%fp, %g2
	bl,a,pt	%xcc, 1b
	 restore
	bne,pn	%xcc, 2f
	 ldx	[%g1 + JB_SP], %o2
	ldx	[%g1 + JB_PC], %o3
	cmp	%o2, %sp
	blt,pn	%xcc, 2f
	 movge	%xcc, %o2, %sp
	jmp	%o3 + 8
	 mov	%g3, %o0
2:	PANIC("longjmp botch", %l1)
END(longjmp)

ENTRY(setjmp)
	stx	%sp, [%o0 + JB_SP]
	stx	%o7, [%o0 + JB_PC]
	stx	%fp, [%o0 + JB_FP]
	retl
	 clr	%o0
END(setjmp)

/*
 * void openfirmware(cell_t args[])
 */
ENTRY(openfirmware)
	save	%sp, -CCFSZ, %sp
	setx	ofw_vec, %l7, %l6
	ldx	[%l6], %l6
	rdpr	%pil, %l7
	wrpr	%g0, PIL_TICK, %pil
	call	%l6
	 mov	%i0, %o0
	wrpr	%l7, 0, %pil
	ret
	 restore %o0, %g0, %o0
END(openfirmware)
