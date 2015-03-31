/*-
 * Copyright 2013-2015 John Wehle <john@feith.com>
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

#include "opt_global.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/devmap.h>
#include <machine/machdep.h>
#include <machine/platform.h>

#include <dev/fdt/fdt_common.h>

#include <arm/amlogic/aml8726/aml8726_clkmsr.h>

#if defined(SOCDEV_PA) && defined(SOCDEV_VA)
vm_offset_t aml8726_aobus_kva_base = SOCDEV_VA;
#else
vm_offset_t aml8726_aobus_kva_base;
#endif

static void
aml8726_fixup_busfreq()
{
	phandle_t node, child;
	pcell_t freq, prop;
	ssize_t len;

	/*
	 * Set the bus-frequency for any top level SoC simple-bus which
	 * needs updating (meaning the current frequency is zero).
	 */

	if ((freq = aml8726_clkmsr_bus_frequency()) == 0 ||
	    (node = OF_finddevice("/soc")) == 0 ||
	    fdt_is_compatible_strict(node, "simple-bus") == 0)
		while (1);

	freq = cpu_to_fdt32(freq);

	len = OF_getencprop(node, "bus-frequency", &prop, sizeof(prop));
	if ((len / sizeof(prop)) == 1 && prop == 0)
		OF_setprop(node, "bus-frequency", (void *)&freq, sizeof(freq));

	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if (fdt_is_compatible_strict(child, "simple-bus")) {
			len = OF_getencprop(child, "bus-frequency",
			    &prop, sizeof(prop));
			if ((len / sizeof(prop)) == 1 && prop == 0)
				OF_setprop(child, "bus-frequency",
				    (void *)&freq, sizeof(freq));
		}
	}
}

vm_offset_t
platform_lastaddr(void)
{

	return (arm_devmap_lastaddr());
}

void
platform_probe_and_attach(void)
{
}

void
platform_gpio_init(void)
{

	/*
	 * The UART console driver used for debugging early boot code
	 * needs to know the virtual base address of the aobus.  It's
	 * expected to equal SOCDEV_VA prior to initarm calling setttb
	 * ... afterwards it needs to be updated due to the new page
	 * tables.
	 *
	 * This means there's a deadzone in initarm between setttb
	 * and platform_gpio_init during which printf can't be used.
	 */
	aml8726_aobus_kva_base =
	    (vm_offset_t)arm_devmap_ptov(0xc8100000, 0x100000);

	/*
	 * This FDT fixup should arguably be called through fdt_fixup_table,
	 * however currently there's no mechanism to specify a fixup which
	 * should always be invoked.
	 *
	 * It needs to be called prior to the console being initialized which
	 * is why it's called here, rather than from platform_late_init.
	 */
	aml8726_fixup_busfreq();
}

void
platform_late_init(void)
{
}

/*
 * Construct static devmap entries to map out the core
 * peripherals using 1mb section mappings.
 */
int
platform_devmap_init(void)
{

	arm_devmap_add_entry(0xc1100000, 0x200000); /* cbus */
	arm_devmap_add_entry(0xc4200000, 0x100000); /* pl310 */
	arm_devmap_add_entry(0xc4300000, 0x100000); /* periph */
	arm_devmap_add_entry(0xc8000000, 0x100000); /* apbbus */
	arm_devmap_add_entry(0xc8100000, 0x100000); /* aobus */
	arm_devmap_add_entry(0xc9000000, 0x800000); /* ahbbus */
	arm_devmap_add_entry(0xd9000000, 0x100000); /* ahb */
	arm_devmap_add_entry(0xda000000, 0x100000); /* secbus */

	return (0);
}

struct arm32_dma_range *
bus_dma_get_range(void)
{

	return (NULL);
}

int
bus_dma_get_range_nb(void)
{

	return (0);
}

struct fdt_fixup_entry fdt_fixup_table[] = {
	{ NULL, NULL }
};

static int
fdt_pic_decode_ic(phandle_t node, pcell_t *intr, int *interrupt, int *trig,
    int *pol)
{

	/*
	 * The single core chips have just an Amlogic PIC.  However the
	 * multi core chips also have a GIC.
	 */
#ifdef SMP
	if (!fdt_is_compatible_strict(node, "arm,gic"))
#else
	if (!fdt_is_compatible_strict(node, "amlogic,aml8726-pic"))
#endif
		return (ENXIO);

	*interrupt = fdt32_to_cpu(intr[0]);
	*trig = INTR_TRIGGER_EDGE;
	*pol = INTR_POLARITY_HIGH;

	switch (*interrupt) {
	case 30: /* INT_USB_A */
	case 31: /* INT_USB_B */
		*trig = INTR_TRIGGER_LEVEL;
		break;
	default:
		break;
	}

#ifdef SMP
	*interrupt += 32;
#endif

	return (0);
}

fdt_pic_decode_t fdt_pic_table[] = {
	&fdt_pic_decode_ic,
	NULL
};
