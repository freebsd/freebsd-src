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

#define	E

#define	_LD(w, a)	ld ## w ## a
#define	_ST(w, a)	st ## w ## a

#define	LD(w, a)	_LD(w, a)
#define	ST(w, a)	_ST(w, a)

#define	_BCOPY(src, dst, len, sa, sasi, da, dasi) \
	brz,pn	len, 2f ; \
	 mov	len, %o3 ; \
1:	LD(ub, sa) [src] sasi, %o4 ; \
	ST(b, da) %o4, [dst] dasi ; \
	dec	%o3 ; \
	inc	src ; \
	brnz,pt	%o3, 1b ; \
	 inc	dst ; \
2:

#define	BCOPY(src, dst, len) \
	_BCOPY(src, dst, len, E, E, E, E)

#define	COPYIN(uaddr, kaddr, len) \
	wr	%g0, ASI_AIUP, %asi ; \
	_BCOPY(uaddr, kaddr, len, a, %asi, E, E)

#define	COPYOUT(kaddr, uaddr, len) \
	wr	%g0, ASI_AIUP, %asi ; \
	_BCOPY(kaddr, uaddr, len, E, E, a, %asi)

#define	_COPYSTR(src, dst, len, done, sa, sasi, da, dasi) \
	clr	%o4 ; \
	clr	%o5 ; \
1:	LD(ub, sa) [src] sasi, %g1 ; \
	ST(b, da) %g1, [dst] dasi ; \
	brz,pn	%g1, 2f ; \
	 inc	%o4 ; \
	dec	len ; \
	inc	src ; \
	brgz,pt	len, 1b ; \
	 inc	dst ; \
	mov	ENAMETOOLONG, %o5 ; \
2:	brnz,a	done, 3f ; \
	 stx	%o4, [done] ; \
3:

#define	COPYSTR(dst, src, len, done) \
	_COPYSTR(dst, src, len, done, E, E, E, E)

#define	COPYINSTR(uaddr, kaddr, len, done) \
	wr	%g0, ASI_AIUP, %asi ; \
	_COPYSTR(uaddr, kaddr, len, done, a, %asi, E, E)

#define	CATCH_SETUP(label) \
	setx	label, %g2, %g1 ; \
	ldx	[PCPU(CURPCB)], %g6 ; \
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
	retl ; \
	 CATCH_END()

#define	SU_BYTES(storer, size, label) \
	btst	(size) - 1, %o0 ; \
	bnz,pn	%xcc, .Lfsalign ; \
	 EMPTY ; \
	SU_ALIGNED(storer, label)

/*
 * void bcmp(void *b, size_t len)
 */
ENTRY(bcmp)
	brz,pn	%o2, 2f
	 clr	%o3
1:	ldub	[%o0 + %o3], %o4
	ldub	[%o1 + %o3], %o5
	cmp	%o4, %o5
	bne,pn	%xcc, 1f
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
ENTRY(bcopy)
	BCOPY(%o0, %o1, %o2)
	retl
	 nop
END(bcopy)

/*
 * void ovbcopy(const void *src, void *dst, size_t len)
 * XXX handle overlap...
 */
ENTRY(ovbcopy)
	BCOPY(%o0, %o1, %o2)
	retl
	 nop
END(ovbcopy)

/*
 * void bzero(void *b, size_t len)
 */
ENTRY(bzero)
	brz,pn	%o1, 1f
	 nop
1:	deccc	%o1
	stb	%g0, [%o0]
	bne,pt	%xcc, 1b
	 inc	%o0
2:	retl
	 nop
END(bzero)

/*
 * void *memcpy(void *dst, const void *src, size_t len)
 */
ENTRY(memcpy)
	BCOPY(%o1, %o0, %o2)
	retl
	 nop
