/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/fdt.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/ti/tivar.h>
#include <arm/ti/ti_cpuid.h>

#include <arm/ti/am335x/am335x_reg.h>

static uint32_t chip_revision = 0xffffffff;

/**
 *	ti_revision - Returns the revision number of the device
 *
 *	Simply returns an identifier for the revision of the chip we are running
 *	on.
 *
 *	RETURNS
 *	A 32-bit identifier for the current chip
 */
uint32_t
ti_revision(void)
{
	return chip_revision;
}

static void
am335x_get_revision(void)
{
	uint32_t dev_feature;
	char cpu_last_char;
	bus_space_handle_t bsh;
	int major;
	int minor;

	bus_space_map(fdtbus_bs_tag, AM335X_CONTROL_BASE, AM335X_CONTROL_SIZE, 0, &bsh);
	chip_revision = bus_space_read_4(fdtbus_bs_tag, bsh, AM335X_CONTROL_DEVICE_ID);
	dev_feature = bus_space_read_4(fdtbus_bs_tag, bsh, AM335X_CONTROL_DEV_FEATURE);
	bus_space_unmap(fdtbus_bs_tag, bsh, AM335X_CONTROL_SIZE);

	switch (dev_feature) {
		case 0x00FF0382:
			cpu_last_char='2';
			break;
		case 0x20FF0382:
			cpu_last_char='4';
			break;
		case 0x00FF0383:
			cpu_last_char='6';
			break;
		case 0x00FE0383:
			cpu_last_char='7';
			break;
		case 0x20FF0383:
			cpu_last_char='8';
			break;
		case 0x20FE0383:
			cpu_last_char='9';
			break;
		default:
			cpu_last_char='x';
	}

	switch(AM335X_DEVREV(chip_revision)) {
		case 0:
			major = 1;
			minor = 0;
			break;
		case 1:
			major = 2;
			minor = 0;
			break;
		case 2:
			major = 2;
			minor = 1;
			break;
		default:
			major = 0;
			minor = AM335X_DEVREV(chip_revision);
			break;
	}
	printf("Texas Instruments AM335%c Processor, Revision ES%u.%u\n",
		cpu_last_char, major, minor);
}

/**
 *	ti_cpu_ident - attempts to identify the chip we are running on
 *	@dummy: ignored
 *
 *	This function is called before any of the driver are initialised, however
 *	the basic virt to phys maps have been setup in machdep.c so we can still
 *	access the required registers, we just have to use direct register reads
 *	and writes rather than going through the bus stuff.
 *
 *
 */
static void
ti_cpu_ident(void *dummy)
{
	if (!ti_soc_is_supported())
		return;
	switch(ti_chip()) {
	case CHIP_AM335X:
		am335x_get_revision();
		break;
	default:
		panic("Unknown chip type, fixme!\n");
	}
}

SYSINIT(ti_cpu_ident, SI_SUB_CPU, SI_ORDER_SECOND, ti_cpu_ident, NULL);
