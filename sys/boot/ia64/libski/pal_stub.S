/*
 * Copyright (c) 2003 Marcel Moolenaar
 * Copyright (c) 2001 Doug Rabson
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

	.text
ENTRY(PalProc, 0)
	cmp.eq		p6,p0=6,r28		// PAL_PTCE_INFO
(p6)	br.cond.dptk	pal_ptce_info
	;;
	cmp.eq		p6,p0=8,r28		// PAL_VM_SUMMARY
(p6)	br.cond.dptk	pal_vm_summary
	;;
	cmp.eq		p6,p0=14,r28		// PAL_FREQ_RATIOS
(p6)	br.cond.dptk	pal_freq_ratios
	;;
	cmp.eq		p6,p0=29,r28		// PAL_HALT_LIGHT
(p6)	br.cond.dptk	pal_halt_light
	;;
	mov		r15=66			// EXIT
	break.i		0x80000			// SSC
	;;
pal_ptce_info:
	mov		r8=0
	mov		r9=0			// base
	movl		r10=0x0000000100000001	// loop counts (outer|inner)
	mov		r11=0x0000000000000000	// loop strides (outer|inner)
	br.sptk		b0
pal_vm_summary:
	mov		r8=0
	movl		r9=(8<<40)|(8<<32)	// VM info 1
	mov		r10=(18<<8)|(41<<0)	// VM info 2
	mov		r11=0
	br.sptk		b0
pal_freq_ratios:
	mov		r8=0
	movl		r9=0x0000000B00000002	// processor ratio 11/2
	movl		r10=0x0000000100000001	// bus ratio 1/1
	movl		r11=0x0000000B00000002	// ITC ratio 11/2
	br.sptk		b0
pal_halt_light:
	mov		r8=0
	mov		r9=0
	mov		r10=0
	mov		r11=0
	br.sptk		b0
END(PalProc)
