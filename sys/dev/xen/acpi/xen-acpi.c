/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Citrix Systems R&D
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
#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/kobj.h>

#include <machine/_inttypes.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>

#include <xen/xen-os.h>

static int prepare_sleep_state(uint8_t state, uint32_t a, uint32_t b, bool ext)
{
	struct xen_platform_op op = {
		.cmd = XENPF_enter_acpi_sleep,
		.interface_version = XENPF_INTERFACE_VERSION,
		.u.enter_acpi_sleep.val_a = a,
		.u.enter_acpi_sleep.val_b = b,
		.u.enter_acpi_sleep.sleep_state = state,
		.u.enter_acpi_sleep.flags =
		    ext ? XENPF_ACPI_SLEEP_EXTENDED : 0,
	};
	int error;

	error = HYPERVISOR_platform_op(&op);
	if (error)
		printf("Xen notify ACPI sleep failed - "
		    "State %#x A %#x B %#x: %d\n", state, a, b, error);

	return (error ? error : 1);
}

static int init_xen_acpi_sleep(void *arg)
{
	if (!xen_initial_domain())
		return (0);

	acpi_set_prepare_sleep(&prepare_sleep_state);
	return (0);
}

SYSINIT(xen_sleep, SI_SUB_CONFIGURE, SI_ORDER_ANY, init_xen_acpi_sleep, NULL);
