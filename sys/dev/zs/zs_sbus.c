/*-
 * Copyright (c) 2003 Jake Burkholder.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <ofw/openfirm.h>

#include <sparc64/fhc/fhcreg.h>
#include <sparc64/fhc/fhcvar.h>

#include <sparc64/sbus/sbusvar.h>

#include <dev/zs/z8530reg.h>
#include <dev/zs/z8530var.h>

#define	ZS_SBUS_CHAN_A		4
#define	ZS_SBUS_CHAN_B		0

#define	ZS_SBUS_CSR		0
#define	ZS_SBUS_DATA		2

#define	ZS_SBUS_CLOCK		(9600 * 512)
#define	ZS_SBUS_CLOCK_DIV	(16)

#define	ZS_SBUS_DEF_SPEED	(9600)

enum zs_device_ivars {
	ZS_IVAR_NODE
};

__BUS_ACCESSOR(zs, node, ZS, NODE, phandle_t)

struct zs_sbus_softc {
	struct zs_softc		sc_zs;
	struct resource		*sc_irqres;
	int			sc_irqrid;
	void			*sc_ih;
	struct resource		*sc_memres;
	int			sc_memrid;
};

static int zs_fhc_probe(device_t dev);
static int zs_fhc_attach(device_t dev);
static int zs_fhc_read_ivar(device_t dev, device_t child, int which,
    uintptr_t *result);

static int zs_sbus_probe(device_t dev);
static int zs_sbus_attach(device_t dev);
static int zs_sbus_read_ivar(device_t dev, device_t child, int which,
    uintptr_t *result);

static int zs_sbus_detach(device_t dev);

static int zstty_sbus_attach(device_t dev);
static int zstty_sbus_probe(device_t dev);

static int zstty_keyboard(device_t dev);

static device_method_t zs_fhc_methods[] = {
	DEVMETHOD(device_probe,		zs_fhc_probe),
	DEVMETHOD(device_attach,	zs_fhc_attach),
	DEVMETHOD(device_detach,	zs_sbus_detach),

	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	zs_fhc_read_ivar),

	{ 0, 0 }
};

static device_method_t zs_sbus_methods[] = {
	DEVMETHOD(device_probe,		zs_sbus_probe),
	DEVMETHOD(device_attach,	zs_sbus_attach),
	DEVMETHOD(device_detach,	zs_sbus_detach),

	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	zs_sbus_read_ivar),

	{ 0, 0 }
};

static device_method_t zstty_sbus_methods[] = {
	DEVMETHOD(device_probe,		zstty_sbus_probe),
	DEVMETHOD(device_attach,	zstty_sbus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),

	{ 0, 0 }
};

static driver_t zs_fhc_driver = {
	"zs",
	zs_fhc_methods,
	sizeof(struct zs_sbus_softc),
};

static driver_t zs_sbus_driver = {
	"zs",
	zs_sbus_methods,
	sizeof(struct zs_sbus_softc),
};

static driver_t zstty_sbus_driver = {
	"zstty",
	zstty_sbus_methods,
	sizeof(struct zstty_softc),
};

static devclass_t zs_fhc_devclass;
static devclass_t zs_sbus_devclass;

DRIVER_MODULE(zs, fhc, zs_fhc_driver, zs_fhc_devclass, 0, 0);
DRIVER_MODULE(zs, sbus, zs_sbus_driver, zs_sbus_devclass, 0, 0);

static devclass_t zstty_sbus_devclass;

DRIVER_MODULE(zstty, zs, zstty_sbus_driver, zstty_sbus_devclass, 0, 0);

static uint8_t zs_sbus_init_reg[16] = {
	0,	/* 0: CMD (reset, etc.) */
	0,	/* 1: No interrupts yet. */
	0,	/* 2: IVECT */
	ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
	ZSWR4_CLK_X16 | ZSWR4_ONESB | ZSWR4_EVENP,
	ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
	0,	/* 6: TXSYNC/SYNCLO */
	0,	/* 7: RXSYNC/SYNCHI */
	0,	/* 8: alias for data port */
	ZSWR9_MASTER_IE | ZSWR9_NO_VECTOR,
	0,	/* 10: Misc. TX/RX control bits */
	ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
	((ZS_SBUS_CLOCK/32)/ZS_SBUS_DEF_SPEED)-2,
	0,
	ZSWR14_BAUD_ENA | ZSWR14_BAUD_FROM_PCLK,
	ZSWR15_BREAK_IE,
};

static int
zs_fhc_probe(device_t dev)
{

	if (strcmp(fhc_get_name(dev), "zs") != 0 ||
	    device_get_unit(dev) != 0)
		return (ENXIO);
	return (zs_probe(dev));
}

static int
zs_sbus_probe(device_t dev)
{

	if (strcmp(sbus_get_name(dev), "zs") != 0 ||
	    device_get_unit(dev) != 0)
		return (ENXIO);
	return (zs_probe(dev));
}

static int
zs_fhc_attach(device_t dev)
{
	struct zs_sbus_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_memres = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->sc_memrid,
	    0, ~0, 1, RF_ACTIVE);
	if (sc->sc_memres == NULL)
		goto error;
	sc->sc_irqrid = FHC_UART;
	sc->sc_irqres = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->sc_irqrid, 0,
	    ~0, 1, RF_ACTIVE);
	if (sc->sc_irqres == NULL)
		goto error;
	if (bus_setup_intr(dev, sc->sc_irqres, INTR_TYPE_TTY | INTR_FAST,
	    zs_intr, sc, &sc->sc_ih) != 0)
		goto error;
	sc->sc_zs.sc_bt = rman_get_bustag(sc->sc_memres);
	sc->sc_zs.sc_bh = rman_get_bushandle(sc->sc_memres);
	return (zs_attach(dev));

