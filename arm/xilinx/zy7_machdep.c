/*-
 * Copyright (c) 2013 Thomas Skibo
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
 *
 * $FreeBSD$
 */

/*
 * Machine dependent code for Xilinx Zynq-7000 Soc.
 *
 * Reference: Zynq-7000 All Programmable SoC Technical Reference Manual.
 * (v1.4) November 16, 2012.  Xilinx doc UG585.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/devmap.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/fdt/fdt_common.h>

#include <machine/bus.h>
#include <machine/machdep.h>
#include <machine/platform.h> 

#include <arm/xilinx/zy7_reg.h>

void (*zynq7_cpu_reset)(void);

vm_offset_t
platform_lastaddr(void)
{

	return (devmap_lastaddr());
}

void
platform_probe_and_attach(void)
{

}

void
platform_gpio_init(void)
{
}

void
platform_late_init(void)
{
}

/*
 * Set up static device mappings.  Not strictly necessary -- simplebus will
 * dynamically establish mappings as needed -- but doing it this way gets us
 * nice efficient 1MB section mappings.
 */
int
platform_devmap_init(void)
{

	devmap_add_entry(ZYNQ7_PSIO_HWBASE, ZYNQ7_PSIO_SIZE);
	devmap_add_entry(ZYNQ7_PSCTL_HWBASE, ZYNQ7_PSCTL_SIZE);

	return (0);
}


struct fdt_fixup_entry fdt_fixup_table[] = {
	{ NULL, NULL }
};

#ifndef INTRNG
static int
fdt_gic_decode_ic(phandle_t node, pcell_t *intr, int *interrupt, int *trig,
    int *pol)
{

	if (!fdt_is_compatible(node, "arm,gic"))
		return (ENXIO);

	*interrupt = fdt32_to_cpu(intr[0]);
	*trig = INTR_TRIGGER_CONFORM;
	*pol = INTR_POLARITY_CONFORM;

	return (0);
}

fdt_pic_decode_t fdt_pic_table[] = {
	&fdt_gic_decode_ic,
	NULL
};
#endif

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

void
cpu_reset()
{
	if (zynq7_cpu_reset != NULL)
		(*zynq7_cpu_reset)();

	printf("cpu_reset: no platform cpu_reset.  hanging.\n");
	for (;;)
		;
}
