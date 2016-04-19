/*-
 * Copyright (c) 2014 M. Warner Losh. All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <vm/vm.h>

#include <machine/devmap.h>
#include <machine/intr.h>
#include <machine/machdep.h>
#include <machine/platform.h> 

#include <arm/at91/at91var.h>
#include <arm/at91/at91soc.h>
#include <arm/at91/at91_aicreg.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>

#include <machine/fdt.h>

extern const struct arm_devmap_entry at91_devmap[];

struct fdt_fixup_entry fdt_fixup_table[] = {
	{ NULL, NULL }
};

#ifndef INTRNG
static int
fdt_aic_decode_ic(phandle_t node, pcell_t *intr, int *interrupt, int *trig,
    int *pol)
{
	int offset;

	if (fdt_is_compatible(node, "atmel,at91rm9200-aic"))
		offset = 0;
	else
		return (ENXIO);

	*interrupt = fdt32_to_cpu(intr[0]) + offset;
	*trig = INTR_TRIGGER_CONFORM;
	*pol = INTR_POLARITY_CONFORM;

	return (0);
}

fdt_pic_decode_t fdt_pic_table[] = {
	&fdt_aic_decode_ic,
	NULL
};
#endif

static void
at91_eoi(void *unused)
{
	uint32_t *eoicr = (uint32_t *)(0xdffff000 + IC_EOICR);

	*eoicr = 0;
}


vm_offset_t
platform_lastaddr(void)
{

	return (arm_devmap_lastaddr());
}

void
platform_probe_and_attach(void)
{

	arm_post_filter = at91_eoi;
	at91_soc_id();
}

int
platform_devmap_init(void)
{

//	arm_devmap_add_entry(0xfff00000, 0x00100000); /* 1MB - uart, aic and timers*/

	arm_devmap_register_table(at91_devmap);

	return (0);
}

void
platform_gpio_init(void)
{
}

void
platform_late_init(void)
{
}
