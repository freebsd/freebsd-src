/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 The FreeBSD Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VMM_VTIMER_H_
#define _VMM_VTIMER_H_

#define	GT_PHYS_NS_IRQ	30
#define	GT_VIRT_IRQ	27

struct hyp;
struct hypctx;

struct vtimer {
	uint64_t	cnthctl_el2;
	uint64_t	cntvoff_el2;
};

struct vtimer_timer {
	struct callout	callout;
	struct mtx	mtx;

	uint32_t	irqid;

	/*
	 * These registers are either emulated for the physical timer, or
	 * the guest has full access to them for the virtual timer.

	 * CNTx_CTL_EL0:  Counter-timer Timer Control Register
	 * CNTx_CVAL_EL0: Counter-timer Timer CompareValue Register
	 */
	uint64_t	cntx_cval_el0;
	uint64_t	cntx_ctl_el0;
};

struct vtimer_cpu {
	struct vtimer_timer phys_timer;
	struct vtimer_timer virt_timer;

	uint32_t	cntkctl_el1;
};

int 	vtimer_init(uint64_t cnthctl_el2);
void 	vtimer_vminit(struct hyp *);
void 	vtimer_cpuinit(struct hypctx *);
void 	vtimer_cpucleanup(struct hypctx *);
void	vtimer_vmcleanup(struct hyp *);
void	vtimer_cleanup(void);
void	vtimer_sync_hwstate(struct hypctx *hypctx);

int 	vtimer_phys_ctl_read(struct vcpu *vcpu, uint64_t *rval, void *arg);
int 	vtimer_phys_ctl_write(struct vcpu *vcpu, uint64_t wval, void *arg);
int 	vtimer_phys_cnt_read(struct vcpu *vcpu, uint64_t *rval, void *arg);
int 	vtimer_phys_cnt_write(struct vcpu *vcpu, uint64_t wval, void *arg);
int 	vtimer_phys_cval_read(struct vcpu *vcpu, uint64_t *rval, void *arg);
int 	vtimer_phys_cval_write(struct vcpu *vcpu, uint64_t wval, void *arg);
int 	vtimer_phys_tval_read(struct vcpu *vcpu, uint64_t *rval, void *arg);
int 	vtimer_phys_tval_write(struct vcpu *vcpu, uint64_t wval, void *arg);
#endif
