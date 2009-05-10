/*	$OpenBSD: locore.S,v 1.18 1998/09/15 10:58:53 pefo Exp $	*/
/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Digital Equipment Corporation and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (C) 1989 Digital Equipment Corporation.
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies.
 * Digital Equipment Corporation makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/loMem.s,
 *	v 1.1 89/07/11 17:55:04 nelson Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/machAsm.s,
 *	v 9.2 90/01/29 18:00:39 shirriff Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/vm/ds3100.md/vmPmaxAsm.s,
 *	v 1.1 89/07/10 14:27:41 nelson Exp  SPRITE (DECWRL)
 *
 *	from: @(#)locore.s	8.5 (Berkeley) 1/4/94
 *	JNPR: support.S,v 1.5.2.2 2007/08/29 10:03:49 girish
 * $FreeBSD$
 */

/*
 *	Contains code that is the first executed at boot time plus
 *	assembly language support routines.
 */

#include "opt_ddb.h"
#include <sys/errno.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>

#include "assym.s"

	.set	noreorder		# Noreorder is default style!

/*
 * Primitives
 */

/*
 * This table is indexed by u.u_pcb.pcb_onfault in trap().
 * The reason for using this table rather than storing an address in
 * u.u_pcb.pcb_onfault is simply to make the code faster.
 */
	.globl	onfault_table
	.data
	.align	3
onfault_table:
	.word	0			# invalid index number
#define	BADERR		1
	.word	baderr
#define	COPYERR		2
	.word	copyerr
#define	FSWBERR		3
	.word	fswberr
#define	FSWINTRBERR	4
	.word	fswintrberr
#if defined(DDB) || defined(DEBUG)
#define	DDBERR	5
	.word	ddberr
#else
	.word	0
#endif

	.text

/*
 * See if access to addr with a len type instruction causes a machine check.
 * len is length of access (1=byte, 2=short, 4=long)
 *
 * badaddr(addr, len)
 *	char *addr;
 *	int len;
 */
LEAF(badaddr)
	li	v0, BADERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	bne	a1, 1, 2f
	sw	v0, U_PCB_ONFAULT(v1)
	b	5f
	lbu	v0, (a0)
2:
	bne	a1, 2, 4f
	nop
	b	5f
	lhu	v0, (a0)
4:
	lw	v0, (a0)
5:
	sw	zero, U_PCB_ONFAULT(v1)
	j	ra
	move	v0, zero		# made it w/o errors
baderr:
	j	ra
	li	v0, 1			# trap sends us here
END(badaddr)

/*
 * int copystr(void *kfaddr, void *kdaddr, size_t maxlen, size_t *lencopied)
 * Copy a NIL-terminated string, at most maxlen characters long.  Return the
 * number of characters copied (including the NIL) in *lencopied.  If the
 * string is too long, return ENAMETOOLONG; else return 0.
 */
LEAF(copystr)
	move	t0, a2
	beq	a2, zero, 4f
1:
	lbu	v0, 0(a0)
	subu	a2, a2, 1
	beq	v0, zero, 2f
	sb	v0, 0(a1)		# each byte until NIL
	addu	a0, a0, 1
	bne	a2, zero, 1b		# less than maxlen
	addu	a1, a1, 1
4:
	li	v0, ENAMETOOLONG	# run out of space
2:
	beq	a3, zero, 3f		# return num. of copied bytes
	subu	a2, t0, a2		# if the 4th arg was non-NULL
	sw	a2, 0(a3)
3:
	j	ra			# v0 is 0 or ENAMETOOLONG
	nop
END(copystr)


/*
 * fillw(pat, addr, count)
 */
LEAF(fillw)
1:
	addiu	a2, a2, -1
	sh	a0, 0(a1)
	bne	a2,zero, 1b
	addiu	a1, a1, 2

	jr	ra
	nop
END(fillw)

/*
 * Optimized memory zero code.
 * mem_zero_page(addr);
 */
LEAF(mem_zero_page)
	li	v0, NBPG
1:
	subu	v0, 8
	sd	zero, 0(a0)
	bne	zero, v0, 1b
	addu	a0, 8
	jr	ra
	nop
END(mem_zero_page)

/*
 *	Block I/O routines mainly used by I/O drivers.
 *
 *	Args as:	a0 = port
 *			a1 = memory address
 *			a2 = count
 */
LEAF(insb)
	beq	a2, zero, 2f
	addu	a2, a1
1:
	lbu	v0, 0(a0)
	addiu	a1, 1
	bne	a1, a2, 1b
	sb	v0, -1(a1)
2:
	jr	ra
	nop
END(insb)

LEAF(insw)
	beq	a2, zero, 2f
	addu	a2, a2
	addu	a2, a1
1:
	lhu	v0, 0(a0)
	addiu	a1, 2
	bne	a1, a2, 1b
	sh	v0, -2(a1)
2:
	jr	ra
	nop
END(insw)

LEAF(insl)
	beq	a2, zero, 2f
	sll	a2, 2
	addu	a2, a1
1:
	lw	v0, 0(a0)
	addiu	a1, 4
	bne	a1, a2, 1b
	sw	v0, -4(a1)
2:
	jr	ra
	nop
END(insl)

LEAF(outsb)
	beq	a2, zero, 2f
	addu	a2, a1
1:
	lbu	v0, 0(a1)
	addiu	a1, 1
	bne	a1, a2, 1b
	sb	v0, 0(a0)
2:
	jr	ra
	nop
END(outsb)

LEAF(outsw)
	beq	a2, zero, 2f
	addu	a2, a2
	li	v0, 1
	and	v0, a1
	bne	v0, zero, 3f		# arghh, unaligned.
	addu	a2, a1
1:
	lhu	v0, 0(a1)
	addiu	a1, 2
	bne	a1, a2, 1b
	sh	v0, 0(a0)
2:
	jr	ra
	nop
3:
	LWHI	v0, 0(a1)
	LWLO	v0, 3(a1)
	addiu	a1, 2
	bne	a1, a2, 3b
	sh	v0, 0(a0)

	jr	ra
	nop
END(outsw)

LEAF(outsl)
	beq	a2, zero, 2f
	sll	a2, 2
	li	v0, 3
	and	v0, a1
	bne	v0, zero, 3f		# arghh, unaligned.
	addu	a2, a1
1:
	lw	v0, 0(a1)
	addiu	a1, 4
	bne	a1, a2, 1b
	sw	v0, 0(a0)
2:
	jr	ra
	nop
3:
	LWHI	v0, 0(a1)
	LWLO	v0, 3(a1)
	addiu	a1, 4
	bne	a1, a2, 3b
	sw	v0, 0(a0)

	jr	ra
	nop
END(outsl)

/*
 * Copy a null terminated string from the user address space into
 * the kernel address space.
 *
 *	copyinstr(fromaddr, toaddr, maxlength, &lencopied)
 *		caddr_t fromaddr;
 *		caddr_t toaddr;
 *		u_int maxlength;
 *		u_int *lencopied;
 */
NON_LEAF(copyinstr, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	sw	ra, STAND_RA_OFFSET(sp)
	blt	a0, zero, _C_LABEL(copyerr)  # make sure address is in user space
	li	v0, COPYERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	jal	_C_LABEL(copystr)
	sw	v0, U_PCB_ONFAULT(v1)
	lw	ra, STAND_RA_OFFSET(sp)
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	zero, U_PCB_ONFAULT(v1)
	j	ra
	addu	sp, sp, STAND_FRAME_SIZE
END(copyinstr)

/*
 * Copy a null terminated string from the kernel address space into
 * the user address space.
 *
 *	copyoutstr(fromaddr, toaddr, maxlength, &lencopied)
 *		caddr_t fromaddr;
 *		caddr_t toaddr;
 *		u_int maxlength;
 *		u_int *lencopied;
 */
NON_LEAF(copyoutstr, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	sw	ra, STAND_RA_OFFSET(sp)
	blt	a1, zero, _C_LABEL(copyerr)  # make sure address is in user space
	li	v0, COPYERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	jal	_C_LABEL(copystr)
	sw	v0, U_PCB_ONFAULT(v1)
	lw	ra, STAND_RA_OFFSET(sp)
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	zero, U_PCB_ONFAULT(v1)
	j	ra
	addu	sp, sp, STAND_FRAME_SIZE
END(copyoutstr)

/*
 * Copy specified amount of data from user space into the kernel
 *	copyin(from, to, len)
 *		caddr_t *from;	(user source address)
 *		caddr_t *to;	(kernel destination address)
 *		unsigned len;
 */
NON_LEAF(copyin, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	sw	ra, STAND_RA_OFFSET(sp)
	blt	a0, zero, _C_LABEL(copyerr)  # make sure address is in user space
	li	v0, COPYERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	jal	_C_LABEL(bcopy)
	sw	v0, U_PCB_ONFAULT(v1)
	lw	ra, STAND_RA_OFFSET(sp)
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)	 	# bcopy modified v1, so reload
	sw	zero, U_PCB_ONFAULT(v1)
	addu	sp, sp, STAND_FRAME_SIZE
	j	ra
	move	v0, zero
END(copyin)

/*
 * Copy specified amount of data from kernel to the user space
 *	copyout(from, to, len)
 *		caddr_t *from;	(kernel source address)
 *		caddr_t *to;	(user destination address)
 *		unsigned len;
 */
NON_LEAF(copyout, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	sw	ra, STAND_RA_OFFSET(sp)
	blt	a1, zero, _C_LABEL(copyerr) # make sure address is in user space
	li	v0, COPYERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	jal	_C_LABEL(bcopy)
	sw	v0, U_PCB_ONFAULT(v1)
	lw	ra, STAND_RA_OFFSET(sp)
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)		# bcopy modified v1, so reload
	sw	zero, U_PCB_ONFAULT(v1)
	addu	sp, sp, STAND_FRAME_SIZE
	j	ra
	move	v0, zero
END(copyout)

LEAF(copyerr)
	lw	ra, STAND_RA_OFFSET(sp)
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	zero, U_PCB_ONFAULT(v1)
	addu	sp, sp, STAND_FRAME_SIZE
	j	ra
	li	v0, EFAULT			# return error
END(copyerr)

/*
 * {fu,su},{ibyte,isword,iword}, fetch or store a byte, short or word to
 * user text space.
 * {fu,su},{byte,sword,word}, fetch or store a byte, short or word to
 * user data space.
 */
LEAF(fuword)
ALEAF(fuword32)
ALEAF(fuiword)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	v0, U_PCB_ONFAULT(v1)
	lw	v0, 0(a0)		# fetch word
	j	ra
	sw	zero, U_PCB_ONFAULT(v1)
END(fuword)

LEAF(fusword)
ALEAF(fuisword)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	v0, U_PCB_ONFAULT(v1)
	lhu	v0, 0(a0)		# fetch short
	j	ra
	sw	zero, U_PCB_ONFAULT(v1)
END(fusword)

LEAF(fubyte)
ALEAF(fuibyte)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	v0, U_PCB_ONFAULT(v1)
	lbu	v0, 0(a0)		# fetch byte
	j	ra
	sw	zero, U_PCB_ONFAULT(v1)
END(fubyte)

LEAF(suword)
XLEAF(suword32)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	v0, U_PCB_ONFAULT(v1)
	sw	a1, 0(a0)		# store word
	sw	zero, U_PCB_ONFAULT(v1)
	j	ra
	move	v0, zero
END(suword)

/*
 * casuword(9)
 * <v0>u_long casuword(<a0>u_long *p, <a1>u_long oldval, <a2>u_long newval)
 */
ENTRY(casuword)
	break
	li	v0, -1
	jr	ra
	nop
END(casuword)

/*
 * casuword32(9)
 * <v0>uint32_t casuword(<a0>uint32_t *p, <a1>uint32_t oldval, 
 *							<a2>uint32_t newval)
 */
ENTRY(casuword32)
	break
	li	v0, -1
	jr	ra
	nop
END(casuword32)

#if 0
	/* unused in FreeBSD */
/*
 * Have to flush instruction cache afterwards.
 */
LEAF(suiword)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	v0, U_PCB_ONFAULT(v1)
	sw	a1, 0(a0)		# store word
	sw	zero, U_PCB_ONFAULT(v1)
	j	_C_LABEL(Mips_SyncICache)  # FlushICache sets v0 = 0. (Ugly)
	li	a1, 4			# size of word
END(suiword)
#endif

/*
 * Will have to flush the instruction cache if byte merging is done in hardware.
 */
LEAF(susword)
ALEAF(suisword)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	v0, U_PCB_ONFAULT(v1)
	sh	a1, 0(a0)		# store short
	sw	zero, U_PCB_ONFAULT(v1)
	j	ra
	move	v0, zero
END(susword)

LEAF(subyte)
ALEAF(suibyte)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	v0, U_PCB_ONFAULT(v1)
	sb	a1, 0(a0)		# store byte
	sw	zero, U_PCB_ONFAULT(v1)
	j	ra
	move	v0, zero
END(subyte)

LEAF(fswberr)
	j	ra
	li	v0, -1
END(fswberr)

/*
 * fuswintr and suswintr are just like fusword and susword except that if
 * the page is not in memory or would cause a trap, then we return an error.
 * The important thing is to prevent sleep() and switch().
 */
LEAF(fuswintr)
	blt	a0, zero, fswintrberr	# make sure address is in user space
	li	v0, FSWINTRBERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	v0, U_PCB_ONFAULT(v1)
	lhu	v0, 0(a0)		# fetch short
	j	ra
	sw	zero, U_PCB_ONFAULT(v1)
END(fuswintr)

LEAF(suswintr)
	blt	a0, zero, fswintrberr	# make sure address is in user space
	li	v0, FSWINTRBERR
	GET_CPU_PCPU(v1)
	lw	v1, PC_CURPCB(v1)
	sw	v0, U_PCB_ONFAULT(v1)
	sh	a1, 0(a0)		# store short
	sw	zero, U_PCB_ONFAULT(v1)
	j	ra
	move	v0, zero
END(suswintr)

LEAF(fswintrberr)
	j	ra
	li	v0, -1
END(fswintrberr)

/*
 * Insert 'p' after 'q'.
 *	_insque(p, q)
 *		caddr_t p, q;
 */
LEAF(_insque)
	lw	v0, 0(a1)		# v0 = q->next
	sw	a1, 4(a0)		# p->prev = q
	sw	v0, 0(a0)		# p->next = q->next
	sw	a0, 4(v0)		# q->next->prev = p
	j	ra
	sw	a0, 0(a1)		# q->next = p
END(_insque)

/*
 * Remove item 'p' from queue.
 *	_remque(p)
 *		caddr_t p;
 */
LEAF(_remque)
	lw	v0, 0(a0)		# v0 = p->next
	lw	v1, 4(a0)		# v1 = p->prev
	nop
	sw	v0, 0(v1)		# p->prev->next = p->next
	j	ra
	sw	v1, 4(v0)		# p->next->prev = p->prev
END(_remque)

/*--------------------------------------------------------------------------
 *
 * Mips_GetCOUNT --
 *
 *	Mips_GetCOUNT()
 *
 * Results:
 *	Returns the current COUNT reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(Mips_GetCOUNT)
	mfc0	v0, COP_0_COUNT
	nop	#???
	nop	#???
	j	ra
	nop
END(Mips_GetCOUNT)

/*--------------------------------------------------------------------------
 *
 * Mips_SetCOMPARE --
 *
 *	Mips_SetCOMPARE()
 *
 * Results:
 *	Sets a new value to the COMPARE register.
 *
 * Side effects:
 *	The COMPARE equal interrupt is acknowledged.
 *
 *--------------------------------------------------------------------------
 */
LEAF(Mips_SetCOMPARE)
	mtc0	a0, COP_0_COMPARE
	j	ra
	nop
END(Mips_SetCOMPARE)

LEAF(Mips_GetCOMPARE)
	mfc0	v0, COP_0_COMPARE
	j	ra
	nop
END(Mips_GetCOMPARE)

/*
 * u_int32_t mips_cp0_status_read(void)
 *
 *	Return the current value of the CP0 Status register.
 */
LEAF(mips_cp0_status_read)
	mfc0	v0, COP_0_STATUS_REG
	j	ra
	nop
END(mips_cp0_status_read)

/*
 * void mips_cp0_status_write(u_int32_t)
 *
 *	Set the value of the CP0 Status register.
 *
 *	Note: This is almost certainly not the way you want to write a
 *	"permanent" value to to the CP0 Status register, since it gets
 *	saved in trap frames and restores.
 */
LEAF(mips_cp0_status_write)
	mtc0	a0, COP_0_STATUS_REG
	nop
	nop
	j	ra
	nop
END(mips_cp0_status_write)


/*
 * memcpy(to, from, len)
 * {ov}bcopy(from, to, len)
 */
LEAF(memcpy)
	.set	noreorder
	move	v0, a0			# swap from and to
	move	a0, a1
	move	a1, v0
ALEAF(bcopy)
ALEAF(ovbcopy)
	.set	noreorder
	addu	t0, a0, a2		# t0 = end of s1 region
	sltu	t1, a1, t0
	sltu	t2, a0, a1
	and	t1, t1, t2		# t1 = true if from < to < (from+len)
	beq	t1, zero, forward	# non overlapping, do forward copy
	slt	t2, a2, 12		# check for small copy

	ble	a2, zero, 2f
	addu	t1, a1, a2		# t1 = end of to region
1:
	lb	v1, -1(t0)		# copy bytes backwards,
	subu	t0, t0, 1		#   doesnt happen often so do slow way
	subu	t1, t1, 1
	bne	t0, a0, 1b
	sb	v1, 0(t1)
2:
	j	ra
	nop
forward:
	bne	t2, zero, smallcpy	# do a small bcopy
	xor	v1, a0, a1		# compare low two bits of addresses
	and	v1, v1, 3
	subu	a3, zero, a1		# compute # bytes to word align address
	beq	v1, zero, aligned	# addresses can be word aligned
	and	a3, a3, 3

	beq	a3, zero, 1f
	subu	a2, a2, a3		# subtract from remaining count
	LWHI	v1, 0(a0)		# get next 4 bytes (unaligned)
	LWLO	v1, 3(a0)
	addu	a0, a0, a3
	SWHI	v1, 0(a1)		# store 1, 2, or 3 bytes to align a1
	addu	a1, a1, a3
1:
	and	v1, a2, 3		# compute number of words left
	subu	a3, a2, v1
	move	a2, v1
	addu	a3, a3, a0		# compute ending address
2:
	LWHI	v1, 0(a0)		# copy words a0 unaligned, a1 aligned
	LWLO	v1, 3(a0)
	addu	a0, a0, 4
	sw	v1, 0(a1)
	addu	a1, a1, 4
	bne	a0, a3, 2b
	nop				# We have to do this mmu-bug.
	b	smallcpy
	nop
aligned:
	beq	a3, zero, 1f
	subu	a2, a2, a3		# subtract from remaining count
	LWHI	v1, 0(a0)		# copy 1, 2, or 3 bytes to align
	addu	a0, a0, a3
	SWHI	v1, 0(a1)
	addu	a1, a1, a3
1:
	and	v1, a2, 3		# compute number of whole words left
	subu	a3, a2, v1
	move	a2, v1
	addu	a3, a3, a0		# compute ending address
2:
	lw	v1, 0(a0)		# copy words
	addu	a0, a0, 4
	sw	v1, 0(a1)
	bne	a0, a3, 2b
	addu	a1, a1, 4
smallcpy:
	ble	a2, zero, 2f
	addu	a3, a2, a0		# compute ending address
1:
	lbu	v1, 0(a0)		# copy bytes
	addu	a0, a0, 1
	sb	v1, 0(a1)
	bne	a0, a3, 1b
	addu	a1, a1, 1	   # MMU BUG ? can not do -1(a1) at 0x80000000!!
2:
	j	ra
	nop
END(memcpy)

/*
 * memset(void *s1, int c, int len)
 * NetBSD: memset.S,v 1.3 2001/10/16 15:40:53 uch Exp
 */
LEAF(memset)
	.set noreorder
	blt	a2, 12, memsetsmallclr	# small amount to clear?
	move	v0, a0			# save s1 for result

	sll	t1, a1, 8		# compute  c << 8 in t1
	or	t1, t1, a1		# compute c << 8 | c in 11
	sll	t2, t1, 16		# shift that left 16
	or	t1, t2, t1		# or together

	subu	t0, zero, a0		# compute # bytes to word align address
	and	t0, t0, 3
	beq	t0, zero, 1f		# skip if word aligned
	subu	a2, a2, t0		# subtract from remaining count
	SWHI	t1, 0(a0)		# store 1, 2, or 3 bytes to align
	addu	a0, a0, t0
1:
	and	v1, a2, 3		# compute number of whole words left
	subu	t0, a2, v1
	subu	a2, a2, t0
	addu	t0, t0, a0		# compute ending address
2:
	addu	a0, a0, 4		# clear words
#ifdef MIPS3_5900
	nop
	nop
	nop
	nop
#endif
	bne	a0, t0, 2b		#  unrolling loop does not help
	sw	t1, -4(a0)		#  since we are limited by memory speed

memsetsmallclr:
	ble	a2, zero, 2f
	addu	t0, a2, a0		# compute ending address
1:
	addu	a0, a0, 1		# clear bytes
#ifdef MIPS3_5900
	nop
	nop
	nop
	nop
#endif
	bne	a0, t0, 1b
	sb	a1, -1(a0)
2:
	j	ra
	nop
	.set reorder
END(memset)

/*
 * bzero(s1, n)
 */
LEAF(bzero)
ALEAF(blkclr)
	.set	noreorder
	blt	a1, 12, smallclr	# small amount to clear?
	subu	a3, zero, a0		# compute # bytes to word align address
	and	a3, a3, 3
	beq	a3, zero, 1f		# skip if word aligned
	subu	a1, a1, a3		# subtract from remaining count
	SWHI	zero, 0(a0)		# clear 1, 2, or 3 bytes to align
	addu	a0, a0, a3
1:
	and	v0, a1, 3		# compute number of words left
	subu	a3, a1, v0
	move	a1, v0
	addu	a3, a3, a0		# compute ending address
2:
	addu	a0, a0, 4		# clear words
	bne	a0, a3, 2b		#  unrolling loop does not help
	sw	zero, -4(a0)		#  since we are limited by memory speed
smallclr:
	ble	a1, zero, 2f
	addu	a3, a1, a0		# compute ending address
1:
	addu	a0, a0, 1		# clear bytes
	bne	a0, a3, 1b
	sb	zero, -1(a0)
2:
	j	ra
	nop
END(bzero)


/*
 * bcmp(s1, s2, n)
 */
LEAF(bcmp)
	.set	noreorder
	blt	a2, 16, smallcmp	# is it worth any trouble?
	xor	v0, a0, a1		# compare low two bits of addresses
	and	v0, v0, 3
	subu	a3, zero, a1		# compute # bytes to word align address
	bne	v0, zero, unalignedcmp	# not possible to align addresses
	and	a3, a3, 3

	beq	a3, zero, 1f
	subu	a2, a2, a3		# subtract from remaining count
	move	v0, v1			# init v0,v1 so unmodified bytes match
	LWHI	v0, 0(a0)		# read 1, 2, or 3 bytes
	LWHI	v1, 0(a1)
	addu	a1, a1, a3
	bne	v0, v1, nomatch
	addu	a0, a0, a3
1:
	and	a3, a2, ~3		# compute number of whole words left
	subu	a2, a2, a3		#   which has to be >= (16-3) & ~3
	addu	a3, a3, a0		# compute ending address
2:
	lw	v0, 0(a0)		# compare words
	lw	v1, 0(a1)
	addu	a0, a0, 4
	bne	v0, v1, nomatch
	addu	a1, a1, 4
	bne	a0, a3, 2b
	nop
	b	smallcmp		# finish remainder
	nop
unalignedcmp:
	beq	a3, zero, 2f
	subu	a2, a2, a3		# subtract from remaining count
	addu	a3, a3, a0		# compute ending address
1:
	lbu	v0, 0(a0)		# compare bytes until a1 word aligned
	lbu	v1, 0(a1)
	addu	a0, a0, 1
	bne	v0, v1, nomatch
	addu	a1, a1, 1
	bne	a0, a3, 1b
	nop
2:
	and	a3, a2, ~3		# compute number of whole words left
	subu	a2, a2, a3		#   which has to be >= (16-3) & ~3
	addu	a3, a3, a0		# compute ending address
3:
	LWHI	v0, 0(a0)		# compare words a0 unaligned, a1 aligned
	LWLO	v0, 3(a0)
	lw	v1, 0(a1)
	addu	a0, a0, 4
	bne	v0, v1, nomatch
	addu	a1, a1, 4
	bne	a0, a3, 3b
	nop
smallcmp:
	ble	a2, zero, match
	addu	a3, a2, a0		# compute ending address
1:
	lbu	v0, 0(a0)
	lbu	v1, 0(a1)
	addu	a0, a0, 1
	bne	v0, v1, nomatch
	addu	a1, a1, 1
	bne	a0, a3, 1b
	nop
match:
	j	ra
	 move	v0, zero
nomatch:
	j	ra
	li	v0, 1
END(bcmp)


/*
 * bit = ffs(value)
 */
LEAF(ffs)
	.set	noreorder
	beq	a0, zero, 2f
	move	v0, zero
1:
	and	v1, a0, 1		# bit set?
	addu	v0, v0, 1
	beq	v1, zero, 1b		# no, continue
	srl	a0, a0, 1
2:
	j	ra
	nop
END(ffs)

LEAF(get_current_fp)
	j	ra
	move	v0, s8
END(get_current_fp)

LEAF(loadandclear)
	.set	noreorder
1:
	ll	v0, 0(a0)
	move	t0, zero
	sc	t0, 0(a0)
	beq	t0, zero, 1b
	nop
	j	ra
	nop
END(loadandclear)

#if 0
/*
 * u_int32_t atomic_cmpset_32(u_int32_t *p, u_int32_t cmpval, u_int32_t newval)
 * Atomically compare the value stored at p with cmpval
 * and if the two values are equal, update value *p with
 * newval. Return zero if compare failed, non-zero otherwise
 *
 */

LEAF(atomic_cmpset_32)
	.set	noreorder
1:
	ll	t0, 0(a0)
	move	v0, zero
	bne	t0, a1, 2f
	move	t1, a2
	sc	t1, 0(a0)
	beq	t1, zero, 1b
	or	v0, v0, 1
2:
	j	ra
	nop
END(atomic_cmpset_32)

/**
 * u_int32_t
 * atomic_readandclear_32(u_int32_t *a)
 * {
 *	u_int32_t retval;
 *	retval = *a;
 *	*a = 0;
 * }
 */
LEAF(atomic_readandclear_32)
	.set	noreorder
1:
	ll	t0, 0(a0)
	move	t1, zero
	move	v0, t0
	sc	t1, 0(a0)
	beq	t1, zero, 1b
	nop
	j	ra
	nop
END(atomic_readandclear_32)

/**
 * void
 * atomic_set_32(u_int32_t *a, u_int32_t b)
 * {
 *	*a |= b;
 * }
 */
LEAF(atomic_set_32)
	.set	noreorder
1:
	ll	t0, 0(a0)
	or	t0, t0, a1
	sc	t0, 0(a0)
	beq	t0, zero, 1b
	nop
	j	ra
	nop
END(atomic_set_32)

/**
 * void
 * atomic_add_32(uint32_t *a, uint32_t b)
 * {
 *	*a += b;
 * }
 */
LEAF(atomic_add_32)
	.set	noreorder
	srl	a0, a0, 2	# round down address to be 32-bit aligned
	sll	a0, a0, 2
1:
	ll	t0, 0(a0)
	addu	t0, t0, a1
	sc	t0, 0(a0)
	beq	t0, zero, 1b
	nop
	j	ra
	nop
END(atomic_add_32)

/**
 * void
 * atomic_clear_32(u_int32_t *a, u_int32_t b)
 * {
 *	*a &= ~b;
 * }
 */
LEAF(atomic_clear_32)
	.set	noreorder
	srl	a0, a0, 2	# round down address to be 32-bit aligned
	sll	a0, a0, 2
	nor	a1, zero, a1
1:
	ll	t0, 0(a0)
	and	t0, t0, a1	# t1 has the new lower 16 bits
	sc	t0, 0(a0)
	beq	t0, zero, 1b
	nop
	j	ra
	nop
END(atomic_clear_32)

/**
 * void
 * atomic_subtract_32(uint16_t *a, uint16_t b)
 * {
 *	*a -= b;
 * }
 */
LEAF(atomic_subtract_32)
	.set	noreorder
	srl	a0, a0, 2	# round down address to be 32-bit aligned
	sll	a0, a0, 2
1:
	ll	t0, 0(a0)
	subu	t0, t0, a1
	sc	t0, 0(a0)
	beq	t0, zero, 1b
	nop
	j	ra
	nop
END(atomic_subtract_32)

#endif

/**
 * void
 * atomic_set_16(u_int16_t *a, u_int16_t b)
 * {
 *	*a |= b;
 * }
 */
LEAF(atomic_set_16)
	.set	noreorder
	srl	a0, a0, 2	# round down address to be 32-bit aligned
	sll	a0, a0, 2
	andi	a1, a1, 0xffff
1:
	ll	t0, 0(a0)
	or	t0, t0, a1
	sc	t0, 0(a0)
	beq	t0, zero, 1b
	nop
	j	ra
	nop
END(atomic_set_16)

/**
 * void
 * atomic_clear_16(u_int16_t *a, u_int16_t b)
 * {
 *	*a &= ~b;
 * }
 */
LEAF(atomic_clear_16)
	.set	noreorder
	srl	a0, a0, 2	# round down address to be 32-bit aligned
	sll	a0, a0, 2
	nor	a1, zero, a1
1:
	ll	t0, 0(a0)
	move	t1, t0
	andi	t1, t1, 0xffff	# t1 has the original lower 16 bits
	and	t1, t1, a1	# t1 has the new lower 16 bits
	srl	t0, t0, 16	# preserve original top 16 bits
	sll	t0, t0, 16
	or	t0, t0, t1
	sc	t0, 0(a0)
	beq	t0, zero, 1b
	nop
	j	ra
	nop
END(atomic_clear_16)


/**
 * void
 * atomic_subtract_16(uint16_t *a, uint16_t b)
 * {
 *	*a -= b;
 * }
 */
LEAF(atomic_subtract_16)
	.set	noreorder
	srl	a0, a0, 2	# round down address to be 32-bit aligned
	sll	a0, a0, 2
1:
	ll	t0, 0(a0)
	move	t1, t0
	andi	t1, t1, 0xffff	# t1 has the original lower 16 bits
	subu	t1, t1, a1
	andi	t1, t1, 0xffff	# t1 has the new lower 16 bits
	srl	t0, t0, 16	# preserve original top 16 bits
	sll	t0, t0, 16
	or	t0, t0, t1
	sc	t0, 0(a0)
	beq	t0, zero, 1b
	nop
	j	ra
	nop
END(atomic_subtract_16)

/**
 * void
 * atomic_add_16(uint16_t *a, uint16_t b)
 * {
 *	*a += b;
 * }
 */
LEAF(atomic_add_16)
	.set	noreorder
	srl	a0, a0, 2	# round down address to be 32-bit aligned
	sll	a0, a0, 2
1:
	ll	t0, 0(a0)
	move	t1, t0
	andi	t1, t1, 0xffff	# t1 has the original lower 16 bits
	addu	t1, t1, a1
	andi	t1, t1, 0xffff	# t1 has the new lower 16 bits
	srl	t0, t0, 16	# preserve original top 16 bits
	sll	t0, t0, 16
	or	t0, t0, t1
	sc	t0, 0(a0)
	beq	t0, zero, 1b
	nop
	j	ra
	nop
END(atomic_add_16)

/**
 * void
 * atomic_add_8(uint8_t *a, uint8_t b)
 * {
 *	*a += b;
 * }
 */
LEAF(atomic_add_8)
	.set	noreorder
	srl	a0, a0, 2	# round down address to be 32-bit aligned
	sll	a0, a0, 2
1:
	ll	t0, 0(a0)
	move	t1, t0
	andi	t1, t1, 0xff	# t1 has the original lower 8 bits
	addu	t1, t1, a1
	andi	t1, t1, 0xff	# t1 has the new lower 8 bits
	srl	t0, t0, 8	# preserve original top 24 bits
	sll	t0, t0, 8
	or	t0, t0, t1
	sc	t0, 0(a0)
	beq	t0, zero, 1b
	nop
	j	ra
	nop
END(atomic_add_8)


/**
 * void
 * atomic_subtract_8(uint8_t *a, uint8_t b)
 * {
 *	*a += b;
 * }
 */
LEAF(atomic_subtract_8)
	.set	noreorder
	srl	a0, a0, 2	# round down address to be 32-bit aligned
	sll	a0, a0, 2
1:
	ll	t0, 0(a0)
	move	t1, t0
	andi	t1, t1, 0xff	# t1 has the original lower 8 bits
	subu	t1, t1, a1
	andi	t1, t1, 0xff	# t1 has the new lower 8 bits
	srl	t0, t0, 8	# preserve original top 24 bits
	sll	t0, t0, 8
	or	t0, t0, t1
	sc	t0, 0(a0)
	beq	t0, zero, 1b
	nop
	j	ra
	nop
END(atomic_subtract_8)

/*
 *	atomic 64-bit register read/write assembly language support routines.
 */

	.set	noreorder		# Noreorder is default style!
#ifndef _MIPS_ARCH_XLR
	.set	mips3
#endif

LEAF(atomic_readandclear_64)
1:
	lld	v0, 0(a0)
	li	t0, 0
	scd	t0, 0(a0)
	beqz	t0, 1b
	nop
	j	ra
	nop
END(atomic_readandclear_64)

LEAF(atomic_store_64)
	mfc0	t1, COP_0_STATUS_REG
	and	t2, t1, ~SR_INT_ENAB
	mtc0	t2, COP_0_STATUS_REG
	nop
	nop
	nop
	nop
	ld	t0, (a1)
	nop
	nop
	sd	t0, (a0)
	nop
	nop
	mtc0	t1,COP_0_STATUS_REG
	nop
	nop
	nop
	nop
	j	ra
	nop
END(atomic_store_64)

LEAF(atomic_load_64)
	mfc0	t1, COP_0_STATUS_REG
	and	t2, t1, ~SR_INT_ENAB
	mtc0	t2, COP_0_STATUS_REG
	nop
	nop
	nop
	nop
	ld	t0, (a0)
	nop
	nop
	sd	t0, (a1)
	nop
	nop
	mtc0	t1,COP_0_STATUS_REG
	nop
	nop
	nop
	nop
	j	ra
	nop
END(atomic_load_64)

#if defined(DDB) || defined(DEBUG)

LEAF(kdbpeek)
	li	v1, DDBERR
	and	v0, a0, 3			# unaligned ?
	GET_CPU_PCPU(t1)
	lw	t1, PC_CURPCB(t1)
	bne	v0, zero, 1f
	sw	v1, U_PCB_ONFAULT(t1)

	lw	v0, (a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t1)

1:
	LWHI	v0, 0(a0)
	LWLO	v0, 3(a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t1)
END(kdbpeek)

ddberr:
	jr	ra
	nop

#if defined(DDB)
LEAF(kdbpoke)
	li	v1, DDBERR
	and	v0, a0, 3			# unaligned ?
	GET_CPU_PCPU(t1)
	lw	t1, PC_CURPCB(t1)
	bne	v0, zero, 1f
	sw	v1, U_PCB_ONFAULT(t1)

	sw	a1, (a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t1)

1:
	SWHI	a1, 0(a0)
	SWLO	a1, 3(a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t1)
END(kdbpoke)

	.data
	.globl	esym
esym:	.word	0

#ifndef _MIPS_ARCH_XLR
	.set	mips2
#endif
#endif /* DDB */
#endif /* DDB || DEBUG */

#ifndef MIPS_ISAIII
#define	STORE	sw	/* 32 bit mode regsave instruction */
#define	LOAD	lw	/* 32 bit mode regload instruction */
#define	RSIZE	4	/* 32 bit mode register size */
#else
#define	STORE	sd	/* 64 bit mode regsave instruction */
#define	LOAD	ld	/* 64 bit mode regload instruction */
#define	RSIZE	8	/* 64 bit mode register size */
#endif

#define	ITLBNOPFIX	nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;

	.text
LEAF(breakpoint)
	break	BREAK_SOVER_VAL
	jr	ra
	nop
	END(breakpoint)

LEAF(setjmp)
	mfc0	v0, COP_0_STATUS_REG	# Later the "real" spl value!
	STORE	s0, (RSIZE * PREG_S0)(a0)
	STORE	s1, (RSIZE * PREG_S1)(a0)
	STORE	s2, (RSIZE * PREG_S2)(a0)
	STORE	s3, (RSIZE * PREG_S3)(a0)
	STORE	s4, (RSIZE * PREG_S4)(a0)
	STORE	s5, (RSIZE * PREG_S5)(a0)
	STORE	s6, (RSIZE * PREG_S6)(a0)
	STORE	s7, (RSIZE * PREG_S7)(a0)
	STORE	s8, (RSIZE * PREG_SP)(a0)
	STORE	sp, (RSIZE * PREG_S8)(a0)
	STORE	ra, (RSIZE * PREG_RA)(a0)
	STORE	v0, (RSIZE * PREG_SR)(a0)
	jr	ra
	li	v0, 0			# setjmp return
END(setjmp)

LEAF(longjmp)
	LOAD	v0, (RSIZE * PREG_SR)(a0)
	LOAD	ra, (RSIZE * PREG_RA)(a0)
	LOAD	s0, (RSIZE * PREG_S0)(a0)
	LOAD	s1, (RSIZE * PREG_S1)(a0)
	LOAD	s2, (RSIZE * PREG_S2)(a0)
	LOAD	s3, (RSIZE * PREG_S3)(a0)
	LOAD	s4, (RSIZE * PREG_S4)(a0)
	LOAD	s5, (RSIZE * PREG_S5)(a0)
	LOAD	s6, (RSIZE * PREG_S6)(a0)
	LOAD	s7, (RSIZE * PREG_S7)(a0)
	LOAD	s8, (RSIZE * PREG_S8)(a0)
	LOAD	sp, (RSIZE * PREG_SP)(a0)
	mtc0	v0, COP_0_STATUS_REG	# Later the "real" spl value!
	ITLBNOPFIX
	jr	ra
	li	v0, 1			# longjmp return
END(longjmp)

LEAF(fusufault)
	GET_CPU_PCPU(t0)
	lw	t0, PC_CURTHREAD(t0)
	lw	t0, TD_PCB(t0)
	sw	zero, U_PCB_ONFAULT(t0)
	li	v0, -1
	j	ra
END(fusufault)

    /* Define a new md function 'casuptr'.  This atomically compares and sets
       a pointer that is in user space.	 It will be used as the basic primitive
       for a kernel supported user space lock implementation. */
LEAF(casuptr)

	li	t0, VM_MAXUSER_ADDRESS /* verify address validity */
	blt	a0, t0, fusufault		/* trap faults */
	nop

	GET_CPU_PCPU(t1)
	lw	t1, PC_CURTHREAD(t1)
	lw	t1, TD_PCB(t1)

	lw	t2, fusufault
	sw	t2, U_PCB_ONFAULT(t1)
1:
	ll	v0, 0(a0)		/* try to load the old value */
	beq	v0, a1, 2f		/* compare */
	move	t0, a2			/* setup value to write */
	sc	t0, 0(a0)		/* write if address still locked */
	beq	t0, zero, 1b			/* if it failed, spin */
2:
	sw	zero, U_PCB_ONFAULT(t1) /* clean up */
	j	ra
END(casuptr)


#ifdef TARGET_OCTEON
/* 
 * void octeon_enable_shadow(void)
 *	turns on access to CC and CCRes
 */	
LEAF(octeon_enable_shadow)
	li      t1, 0x0000000f
	mtc0	t1, COP_0_INFO
	jr	ra
	nop
END(octeon_enable_shadow)

	
LEAF(octeon_get_shadow)
	mfc0	v0, COP_0_INFO
	jr	ra
	nop
END(octeon_get_shadow)

/*
 * octeon_set_control(addr, uint32_t val)
 */
LEAF(octeon_set_control)
	.set mips64r2
	or      t1, a1, zero
/*	dmfc0   a1, 9, 7*/
	.word 0x40254807
	sd	a1, 0(a0)
	or      a1, t1, zero
/*	dmtc0   a1, 9, 7*/
	.word 0x40a54807
	jr	ra
	nop
        .set	mips0
END(octeon_set_control)

/*
 * octeon_get_control(addr)
 */
LEAF(octeon_get_control)
	.set mips64r2
/*	dmfc0   a1, 9, 7 */
	.word 0x40254807
	sd	a1, 0(a0)
	jr	ra
	nop
        .set	mips0
END(octeon_get_control)
#endif
