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

	.register %g2, #ignore
	.register %g3, #ignore
	.register %g6, #ignore

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
 * Used to implement bzero(), memset() and aszero().
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
	_ALIGN_TEXT ; \
1:	deccc	1, len ; \
	bl,pn	%xcc, 5f ; \
	 btst	7, dst ; \
	bz,a,pt	%xcc, 2f ; \
	 inc	1, len ; \
	ST(b, da) pat, [dst] dasi ; \
	b	%xcc, 1b ; \
	 inc	dst ; \
	_ALIGN_TEXT ; \
2:	deccc	32, len ; \
	bl,a,pn	%xcc, 3f ; \
	 inc	32, len ; \
	ST(x, da) pat, [dst] dasi ; \
	ST(x, da) pat, [dst + 8] dasi ; \
	ST(x, da) pat, [dst + 16] dasi ; \
	ST(x, da) pat, [dst + 24] dasi ; \
	b	%xcc, 2b ; \
	 inc	32, dst ; \
	_ALIGN_TEXT ; \
3:	deccc	8, len ; \
	bl,a,pn	%xcc, 4f ; \
	 inc	8, len ; \
	ST(x, da) pat, [dst] dasi ; \
	b	%xcc, 3b ; \
	 inc	8, dst ; \
	_ALIGN_TEXT ; \
4:	deccc	1, len ; \
	bl,a,pn	%xcc, 5f ; \
	 nop ; \
	ST(b, da) pat, [dst] dasi ; \
	b	%xcc, 4b ; \
	 inc	1, dst ; \
5:

/*
 * ASI independent implementation of memcpy(3).
 * Used to implement bcopy(), copyin(), copyout(), memcpy(), ascopy(),
 * ascopyfrom() and ascopyto().
 *
 * Transfer bytes until dst is 8-byte aligned.  If src is then also 8 byte
 * aligned, transfer 8 bytes, otherwise finish with bytes.  The unaligned
 * case could be optimized, but it is expected that this is the uncommon
 * case and of questionable value.  The code to do so is also rather large
 * and ugly.  It has yet to be determined how much unrolling is beneficial.
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
	_ALIGN_TEXT ; \
2:	btst	7, src ; \
	bz,a,pt	%xcc, 3f ; \
	 nop ; \
	b,a	%xcc, 5f ; \
	_ALIGN_TEXT ; \
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
	_ALIGN_TEXT ; \
4:	deccc	8, len ; \
	bl,a,pn	%xcc, 5f ; \
	 inc	8, len ; \
	LD(x, sa) [src] sasi, %g1 ; \
	ST(x, da) %g1, [dst] dasi ; \
	inc	8, src ; \
	b	%xcc, 4b ; \
	 inc	8, dst ; \
	_ALIGN_TEXT ; \
5:	deccc	1, len ; \
	bl,a,pn	%xcc, 6f ; \
	 nop ; \
	LD(ub, sa) [src] sasi, %g1 ; \
	ST(b, da) %g1, [dst] dasi ; \
	inc	src ; \
	b	%xcc, 5b ; \
	 inc	dst ; \
6:

/*
 * void ascopy(u_long asi, vm_offset_t src, vm_offset_t dst, size_t len)
 */
ENTRY(ascopy)
	wr	%o0, 0, %asi
	_MEMCPY(%o2, %o1, %o3, a, %asi, a, %asi)
	retl
	 nop
END(ascopy)

/*
 * void ascopyfrom(u_long sasi, vm_offset_t src, caddr_t dst, size_t len)
 */
ENTRY(ascopyfrom)
	wr	%o0, 0, %asi
	_MEMCPY(%o2, %o1, %o3, E, E, a, %asi)
	retl
	 nop
END(ascopyfrom)

/*
 * void ascopyto(caddr_t src, u_long dasi, vm_offset_t dst, size_t len)
 */
ENTRY(ascopyto)
	wr	%o1, 0, %asi
	_MEMCPY(%o2, %o0, %o3, a, %asi, E, E)
	retl
	 nop
END(ascopyto)

/*
 * void aszero(u_long asi, vm_offset_t pa, size_t len)
 */
