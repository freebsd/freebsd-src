/*	$OpenBSD$ */
/*
 * Copyright (c) 2001 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 *	This product includes software developed by Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	JNPR: psraccess.S,v 1.4.2.1 2007/09/10 10:36:50 girish
 * $FreeBSD$
 *
 */

/*
 *  Low level code to manage processor specific registers.
 */

#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>

#include "opt_cputype.h"

#include "assym.s"

/*
 * FREEBSD_DEVELOPERS_FIXME
 * Some MIPS CPU may need delays using nops between executing CP0 Instructions
 */
#define	MIPS_CPU_NOP_DELAY	nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;

	.set	noreorder		# Noreorder is default style!

/*
 * Set/clear software interrupt.
 */

LEAF(setsoftintr0)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	or	v0, v0, SOFT_INT_MASK_0 # set soft clock interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(setsoftintr0)

LEAF(clearsoftintr0)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	and	v0, v0, ~SOFT_INT_MASK_0  # clear soft clock interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(clearsoftintr0)

LEAF(setsoftintr1)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	or	v0, v0, SOFT_INT_MASK_1 # set soft net interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(setsoftintr1)

LEAF(clearsoftintr1)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	and	v0, v0, ~SOFT_INT_MASK_1  # clear soft net interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(clearsoftintr1)

/*
 * Set/change interrupt priority routines.
 * These routines return the previous state.
 */
LEAF(restoreintr)
	mfc0	t0,COP_0_STATUS_REG
	and	t1,t0,SR_INT_ENAB
	beq	a0,t1,1f
	xor	t0,SR_INT_ENAB
  
	.set	noreorder

	mtc0	t0,COP_0_STATUS_REG
	nop
	nop
	nop
	nop
1:
	j	ra
	nop
END(restoreintr)

/*
 * Set/change interrupt priority routines.
 * These routines return the previous state.
 */
 
LEAF(enableintr)
#ifdef TARGET_OCTEON
	.set mips64r2
	ei     v0
	and	v0, SR_INT_ENAB		# return old interrupt enable bit
        .set	mips0
#else		
	mfc0	v0, COP_0_STATUS_REG	# read status register
	nop
	or	v1, v0, SR_INT_ENAB
	mtc0	v1, COP_0_STATUS_REG	# enable all interrupts
	and	v0, SR_INT_ENAB		# return old interrupt enable
#endif	
	j	ra
	nop
END(enableintr)


LEAF(disableintr)
#ifdef TARGET_OCTEON
	.set mips64r2
	di     v0
	and	v0, SR_INT_ENAB		# return old interrupt enable bit
	.set	mips0
#else		
	mfc0	v0, COP_0_STATUS_REG	# read status register
	nop
	and	v1, v0, ~SR_INT_ENAB
	mtc0	v1, COP_0_STATUS_REG	# disable all interrupts
	MIPS_CPU_NOP_DELAY
	and	v0, SR_INT_ENAB		# return old interrupt enable
#endif	
	j	ra
	nop
END(disableintr)

LEAF(set_intr_mask)
	li	t0, SR_INT_MASK		# 1 means masked so invert.
	not	a0, a0			# 1 means masked so invert.
	and	a0, t0			# 1 means masked so invert.
	mfc0	v0, COP_0_STATUS_REG
	li	v1, ~SR_INT_MASK
	and	v1, v0
	or	v1, a0
	mtc0	v1, COP_0_STATUS_REG
	MIPS_CPU_NOP_DELAY
	move	v0, v1
	jr	ra
	nop

END(set_intr_mask)

LEAF(get_intr_mask)
	li	a0, 0
	mfc0	v0, COP_0_STATUS_REG
	li	v1, SR_INT_MASK
	and	v0, v1
	or	v0, a0
	jr	ra
	nop

END(get_intr_mask)

/*
 * u_int32_t mips_cp0_config1_read(void)
 *
 *	Return the current value of the CP0 Config (Select 1) register.
 */
LEAF(mips_cp0_config1_read)
	.set	push
	.set	mips32
	mfc0	v0, COP_0_CONFIG, 1
	j	ra
	nop
	.set	pop
END(mips_cp0_config1_read)
