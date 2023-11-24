/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Brandon Bergren <bdragon@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

#include <powerpc/powermac/macgpiovar.h>
#include <powerpc/powermac/platform_powermac.h>

static int	tbgpio_probe(device_t);
static int	tbgpio_attach(device_t);
static void	tbgpio_freeze_timebase(device_t, bool);

static device_method_t	tbgpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tbgpio_probe),
	DEVMETHOD(device_attach,	tbgpio_attach),
	DEVMETHOD_END
};

struct tbgpio_softc {
	uint32_t	sc_value;
	uint32_t	sc_mask;
};

static driver_t tbgpio_driver = {
	"tbgpio",
	tbgpio_methods,
	sizeof(struct tbgpio_softc)
};

EARLY_DRIVER_MODULE(tbgpio, macgpio, tbgpio_driver, 0, 0, BUS_PASS_CPU);

static int
tbgpio_probe(device_t dev)
{
	phandle_t node;
	const char *name;
	pcell_t pfunc[32];
	int res;

	name = ofw_bus_get_name(dev);
	node = ofw_bus_get_node(dev);

	if (strcmp(name, "timebase-enable") != 0)
		return (ENXIO);

	res = OF_getencprop(node, "platform-do-cpu-timebase", pfunc,
	    sizeof(pfunc));
	if (res == -1)
		return (ENXIO);

	/*
	 * If this doesn't look like a simple gpio_write pfunc,
	 * complain about it so we can collect the pfunc.
	 */
	if (res != 20 || pfunc[2] != 0x01) {
		printf("\nUnknown platform function detected!\n");
		printf("Please send a PR including the following data:\n");
		printf("===================\n");
		printf("Func: platform-do-cpu-timebase\n");
		hexdump(pfunc, res, NULL, HD_OMIT_CHARS);
		printf("===================\n");
		return (ENXIO);
	}

	device_set_desc(dev, "CPU Timebase Control");
	return (BUS_PROBE_SPECIFIC);
}

static int
tbgpio_attach(device_t dev)
{
	phandle_t node;
	struct tbgpio_softc *sc;

	/*
	 * Structure of pfunc:
	 * pfunc[0]: phandle to /cpus
	 * pfunc[1]: flags
	 * pfunc[2]: 0x1 == CMD_WRITE_GPIO
	 * pfunc[3]: value
	 * pfunc[4]: mask
	 */
	pcell_t pfunc[5];

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	OF_getencprop(node, "platform-do-cpu-timebase", pfunc, sizeof(pfunc));

	sc->sc_value = pfunc[3];
	sc->sc_mask = pfunc[4];

	powermac_register_timebase(dev, tbgpio_freeze_timebase);
	return (0);
}

static void
tbgpio_freeze_timebase(device_t dev, bool freeze)
{
	struct tbgpio_softc *sc;
	uint32_t val;

	sc = device_get_softc(dev);

	val = sc->sc_value;
	if (freeze)
		val = ~val;
	val &= sc->sc_mask;

	macgpio_write(dev, val);
}
