/*-
 * Copyright (c) 2003 Jake Burkholder, Benno Rice.
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
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/ofw/openfirm.h>
#include <powerpc/powermac/maciovar.h>

#include <dev/zs/z8530reg.h>
#include <dev/zs/z8530var.h>

#define	ZS_MACIO_CHAN_A	32
#define	ZS_MACIO_CHAN_B	0

#define	ZS_MACIO_CSR	0
#define	ZS_MACIO_DATA	16

#define	ZS_MACIO_DEF_SPEED	(57600)

#define	ZS_MACIO_CLOCK		(9600 * 384)
#define	ZS_MACIO_CLOCK_DIV	(16)

struct zs_macio_softc {
	struct zs_softc		sc_zs;
	struct resource		*sc_irqres1, *sc_irqres2;
	int			sc_irqrid1, sc_irqrid2;
	void			*sc_ih1, *sc_ih2;
	struct resource		*sc_memres;
	int			sc_memrid;
};

static int zs_macio_attach(device_t dev);
static int zs_macio_detach(device_t dev);
static int zs_macio_probe(device_t dev);

static int zstty_macio_attach(device_t dev);
static int zstty_macio_probe(device_t dev);

static device_method_t zs_macio_methods[] = {
	DEVMETHOD(device_probe,		zs_macio_probe),
	DEVMETHOD(device_attach,	zs_macio_attach),
	DEVMETHOD(device_detach,	zs_macio_detach),

	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	{ 0, 0 }
};

static device_method_t zstty_macio_methods[] = {
	DEVMETHOD(device_probe,		zstty_macio_probe),
	DEVMETHOD(device_attach,	zstty_macio_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),

	{ 0, 0}
};

static driver_t zs_macio_driver = {
	"zs",
	zs_macio_methods,
	sizeof(struct zs_macio_softc),
};

static driver_t zstty_macio_driver = {
	"zstty",
	zstty_macio_methods,
	sizeof(struct zstty_softc),
};

static devclass_t zs_macio_devclass;
static devclass_t zstty_macio_devclass;

DRIVER_MODULE(zstty, zs, zstty_macio_driver, zstty_macio_devclass, 0, 0);
DRIVER_MODULE(zs, macio, zs_macio_driver, zs_macio_devclass, 0, 0);

static u_char zs_macio_init_reg[16] = {
	0,	/* 0: CMD (reset, etc.) */
	0,	/* 1: No interrupts yet. */
	0,	/* IVECT */
	ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
	ZSWR4_CLK_X16 | ZSWR4_ONESB,
	ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
	0,	/* 6: TXSYNC/SYNCLO */
	0,	/* 7: RXSYNC/SYNCHI */
	0,	/* 8: alias for data port */
	ZSWR9_MASTER_IE | ZSWR9_NO_VECTOR,
	0,	/*10: Misc. TX/RX control bits */
	ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
#if 0
	((ZS_MACIO_CLOCK/32)/ZS_MACIO_DEF_SPEED)-2,
#endif
	BPS_TO_TCONST((ZS_MACIO_CLOCK / ZS_MACIO_CLOCK_DIV), ZS_MACIO_DEF_SPEED),
	0,
	ZSWR14_BAUD_ENA,
	ZSWR15_BREAK_IE,
};

static int
zs_macio_probe(device_t dev)
{

	if (strcmp(macio_get_name(dev), "escc") != 0 ||
	    device_get_unit(dev) != 0)
		return (ENXIO);
	return (zs_probe(dev));
}

static int
zs_macio_attach(device_t dev)
{
	struct	zs_macio_softc *sc;
	struct	macio_reg *reg;

	sc = device_get_softc(dev);
	reg = macio_get_regs(dev);

	sc->sc_memres = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->sc_memrid,
	    0, ~1, 1, RF_ACTIVE);
	if (sc->sc_memres == NULL) {
		device_printf(dev, "could not allocate memory\n");
		goto error;
	}
	sc->sc_irqrid1 = 0;
	sc->sc_irqres1 = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->sc_irqrid1,
	    0, ~0, 1, RF_ACTIVE);
	if (sc->sc_irqres1 == NULL) {
		device_printf(dev, "could not allocate interrupt 1\n");
		goto error;
	}
	if (bus_setup_intr(dev, sc->sc_irqres1, INTR_TYPE_TTY | INTR_FAST,
	    zs_intr, sc, &sc->sc_ih1) != 0) {
		device_printf(dev, "could not setup interrupt 1\n");
		goto error;
	}
	sc->sc_irqrid2 = 1;
	sc->sc_irqres2 = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->sc_irqrid2,
	    0, ~0, 1, RF_ACTIVE);
	if (sc->sc_irqres2 == NULL) {
		device_printf(dev, "could not allocate interrupt 2\n");
		goto error;
	}
	if (bus_setup_intr(dev, sc->sc_irqres2, INTR_TYPE_TTY | INTR_FAST,
	    zs_intr, sc, &sc->sc_ih2) != 0) {
		device_printf(dev, "could not setup interrupt 2\n");
		goto error;
	}
	sc->sc_zs.sc_bt = rman_get_bustag(sc->sc_memres);
	sc->sc_zs.sc_bh = rman_get_bushandle(sc->sc_memres);
	return (zs_attach(dev));

