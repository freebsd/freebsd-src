/*-
 * Copyright (c) Peter Wemm <peter@netplex.com.au>
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
 * $FreeBSD: src/sys/i386/include/privatespace.h,v 1.3 2001/12/11 23:33:40 jhb Exp $
 */

#ifndef _MACHINE_PRIVATESPACE_H_
#define _MACHINE_PRIVATESPACE_H_

/*
 * This is the upper (0xff800000) address space layout that is per-cpu.
 * It is setup in locore.s and pmap.c for the BSP and in mp_machdep.c for
 * each AP.  This is only applicable to the x86 SMP kernel.
 */
struct privatespace {
	/* page 0 - data page */
	struct	pcpu pcpu;
	char	__filler0[PAGE_SIZE - sizeof(struct pcpu)];

	/* page 1 - idle stack (KSTACK_PAGES pages) */
	char	idlekstack[KSTACK_PAGES * PAGE_SIZE];
	/* page 1+KSTACK_PAGES... */
};

extern struct privatespace SMP_prvspace[];

#endif	/* ! _MACHINE_PRIVATESPACE_H_ */
