/*-
 * Copyright (c) 2015-2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpuset.h>
#include <sys/interrupt.h>
#include <sys/smp.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/intr.h>

#ifdef SMP
#include <machine/smp.h>
#endif

u_long intrcnt[NIRQS];
size_t sintrcnt = sizeof(intrcnt);

char intrnames[NIRQS * (MAXCOMLEN + 1) * 2];
size_t sintrnames = sizeof(intrnames);

static struct intr_event *intr_events[NIRQS];
static riscv_intrcnt_t riscv_intr_counters[NIRQS];

static int intrcnt_index;

riscv_intrcnt_t
riscv_intrcnt_create(const char* name)
{
	riscv_intrcnt_t counter;

	counter = &intrcnt[intrcnt_index++];
	riscv_intrcnt_setname(counter, name);

	return (counter);
}

void
riscv_intrcnt_setname(riscv_intrcnt_t counter, const char *name)
{
	int i;

	i = (counter - intrcnt);

	KASSERT(counter != NULL, ("riscv_intrcnt_setname: NULL counter"));

	snprintf(intrnames + (MAXCOMLEN + 1) * i,
	    MAXCOMLEN + 1, "%-*s", MAXCOMLEN, name);
}

static void
riscv_mask_irq(void *source)
{
	uintptr_t irq;

	irq = (uintptr_t)source;

	switch (irq) {
	case IRQ_TIMER:
		csr_clear(sie, SIE_STIE);
		break;
	case IRQ_SOFTWARE:
		csr_clear(sie, SIE_SSIE);
		break;
	case IRQ_UART:
		machine_command(ECALL_IO_IRQ_MASK, 0);
		break;
	default:
		panic("Unknown irq %d\n", irq);
	}
}

static void
riscv_unmask_irq(void *source)
{
	uintptr_t irq;

	irq = (uintptr_t)source;

	switch (irq) {
	case IRQ_TIMER:
		csr_set(sie, SIE_STIE);
		break;
	case IRQ_SOFTWARE:
		csr_set(sie, SIE_SSIE);
		break;
	case IRQ_UART:
		machine_command(ECALL_IO_IRQ_MASK, 1);
		break;
	default:
		panic("Unknown irq %d\n", irq);
	}
}

void
riscv_init_interrupts(void)
{
	char name[MAXCOMLEN + 1];
	int i;

	for (i = 0; i < NIRQS; i++) {
		snprintf(name, MAXCOMLEN + 1, "int%d:", i);
		riscv_intr_counters[i] = riscv_intrcnt_create(name);
	}
}

int
riscv_setup_intr(const char *name, driver_filter_t *filt,
    void (*handler)(void*), void *arg, int irq, int flags, void **cookiep)
{
	struct intr_event *event;
	int error;

	if (irq < 0 || irq >= NIRQS)
		panic("%s: unknown intr %d", __func__, irq);

	event = intr_events[irq];
	if (event == NULL) {
		error = intr_event_create(&event, (void *)(uintptr_t)irq, 0,
		    irq, riscv_mask_irq, riscv_unmask_irq,
		    NULL, NULL, "int%d", irq);
		if (error)
			return (error);
		intr_events[irq] = event;
		riscv_unmask_irq((void*)(uintptr_t)irq);
	}

	error = intr_event_add_handler(event, name, filt, handler, arg,
	    intr_priority(flags), flags, cookiep);
	if (error) {
		printf("Failed to setup intr: %d\n", irq);
		return (error);
	}

	riscv_intrcnt_setname(riscv_intr_counters[irq],
			     event->ie_fullname);

	return (0);
}

int
riscv_teardown_intr(void *ih)
{

	/* TODO */

	return (0);
}

int
riscv_config_intr(u_int irq, enum intr_trigger trig, enum intr_polarity pol)
{

	/* There is no configuration for interrupts */

	return (0);
}

void
riscv_cpu_intr(struct trapframe *frame)
{
	struct intr_event *event;
	int active_irq;

	critical_enter();

	KASSERT(frame->tf_scause & EXCP_INTR,
		("riscv_cpu_intr: wrong frame passed"));

	active_irq = (frame->tf_scause & EXCP_MASK);

	switch (active_irq) {
	case IRQ_UART:
	case IRQ_SOFTWARE:
	case IRQ_TIMER:
		event = intr_events[active_irq];
		/* Update counters */
		atomic_add_long(riscv_intr_counters[active_irq], 1);
		PCPU_INC(cnt.v_intr);
		break;
	case IRQ_HTIF:
		/* HTIF interrupts are only handled in machine mode */
		panic("%s: HTIF interrupt", __func__);
		break;
	default:
		event = NULL;
	}

	if (!event || TAILQ_EMPTY(&event->ie_handlers) ||
	    (intr_event_handle(event, frame) != 0))
		printf("stray interrupt %d\n", active_irq);

	critical_exit();
}

#ifdef SMP
void
riscv_setup_ipihandler(driver_filter_t *filt)
{

	riscv_setup_intr("ipi", filt, NULL, NULL, IRQ_SOFTWARE,
	    INTR_TYPE_MISC, NULL);
}

void
riscv_unmask_ipi(void)
{

	csr_set(sie, SIE_SSIE);
}

/* Sending IPI */
static void
ipi_send(struct pcpu *pc, int ipi)
{

	CTR3(KTR_SMP, "%s: cpu=%d, ipi=%x", __func__, pc->pc_cpuid, ipi);

	atomic_set_32(&pc->pc_pending_ipis, ipi);
	machine_command(ECALL_SEND_IPI, pc->pc_reg);

	CTR1(KTR_SMP, "%s: sent", __func__);
}

void
ipi_all_but_self(u_int ipi)
{
	cpuset_t other_cpus;

	other_cpus = all_cpus;
	CPU_CLR(PCPU_GET(cpuid), &other_cpus);

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	ipi_selected(other_cpus, ipi);
}

void
ipi_cpu(int cpu, u_int ipi)
{
	cpuset_t cpus;

	CPU_ZERO(&cpus);
	CPU_SET(cpu, &cpus);

	CTR3(KTR_SMP, "%s: cpu: %d, ipi: %x\n", __func__, cpu, ipi);
	ipi_send(cpuid_to_pcpu[cpu], ipi);
}

void
ipi_selected(cpuset_t cpus, u_int ipi)
{
	struct pcpu *pc;

	CTR1(KTR_SMP, "ipi_selected: ipi: %x", ipi);

	STAILQ_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (CPU_ISSET(pc->pc_cpuid, &cpus)) {
			CTR3(KTR_SMP, "%s: pc: %p, ipi: %x\n", __func__, pc,
			    ipi);
			ipi_send(pc, ipi);
		}
	}
}

#endif