error:
	zs_macio_detach(dev);
	return (ENXIO);
}

static int
zs_macio_detach(device_t dev)
{
	struct zs_macio_softc *sc;

	sc = device_get_softc(dev);
	if (sc->sc_irqres2 != NULL) {
		if (sc->sc_ih2 != NULL)
			bus_teardown_intr(dev, sc->sc_irqres2, sc->sc_ih2);
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irqrid2,
		    sc->sc_irqres2);
	}
	if (sc->sc_irqres1 != NULL) {
		if (sc->sc_ih1 != NULL)
			bus_teardown_intr(dev, sc->sc_irqres1, sc->sc_ih1);
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irqrid1,
		    sc->sc_irqres1);
	}
	if (sc->sc_memres != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_memrid,
		    sc->sc_memres);
	return (0);
}

static int
zstty_macio_probe(device_t dev)
{

	if ((device_get_unit(dev) & 1) == 0)
		device_set_desc(dev, "ttya");
	else
		device_set_desc(dev, "ttyb");

	return (zstty_probe(dev));
}

static int
zstty_macio_attach(device_t dev)
{
	struct	zstty_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_parent = device_get_softc(device_get_parent(dev));
	sc->sc_bt = sc->sc_parent->sc_bt;
	sc->sc_brg_clk = ZS_MACIO_CLOCK / ZS_MACIO_CLOCK_DIV;

	switch (device_get_unit(dev) & 1) {
	case 0:
		bus_space_subregion(sc->sc_bt, sc->sc_parent->sc_bh,
		    ZS_MACIO_CHAN_A + ZS_MACIO_CSR, 1, &sc->sc_csr);
		bus_space_subregion(sc->sc_bt, sc->sc_parent->sc_bh,
		    ZS_MACIO_CHAN_A + ZS_MACIO_DATA, 1, &sc->sc_data);
		break;

	case 1:
		bus_space_subregion(sc->sc_bt, sc->sc_parent->sc_bh,
		    ZS_MACIO_CHAN_B + ZS_MACIO_CSR, 1, &sc->sc_csr);
		bus_space_subregion(sc->sc_bt, sc->sc_parent->sc_bh,
		    ZS_MACIO_CHAN_B + ZS_MACIO_DATA, 1, &sc->sc_data);
		break;
	}

	bcopy(zs_macio_init_reg, sc->sc_creg, 16);
	bcopy(zs_macio_init_reg, sc->sc_preg, 16);

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
	device_t	parent;
	phandle_t	chosen, options;
	ihandle_t	stdin, stdout;
	phandle_t	stdinp, stdoutp;
	char		input[32], output[32];
	const char	*desc;

	parent = device_get_parent(dev);
	chosen = OF_finddevice("/chosen");
	options = OF_finddevice("/options");
	if (OF_getprop(chosen, "stdin", &stdin, sizeof(stdin)) == -1 ||
	    OF_getprop(chosen, "stdout", &stdout, sizeof(stdout)) == -1 ||
	    OF_getprop(options, "input-device", input, sizeof(input)) == -1 ||
	    OF_getprop(options, "output-device", output,
	    sizeof(output)) == -1) {
		return (0);
	}
	stdinp = OF_parent(OF_instance_to_package(stdin));
	stdoutp = OF_parent(OF_instance_to_package(stdout));
	desc = device_get_desc(dev);
	if (macio_get_node(parent) == stdinp &&
	    macio_get_node(parent) == stdoutp &&
	    input[3] == desc[3] && output[3] == desc[3]) {
		if (mode != NULL)
			strlcpy(mode, "57600,8,n,1,-", len);

		return (1);
	}

	return (0);
}
