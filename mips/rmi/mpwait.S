/*-
 * Copyright (c) 2010 RMI Technologies Ltd.
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

#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/cpuregs.h>

#include "assym.s"

	.text
	.set	noat
	.set	noreorder

/*
 * On XLR the slave processors and threads will be executing boot
 * loader code on startup. We need to make them run our code before
 * blowing away boot loader memory.
 */
LEAF(mpwait)
	PTR_LA  gp, _C_LABEL(_gp)
	PTR_LA  t1, _C_LABEL(xlr_ap_release)
	mfc0	t2, $15, 1
	andi	t2, 0x1f
	sll	t2, t2, 2
	add	t1, t2

1:	lw	t0, 0(t1)
	bnez	t0, 2f
	nop     /* We should not busy wait in core0 threads */
	nop     /* on bootup, this will slow the cpu0 thread */
	nop     /* down - TODO - wait with IPI based wakeup */
	nop
	nop
	nop
	nop
	nop
	j	1b
	nop
2:
	PTR_LA  t1, _C_LABEL(mpentry)
	jr	t1
	nop
END(mpwait)