error:
	zs_sbus_detach(dev);
	return (ENXIO);
}

static int
zs_sbus_attach(device_t dev)
{
	struct zs_sbus_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_memres = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->sc_memrid,
	    0, ~0, 1, RF_ACTIVE);
	if (sc->sc_memres == NULL)
		goto error;
	sc->sc_irqres = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->sc_irqrid, 0,
	    ~0, 1, RF_ACTIVE);
	if (sc->sc_irqres == NULL)
		goto error;
	if (bus_setup_intr(dev, sc->sc_irqres, INTR_TYPE_TTY | INTR_FAST,
	    zs_intr, sc, &sc->sc_ih) != 0)
		goto error;
	sc->sc_zs.sc_bt = rman_get_bustag(sc->sc_memres);
	sc->sc_zs.sc_bh = rman_get_bushandle(sc->sc_memres);
	return (zs_attach(dev));

error:
	zs_sbus_detach(dev);
	return (ENXIO);
}

static int
zs_sbus_detach(device_t dev)
{
	struct zs_sbus_softc *sc;

	sc = device_get_softc(dev);
	if (sc->sc_irqres != NULL) {
		if (sc->sc_ih != NULL)
			bus_teardown_intr(dev, sc->sc_irqres, sc->sc_ih);
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irqrid,
		    sc->sc_irqres);
	}
	if (sc->sc_memres != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_memrid,
		    sc->sc_memres);
	return (0);
}

static int
zs_fhc_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{

	switch (which) {
	case ZS_IVAR_NODE:
		*result = fhc_get_node(dev);
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

static int
zs_sbus_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{

	switch (which) {
	case ZS_IVAR_NODE:
		*result = sbus_get_node(dev);
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

static int
zstty_sbus_probe(device_t dev)
{

	if (zstty_keyboard(dev)) {
		if ((device_get_unit(dev) & 1) == 0)
			device_set_desc(dev, "keyboard");
		else
			device_set_desc(dev, "mouse");
	} else {
		if ((device_get_unit(dev) & 1) == 0)
			device_set_desc(dev, "ttya");
		else
			device_set_desc(dev, "ttyb");
	}
	return (zstty_probe(dev));
}

static int
zstty_sbus_attach(device_t dev)
{
	struct zstty_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_parent = device_get_softc(device_get_parent(dev));
	sc->sc_bt = sc->sc_parent->sc_bt;
	sc->sc_brg_clk = ZS_SBUS_CLOCK / ZS_SBUS_CLOCK_DIV;

	switch (device_get_unit(dev) & 1) {
	case 0:
		bus_space_subregion(sc->sc_bt, sc->sc_parent->sc_bh,
		    ZS_SBUS_CHAN_A + ZS_SBUS_CSR, 1, &sc->sc_csr);
		bus_space_subregion(sc->sc_bt, sc->sc_parent->sc_bh,
		    ZS_SBUS_CHAN_A + ZS_SBUS_DATA, 1, &sc->sc_data);
		break;
	case 1:
		bus_space_subregion(sc->sc_bt, sc->sc_parent->sc_bh,
		    ZS_SBUS_CHAN_B + ZS_SBUS_CSR, 1, &sc->sc_csr);
		bus_space_subregion(sc->sc_bt, sc->sc_parent->sc_bh,
		    ZS_SBUS_CHAN_B + ZS_SBUS_DATA, 1, &sc->sc_data);
		break;
	}

	bcopy(zs_sbus_init_reg, sc->sc_creg, 16);
	bcopy(zs_sbus_init_reg, sc->sc_preg, 16);

	return (zstty_attach(dev));
}

void
zstty_set_speed(struct zstty_softc *sc, int ospeed)
{

	sc->sc_preg[12] = ospeed;
	sc->sc_preg[13] = ospeed >> 8;
}

int
zstty_console(device_t dev, char *mode, int len)
{
	phandle_t chosen;
	phandle_t options;
	ihandle_t stdin;
	ihandle_t stdout;
	char output[32];
	char input[32];
	char name[32];

	chosen = OF_finddevice("/chosen");
	options = OF_finddevice("/options");
	if (OF_getprop(chosen, "stdin", &stdin, sizeof(stdin)) == -1 ||
	    OF_getprop(chosen, "stdout", &stdout, sizeof(stdout)) == -1 ||
	    OF_getprop(options, "input-device", input, sizeof(input)) == -1 ||
	    OF_getprop(options, "output-device", output, sizeof(output)) == -1)
		return (0);
	if (zs_get_node(dev) != OF_instance_to_package(stdin) ||
	    zs_get_node(dev) != OF_instance_to_package(stdout))
		return (0);
	if ((strcmp(input, device_get_desc(dev)) == 0 &&
	     strcmp(output, device_get_desc(dev)) == 0) ||
	    (strcmp(input, "keyboard") == 0 && strcmp(output, "screen") == 0 &&
	     (device_get_unit(dev) & 1) == 0 && !zstty_keyboard(dev))) {
		if (mode != NULL) {
			sprintf(name, "%s-mode", device_get_desc(dev));
			return (OF_getprop(options, name, mode, len) != -1);
		} else
			return (1);
	}
	return (0);
}

static int
zstty_keyboard(device_t dev)
{

	return (OF_getproplen(zs_get_node(dev), "keyboard") == 0);
}