END(memcpy)

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
	COPYIN(%o0, %o1, %o2)
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
	COPYINSTR(%o0, %o1, %o2, %o3)
	CATCH_END()
	retl
	 mov	%o5, %o0
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
	COPYOUT(%o0, %o1, %o2)
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
	COPYSTR(%o0, %o1, %o2, %o3)
	retl
	 mov	%o5, %o0
END(copystr)

/*
 * int fubyte(const void *base)
 */
ENTRY(fubyte)
	FU_ALIGNED(lduba, .Lfsfault)
END(fubyte)

/*
 * int fusword(const void *base)
 */
ENTRY(fusword)
	FU_BYTES(lduwa, 2, .Lfsfault)
END(fusword)

/*
 * int fuswintr(const void *base)
 */
ENTRY(fuswintr)
	FU_BYTES(lduwa, 2, fsbail)
END(fuswintr)

/*
 * int fuword(const void *base)
 */
ENTRY(fuword)
	FU_BYTES(ldxa, 8, .Lfsfault)
END(fuword)

/*
 * int subyte(const void *base)
 */
ENTRY(subyte)
	SU_ALIGNED(stba, .Lfsfault)
END(subyte)

/*
 * int suibyte(const void *base)
 */
ENTRY(suibyte)
	SU_ALIGNED(stba, fsbail)
END(suibyte)

/*
 * int susword(const void *base)
 */
ENTRY(susword)
	SU_BYTES(stwa, 2, .Lfsfault)
END(susword)

/*
 * int suswintr(const void *base)
 */
ENTRY(suswintr)
	SU_BYTES(stwa, 2, fsbail)
END(suswintr)

/*
 * int suword(const void *base)
 */
ENTRY(suword)
	SU_BYTES(stxa, 8, .Lfsfault)
END(suword)

ENTRY(fsbail)
	nop
.Lfsfault:
	CATCH_END()
.Lfsalign:
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
 * Temporary stack for calling into the firmware. We need to setup one, because
 * the MMU mapping for our stack page may be lost. When the firmware tries to
 * spill the last window (the others are flushed before), this results in an
 * DMMU miss trap, which is fatal with the firmware trap handlers installed.
 * Additionally, it seems that the firmware does not immediately switch to an
 * own stack (or maybe never?), therefore more space needs to be reserved.
 * I hope this is sufficient now.
 */
	.align	4
DATA(ofwstack)
	.rept	CCFSZ * 8
	.byte	0
	.endr
ofwstack_last:
	.rept	CCFSZ
	.byte	0
	.endr
END(ofwstack)

/*
 * void openfirmware(cell_t args[])
 */
ENTRY(openfirmware)
	/*
	 * Disable interrupts. The firmware should not deal with our interrupts
	 * anyway, and the temporary stack is not large enough to hold the stack
	 * footprint of the interrrupt handling.
	 */
	rdpr	%pstate, %o3
	andn	%o3, PSTATE_IE, %o1
	wrpr	%o1, 0, %pstate
	setx	ofwstack_last - SPOFF, %o1, %o2
	save	%o2, 0, %sp
	flushw
	rdpr	%tl, %l1
	rdpr	%tba, %l2
	mov	AA_DMMU_PCXR, %l3
	ldxa	[%l3] ASI_DMMU, %l4
	stxa	%g0, [%l3] ASI_DMMU
	membar	#Sync
	flush	%sp
	setx	ofw_tba, %l7, %l5
	ldx	[%l5], %l5
	setx	ofw_vec, %l7, %l6
	ldx	[%l6], %l6
	rdpr	%pil, %l7
	wrpr	%g0, 14, %pil
	wrpr	%l5, 0, %tba
	wrpr	%g0, 0, %tl
	call	%l6
	 mov	%i0, %o0
	wrpr	%l1, 0, %tl
	wrpr	%l2, 0, %tba
	stxa	%l4, [%l3] ASI_DMMU
	wrpr	%l7, 0, %pil
	membar	#Sync
	flush	%sp
	restore
	retl
	 wrpr	%o3, 0, %pstate
END(openfirmware)
