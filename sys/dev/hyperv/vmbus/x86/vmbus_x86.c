/*- SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2009-2012,2016-2017, 2022 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * VM Bus Driver Implementation
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/metadata.h>
#include <machine/md_var.h>
#include <machine/resource.h>
#include <machine/intr_machdep.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/vmbus_xact.h>
#include <dev/hyperv/vmbus/hyperv_var.h>
#include <dev/hyperv/vmbus/vmbus_reg.h>
#include <dev/hyperv/vmbus/vmbus_var.h>
#include <dev/hyperv/vmbus/vmbus_chanvar.h>
#include <x86/include/apicvar.h>
#include <dev/hyperv/vmbus/x86/hyperv_machdep.h>
#include <dev/hyperv/vmbus/x86/hyperv_reg.h>
#include <dev/hyperv/vmbus/hyperv_common_reg.h>
#include "acpi_if.h"
#include "pcib_if.h"
#include "vmbus_if.h"

extern inthand_t IDTVEC(vmbus_isr), IDTVEC(vmbus_isr_pti);
#define VMBUS_ISR_ADDR trunc_page((uintptr_t)IDTVEC(vmbus_isr_pti))

void vmbus_handle_timer_intr1(struct vmbus_message *msg_base,
    struct trapframe *frame);
void vmbus_synic_setup1(void *xsc);
void vmbus_synic_teardown1(void);
int vmbus_setup_intr1(struct vmbus_softc *sc);
void vmbus_intr_teardown1(struct vmbus_softc *sc);

void
vmbus_handle_timer_intr1(struct vmbus_message *msg_base,
    struct trapframe *frame)
{
	volatile struct vmbus_message *msg;
	msg = msg_base + VMBUS_SINT_TIMER;
	if (msg->msg_type == HYPERV_MSGTYPE_TIMER_EXPIRED) {
		msg->msg_type = HYPERV_MSGTYPE_NONE;
		vmbus_et_intr(frame);
		/*
		 * Make sure the write to msg_type (i.e. set to
		 * HYPERV_MSGTYPE_NONE) happens before we read the
		 * msg_flags and EOMing. Otherwise, the EOMing will
		 * not deliver any more messages since there is no
		 * empty slot
		 *
		 * NOTE:
		 * mb() is used here, since atomic_thread_fence_seq_cst()
		 * will become compiler fence on UP kernel.
		 */
		mb();
		if (msg->msg_flags & VMBUS_MSGFLAG_PENDING) {
			/*
			 * This will cause message queue rescan to possibly
			 * deliver another msg from the hypervisor
			 */
			wrmsr(MSR_HV_EOM, 0);
		}
	}
	return;
}

void
vmbus_synic_setup1(void *xsc)
{
	struct vmbus_softc *sc = xsc;
	uint32_t sint;
	uint64_t val, orig;

	sint = MSR_HV_SINT0 + VMBUS_SINT_TIMER;
	orig = RDMSR(sint);
	val = sc->vmbus_idtvec | MSR_HV_SINT_AUTOEOI |
	    (orig & MSR_HV_SINT_RSVD_MASK);
	WRMSR(sint, val);
	return;
}

void
vmbus_synic_teardown1(void)
{
	uint64_t orig;
	uint32_t sint;

	sint = MSR_HV_SINT0 + VMBUS_SINT_TIMER;
	orig = RDMSR(sint);
	WRMSR(sint, orig | MSR_HV_SINT_MASKED);
	return;
}

int
vmbus_setup_intr1(struct vmbus_softc *sc)
{
#if defined(__amd64__) && defined(KLD_MODULE)
	pmap_pti_add_kva(VMBUS_ISR_ADDR, VMBUS_ISR_ADDR + PAGE_SIZE, true);
#endif

	/*
	 * All Hyper-V ISR required resources are setup, now let's find a
	 * free IDT vector for Hyper-V ISR and set it up.
	 */
	sc->vmbus_idtvec = lapic_ipi_alloc(
	    pti ? IDTVEC(vmbus_isr_pti) : IDTVEC(vmbus_isr));
	if (sc->vmbus_idtvec < 0) {
#if defined(__amd64__) && defined(KLD_MODULE)
		pmap_pti_remove_kva(VMBUS_ISR_ADDR, VMBUS_ISR_ADDR + PAGE_SIZE);
#endif
		device_printf(sc->vmbus_dev, "cannot find free IDT vector\n");
		return ENXIO;
	}
	if (bootverbose) {
		device_printf(sc->vmbus_dev, "vmbus IDT vector %d\n",
		    sc->vmbus_idtvec);
	}
	return 0;
}

void
vmbus_intr_teardown1(struct vmbus_softc *sc)
{
	int cpu;

	if (sc->vmbus_idtvec >= 0) {
		lapic_ipi_free(sc->vmbus_idtvec);
		sc->vmbus_idtvec = -1;
	}

#if defined(__amd64__) && defined(KLD_MODULE)
	pmap_pti_remove_kva(VMBUS_ISR_ADDR, VMBUS_ISR_ADDR + PAGE_SIZE);
#endif

	CPU_FOREACH(cpu) {
		if (VMBUS_PCPU_GET(sc, event_tq, cpu) != NULL) {
			taskqueue_free(VMBUS_PCPU_GET(sc, event_tq, cpu));
			VMBUS_PCPU_GET(sc, event_tq, cpu) = NULL;
		}
		if (VMBUS_PCPU_GET(sc, message_tq, cpu) != NULL) {
			taskqueue_drain(VMBUS_PCPU_GET(sc, message_tq, cpu),
			    VMBUS_PCPU_PTR(sc, message_task, cpu));
			taskqueue_free(VMBUS_PCPU_GET(sc, message_tq, cpu));
			VMBUS_PCPU_GET(sc, message_tq, cpu) = NULL;
		}
	}
}