ENTRY(aszero)
	wr	%o0, 0, %asi
	_MEMSET(%o1, %g0, %o2, a, %asi)
	retl
	 nop
END(aszero)

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
ENTRY(bcopy)
ENTRY(ovbcopy)
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
 * int copystr(const void *src, void *dst, size_t len, size_t *done)
 */
ENTRY(copystr)
	_COPYSTR(%o0, %o1, %o2, %o3, E, E, E, E)
	retl
	 mov	%g1, %o0
END(copystr)

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

	.globl	copy_nofault_begin
copy_nofault_begin:
	nop

/*
 * int copyin(const void *uaddr, void *kaddr, size_t len)
 */
ENTRY(copyin)
	wr	%g0, ASI_AIUP, %asi
	_MEMCPY(%o1, %o0, %o2, E, E, a, %asi)
	retl
	 clr	%o0
END(copyin)

/*
 * int copyinstr(const void *uaddr, void *kaddr, size_t len, size_t *done)
 */
ENTRY(copyinstr)
	wr	%g0, ASI_AIUP, %asi
	_COPYSTR(%o0, %o1, %o2, %o3, a, %asi, E, E)
	retl
	 mov	%g1, %o0
END(copyinstr)

/*
 * int copyout(const void *kaddr, void *uaddr, size_t len)
 */
ENTRY(copyout)
	wr	%g0, ASI_AIUP, %asi
	_MEMCPY(%o1, %o0, %o2, a, %asi, E, E)
	retl
	 clr	%o0
END(copyout)

	.globl	copy_nofault_end
copy_nofault_end:
	nop

ENTRY(copy_fault)
	retl
	 mov	EFAULT, %o0
END(copy_fault)

	.globl	fs_nofault_begin
fs_nofault_begin:
	nop

/*
 * Chatty aliases for fetch, store functions.
 */
	.globl	fubyte, fusword, fuword, subyte, susword, suword
	.set	fubyte, fuword8
	.set	fusword, fuword16
	.set	fuword, fuword64
	.set	subyte, suword8
	.set	susword, suword16
	.set	suword, suword64

/*
 * int fuword8(const void *base)
 */
ENTRY(fuword8)
	retl
	 lduba	[%o0] ASI_AIUP, %o0
END(fuword8)

/*
 * int fuword16(const void *base)
 */
ENTRY(fuword16)
	retl
	 lduha	[%o0] ASI_AIUP, %o0
END(fuword16)

/*
 * int32_t fuword32(const void *base)
 */
ENTRY(fuword32)
	retl
	 lduwa	[%o0] ASI_AIUP, %o0
END(fuword32)

/*
 * int64_t fuword64(const void *base)
 */
ENTRY(fuword64)
	retl
	 ldxa	[%o0] ASI_AIUP, %o0
END(fuword64)

/*
 * int suword8(const void *base, int word)
 */
ENTRY(suword8)
	stba	%o1, [%o0] ASI_AIUP
	retl
	 clr	%o0
END(suword8)

/*
 * int suword16(const void *base, int word)
 */
ENTRY(suword16)
	stha	%o1, [%o0] ASI_AIUP
	retl
	 clr	%o0
END(suword16)

/*
 * int suword32(const void *base, int32_t word)
 */
ENTRY(suword32)
	stwa	%o1, [%o0] ASI_AIUP
	retl
	 clr	%o0
END(suword32)

/*
 * int suword64(const void *base, int64_t word)
 */
ENTRY(suword64)
	stxa	%o1, [%o0] ASI_AIUP
	retl
	 clr	%o0
END(suword64)

	.globl	fs_nofault_intr_begin
fs_nofault_intr_begin:
	nop

/*
 * int fuswintr(const void *base)
 */
ENTRY(fuswintr)
	retl
	 lduha	[%o0] ASI_AIUP, %o0
END(fuswintr)

/*
 * int suswintr(const void *base, int word)
 */
ENTRY(suswintr)
	stha	%o1, [%o0] ASI_AIUP
	retl
	 clr	%o0
END(suswintr)

	.globl	fs_nofault_intr_end
