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
	u_int64_t		pcb_r4;
	u_int64_t		pcb_r5;
	u_int64_t		pcb_r6;
	u_int64_t		pcb_r7;

	struct ia64_fpreg	pcb_f2;
	struct ia64_fpreg	pcb_f3;
	struct ia64_fpreg	pcb_f4;
	struct ia64_fpreg	pcb_f5;

	u_int64_t		pcb_b0;		/* really restart address */
	u_int64_t		pcb_b1;
	u_int64_t		pcb_b2;
	u_int64_t		pcb_b3;
	u_int64_t		pcb_b4;
	u_int64_t		pcb_b5;

	u_int64_t		pcb_old_unat;	/* caller's ar.unat */
	u_int64_t		pcb_sp;
	u_int64_t		pcb_pfs;
	u_int64_t		pcb_bspstore;
	u_int64_t		pcb_lc;

	u_int64_t		pcb_unat;	/* ar.unat for r4..r7 */
	u_int64_t		pcb_rnat;
	u_int64_t		pcb_pr;		/* predicates */
	u_int64_t		pcb_pmap;	/* current pmap */

	u_int64_t		pcb_fsr;
	u_int64_t		pcb_fcr;
	u_int64_t		pcb_fir;
	u_int64_t		pcb_fdr;
	u_int64_t		pcb_eflag;
	u_int64_t		pcb_csd;
	u_int64_t		pcb_ssd;

	u_int64_t		pcb_onfault;	/* for copy faults */
	u_int64_t		pcb_accessaddr;	/* for [fs]uswintr */

	struct ia64_fpreg	pcb_highfp[96];	/* f32-f127 */
};

#ifdef _KERNEL
void restorectx(struct pcb *);
void savectx(struct pcb *);
#endif

#endif /* _MACHINE_PCB_H_ */
