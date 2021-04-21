/*-
 * SPDX-License-Identifier: MIT OR GPL-2.0-only
 *
 * Copyright © 2013 Spectra Logic Corporation
 * Copyright © 2018 John Baldwin/The FreeBSD Foundation
 * Copyright © 2019 Roger Pau Monné/Citrix Systems R&D
 * Copyright © 2021 Elliott Mitchell
 *
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/stddef.h>

#include <xen/xen-os.h>
#include <xen/xen_intr.h>

#include <x86/intr_machdep.h>
#include <x86/apicvar.h>

/************************ Xen x86 interrupt interface ************************/

/*
 * Pointers to the interrupt counters
 */
DPCPU_DEFINE_STATIC(u_long *, pintrcnt);

static void
xen_intrcnt_init(void *dummy __unused)
{
	unsigned int i;

	if (!xen_domain())
		return;

	CPU_FOREACH(i) {
		char buf[MAXCOMLEN + 1];

		snprintf(buf, sizeof(buf), "cpu%d:xen", i);
		intrcnt_add(buf, DPCPU_ID_PTR(i, pintrcnt));
	}
}
SYSINIT(xen_intrcnt_init, SI_SUB_INTR, SI_ORDER_MIDDLE, xen_intrcnt_init, NULL);

/*
 * Transition from assembly language, called from
 * sys/{amd64/amd64|i386/i386}/apic_vector.S
 */
extern void xen_arch_intr_handle_upcall(struct trapframe *);
void
xen_arch_intr_handle_upcall(struct trapframe *trap_frame)
{
	struct trapframe *old;

	/*
	 * Disable preemption in order to always check and fire events
	 * on the right vCPU
	 */
	critical_enter();

	++*DPCPU_GET(pintrcnt);

	++curthread->td_intr_nesting_level;
	old = curthread->td_intr_frame;
	curthread->td_intr_frame = trap_frame;

	xen_intr_handle_upcall(NULL);

	curthread->td_intr_frame = old;
	--curthread->td_intr_nesting_level;

	if (xen_evtchn_needs_ack)
		lapic_eoi();

	critical_exit();
}
