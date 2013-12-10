/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#ifndef _VMM_LAPIC_H_
#define	_VMM_LAPIC_H_

struct vm;

boolean_t lapic_msr(u_int num);
int	lapic_rdmsr(struct vm *vm, int cpu, u_int msr, uint64_t *rval);
int	lapic_wrmsr(struct vm *vm, int cpu, u_int msr, uint64_t wval);

int	lapic_mmio_read(void *vm, int cpu, uint64_t gpa,
			uint64_t *rval, int size, void *arg);
int	lapic_mmio_write(void *vm, int cpu, uint64_t gpa,
			 uint64_t wval, int size, void *arg);

/*
 * Returns a vector between 32 and 255 if an interrupt is pending in the
 * IRR that can be delivered based on the current state of ISR and TPR.
 *
 * Note that the vector does not automatically transition to the ISR as a
 * result of calling this function.
 *
 * Returns -1 if there is no eligible vector that can be delivered to the
 * guest at this time.
 */
int	lapic_pending_intr(struct vm *vm, int cpu);

/*
 * Transition 'vector' from IRR to ISR. This function is called with the
 * vector returned by 'lapic_pending_intr()' when the guest is able to
 * accept this interrupt (i.e. RFLAGS.IF = 1 and no conditions exist that
 * block interrupt delivery).
 */
void	lapic_intr_accepted(struct vm *vm, int cpu, int vector);

/*
 * Signals to the LAPIC that an interrupt at 'vector' needs to be generated
 * to the 'cpu', the state is recorded in IRR.
 */
int	lapic_set_intr(struct vm *vm, int cpu, int vector, bool trig);

#define	LAPIC_TRIG_LEVEL	true
#define	LAPIC_TRIG_EDGE		false
static __inline int
lapic_intr_level(struct vm *vm, int cpu, int vector)
{

	return (lapic_set_intr(vm, cpu, vector, LAPIC_TRIG_LEVEL));
}

static __inline int
lapic_intr_edge(struct vm *vm, int cpu, int vector)
{

	return (lapic_set_intr(vm, cpu, vector, LAPIC_TRIG_EDGE));
}

#endif
