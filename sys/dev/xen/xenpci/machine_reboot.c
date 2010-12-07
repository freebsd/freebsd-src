/*-
 * Copyright (c) 2008 Citrix Systems, Inc.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: user/dfr/xenhvm/7/sys/dev/xen/xenpci/machine_reboot.c 186766 2009-01-05 10:41:54Z dfr $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/interrupt.h>

#include <machine/xen/xen-os.h>
#include <xen/hypervisor.h>
#include <xen/xen_intr.h>

#include <dev/xen/xenpci/xenpcivar.h>

void
xen_suspend()
{
	int suspend_cancelled;

	if (DEVICE_SUSPEND(root_bus)) {
		printf("xen_suspend: device_suspend failed\n");
		return;
	}

	/*
	 * Make sure we don't change cpus or switch to some other
	 * thread. for the duration.
	 */
	critical_enter();

	/*
	 * Prevent any races with evtchn_interrupt() handler.
	 */
	irq_suspend();
	disable_intr();

	suspend_cancelled = HYPERVISOR_suspend(0);
	if (!suspend_cancelled)
		xenpci_resume();

	/*
	 * Re-enable interrupts and put the scheduler back to normal.
	 */
	enable_intr();
	critical_exit();

	/*
	 * FreeBSD really needs to add DEVICE_SUSPEND_CANCEL or
	 * similar.
	 */
	if (!suspend_cancelled)
		DEVICE_RESUME(root_bus);
}