fs_nofault_intr_end:
	nop

	.globl	fs_nofault_end
fs_nofault_end:
	nop

ENTRY(fs_fault)
	retl
	 mov	-1, %o0
END(fsfault)

ENTRY(longjmp)
	set	1, %g3
	movrz	%o1, %o1, %g3
	mov	%o0, %g1
	ldx	[%g1 + _JB_FP], %g2
1:	cmp	%fp, %g2
	bl,a,pt	%xcc, 1b
	 restore
	bne,pn	%xcc, 2f
	 ldx	[%g1 + _JB_SP], %o2
	cmp	%o2, %sp
	blt,pn	%xcc, 2f
	 movge	%xcc, %o2, %sp
	ldx	[%g1 + _JB_PC], %o7
	retl
	 mov	%g3, %o0
2:	PANIC("longjmp botch", %l1)
END(longjmp)

ENTRY(setjmp)
	stx	%sp, [%o0 + _JB_SP]
	stx	%o7, [%o0 + _JB_PC]
	stx	%fp, [%o0 + _JB_FP]
	retl
	 clr	%o0
END(setjmp)

/*
 * void openfirmware(cell_t args[])
 */
ENTRY(openfirmware)
	save	%sp, -CCFSZ, %sp
	SET(ofw_vec, %l7, %l6)
	ldx	[%l6], %l6
	rdpr	%pil, %l7
	wrpr	%g0, PIL_TICK, %pil
	call	%l6
	 mov	%i0, %o0
	wrpr	%l7, 0, %pil
	ret
	 restore %o0, %g0, %o0
END(openfirmware)

/*
 * void ofw_exit(cell_t args[])
 */
ENTRY(openfirmware_exit)
	save	%sp, -CCFSZ, %sp
	flushw
	wrpr	%g0, PIL_TICK, %pil
	SET(ofw_tba, %l7, %l5)
	ldx	[%l5], %l5
	wrpr	%l5, 0, %tba			! restore the ofw trap table
	SET(ofw_vec, %l7, %l6)
	ldx	[%l6], %l6
	SET(kstack0 + KSTACK_PAGES * PAGE_SIZE - PCB_SIZEOF, %l7, %l0)
	sub	%l0, SPOFF, %fp			! setup a stack in a locked page
	sub	%l0, SPOFF + CCFSZ, %sp
	mov     AA_DMMU_PCXR, %l3		! set context 0
	stxa    %g0, [%l3] ASI_DMMU
	membar	#Sync
	wrpr	%g0, 0, %tl			! force trap level 0
	call	%l6
	 mov	%i0, %o0
	! never to return
END(openfirmware_exit)

#ifdef GPROF

ENTRY(user)
	nop

ENTRY(btrap)
	nop

ENTRY(etrap)
	nop

ENTRY(bintr)
	nop

ENTRY(eintr)
	nop


/*
 * XXX including sys/gmon.h in genassym.c is not possible due to uintfptr_t
 * badness.
 */
#define	GM_STATE	0x0
#define	GMON_PROF_OFF	3
#define	GMON_PROF_HIRES	4

	.globl	_mcount
	.set	_mcount, __cyg_profile_func_enter

ENTRY(__cyg_profile_func_enter)
	SET(_gmonparam, %o3, %o2)
	lduw    [%o2 + GM_STATE], %o3
	cmp     %o3, GMON_PROF_OFF
	be,a,pn %icc, 1f
	 nop
	SET(mcount, %o3, %o2)
	jmpl	%o2, %g0
	 nop
1:      retl
	 nop
END(__cyg_profile_func_enter)

#ifdef GUPROF

ENTRY(__cyg_profile_func_exit)
	SET(_gmonparam, %o3, %o2)
	lduw    [%o2 + GM_STATE], %o3
	cmp     %o3, GMON_PROF_HIRES
	be,a,pn %icc, 1f
	 nop
	SET(mexitcount, %o3, %o2)
	jmpl	%o2, %g0
	 nop
1:      retl
	 nop
END(__cyg_profile_func_exit)

#endif /* GUPROF */

#endif /* GPROF */
