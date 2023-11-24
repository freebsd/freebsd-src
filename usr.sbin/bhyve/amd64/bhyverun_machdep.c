/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 */

#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>

#include <vmmapi.h>

#include "acpi.h"
#include "atkbdc.h"
#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "e820.h"
#include "fwctl.h"
#include "ioapic.h"
#include "inout.h"
#include "kernemu_dev.h"
#include "mptbl.h"
#include "pci_irq.h"
#include "pci_lpc.h"
#include "rtc.h"
#include "smbiostbl.h"
#include "xmsr.h"

void
bhyve_init_config(void)
{
	init_config();

	/* Set default values prior to option parsing. */
	set_config_bool("acpi_tables", true);
	set_config_bool("acpi_tables_in_memory", true);
	set_config_value("memory.size", "256M");
	set_config_bool("x86.strictmsr", true);
	set_config_value("lpc.fwcfg", "bhyve");
}

void
bhyve_init_vcpu(struct vcpu *vcpu)
{
	int err, tmp;

	if (get_config_bool_default("x86.vmexit_on_hlt", false)) {
		err = vm_get_capability(vcpu, VM_CAP_HALT_EXIT, &tmp);
		if (err < 0) {
			EPRINTLN("VM exit on HLT not supported");
			exit(4);
		}
		vm_set_capability(vcpu, VM_CAP_HALT_EXIT, 1);
	}

	if (get_config_bool_default("x86.vmexit_on_pause", false)) {
		/*
		 * pause exit support required for this mode
		 */
		err = vm_get_capability(vcpu, VM_CAP_PAUSE_EXIT, &tmp);
		if (err < 0) {
			EPRINTLN("SMP mux requested, no pause support");
			exit(4);
		}
		vm_set_capability(vcpu, VM_CAP_PAUSE_EXIT, 1);
	}

	if (get_config_bool_default("x86.x2apic", false))
		err = vm_set_x2apic_state(vcpu, X2APIC_ENABLED);
	else
		err = vm_set_x2apic_state(vcpu, X2APIC_DISABLED);

	if (err) {
		EPRINTLN("Unable to set x2apic state (%d)", err);
		exit(4);
	}

	vm_set_capability(vcpu, VM_CAP_ENABLE_INVPCID, 1);

	err = vm_set_capability(vcpu, VM_CAP_IPI_EXIT, 1);
	assert(err == 0);
}

void
bhyve_start_vcpu(struct vcpu *vcpu, bool bsp)
{
	int error;

	if (bsp) {
		if (lpc_bootrom()) {
			error = vm_set_capability(vcpu,
			    VM_CAP_UNRESTRICTED_GUEST, 1);
			if (error != 0) {
				err(4, "ROM boot failed: unrestricted guest "
				    "capability not available");
			}
			error = vcpu_reset(vcpu);
			assert(error == 0);
		}
	} else {
		bhyve_init_vcpu(vcpu);

		/*
		 * Enable the 'unrestricted guest' mode for APs.
		 *
		 * APs startup in power-on 16-bit mode.
		 */
		error = vm_set_capability(vcpu, VM_CAP_UNRESTRICTED_GUEST, 1);
		assert(error == 0);
	}

	fbsdrun_addcpu(vcpu_id(vcpu));
}

int
bhyve_init_platform(struct vmctx *ctx, struct vcpu *bsp __unused)
{
	int error;

	error = init_msr();
	if (error != 0)
		return (error);
	init_inout();
	kernemu_dev_init();
	atkbdc_init(ctx);
	pci_irq_init(ctx);
	ioapic_init(ctx);
	rtc_init(ctx);
	sci_init(ctx);
	error = e820_init(ctx);
	if (error != 0)
		return (error);

	return (0);
}

int
bhyve_init_platform_late(struct vmctx *ctx, struct vcpu *bsp __unused)
{
	int error;

	if (get_config_bool_default("x86.mptable", true)) {
		error = mptable_build(ctx, guest_ncpus);
		if (error != 0)
			return (error);
	}
	error = smbios_build(ctx);
	if (error != 0)
		return (error);
	error = e820_finalize();
	if (error != 0)
		return (error);

	if (lpc_bootrom() && strcmp(lpc_fwcfg(), "bhyve") == 0)
		fwctl_init();

	if (get_config_bool("acpi_tables")) {
		error = acpi_build(ctx, guest_ncpus);
		assert(error == 0);
	}

	return (0);
}
