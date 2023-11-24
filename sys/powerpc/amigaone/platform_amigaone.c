/*-
 * Copyright (c) 2019 Justin Hibbits
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/smp.h>

#include <machine/intr_machdep.h>
#include <machine/platform.h>
#include <machine/platformvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>

#include <powerpc/mpc85xx/mpc85xx.h>

#include "platform_if.h"

static void aeon_pbutton_intr(void *_unused);
static void aeon_setup_intr(void *unused);

static int aeon_probe(platform_t);
static int aeon_attach(platform_t);

static platform_method_t aeon_methods[] = {
	PLATFORMMETHOD(platform_probe,		aeon_probe),
	PLATFORMMETHOD(platform_attach,		aeon_attach),
	PLATFORMMETHOD_END
};

DEFINE_CLASS_1(aeon, aeon_platform, aeon_methods, 0, mpc85xx_platform);

PLATFORM_DEF(aeon_platform);

static bool is_aeon;

static int
aeon_probe(platform_t plat)
{
	phandle_t rootnode;
	char model[32];

	rootnode = OF_finddevice("/");

	if (OF_getprop(rootnode, "model", model, sizeof(model)) > 0) {
		if (strncmp(model, "varisys,", strlen("varisys,")) == 0)
			return (BUS_PROBE_SPECIFIC);
	}

	return (ENXIO);
}

static int
aeon_attach(platform_t plat)
{
	int error;

	error = mpc85xx_attach(plat);
	if (error)
		return (error);

	is_aeon = true;

	return (0);
}

/* Notify devd(8) that the power button was pressed (IRQ#4 on A-Eon machines). */
static void
aeon_pbutton_intr(void *_unused)
{
	devctl_notify("AEON", "power", "press", NULL);
}

/* Manually configure the power button IRQ handler. */
static void
aeon_setup_intr(void *unused)
{
	int irq;

	if (!is_aeon)
		return;

	if (bootverbose)
		printf("Configuring AmigaOne power button.\n");

	irq = 4; /* From TRM, IRQ4 is raised when power button is pressed. */

	/* Get us the root PIC. */
	irq = MAP_IRQ(0, irq);
	powerpc_config_intr(irq, INTR_TRIGGER_EDGE, INTR_POLARITY_LOW);
	powerpc_setup_intr("power_button", irq, NULL, aeon_pbutton_intr, NULL,
	    INTR_TYPE_MISC, NULL, 0);
}

SYSINIT(aeon_setup_intr, SI_SUB_CONFIGURE, SI_ORDER_ANY, aeon_setup_intr, NULL);
