/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD$
 */

#ifndef _MACHINE_PCB_H_
#define _MACHINE_PCB_H_

/*
 * PCB: process control block
 */
struct pcb {
	uint64_t		pcb_sp;
	uint64_t		pcb_ar_unat;
	uint64_t		pcb_rp;
	uint64_t		pcb_pr;
	struct ia64_fpreg	pcb_f[20];
#define	PCB_F2		0
#define	PCB_F3		1
#define	PCB_F4		2
#define	PCB_F5		3
#define	PCB_F16		4
#define	PCB_F17		5
#define	PCB_F18		6
#define	PCB_F19		7
#define	PCB_F20		8
#define	PCB_F21		9
#define	PCB_F22		10
#define	PCB_F23		11
#define	PCB_F24		12
#define	PCB_F25		13
#define	PCB_F26		14
#define	PCB_F27		15
#define	PCB_F28		16
#define	PCB_F29		17
#define	PCB_F30		18
#define	PCB_F31		19
	uint64_t		pcb_r[4];
#define	PCB_R4		0
#define	PCB_R5		1
#define	PCB_R6		2
#define	PCB_R7		3
	uint64_t		pcb_unat47;
	uint64_t		pcb_b[5];
#define	PCB_B1		0
#define	PCB_B2		1
#define	PCB_B3		2
#define	PCB_B4		3
#define	PCB_B5		4
	uint64_t		pcb_ar_bsp;
	uint64_t		pcb_ar_pfs;
	uint64_t		pcb_ar_rnat;
	uint64_t		pcb_ar_lc;

	uint64_t		pcb_current_pmap;

	uint64_t		pcb_ar_fcr;
	uint64_t		pcb_ar_eflag;
	uint64_t		pcb_ar_csd;
	uint64_t		pcb_ar_ssd;
	uint64_t		pcb_ar_fsr;
	uint64_t		pcb_ar_fir;
	uint64_t		pcb_ar_fdr;

	/* Aligned! */
	struct ia64_fpreg	pcb_highfp[96];	/* f32-f127 */

	uint64_t		pcb_onfault;	/* for copy faults */
	uint64_t		pcb_accessaddr;	/* for [fs]uswintr */
};

#ifdef _KERNEL
void restorectx(struct pcb *);
void savectx(struct pcb *);
#endif

#endif /* _MACHINE_PCB_H_ */
