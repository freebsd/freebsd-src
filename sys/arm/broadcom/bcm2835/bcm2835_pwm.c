/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017 Poul-Henning Kamp <phk@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/broadcom/bcm2835/bcm2835_gpio.h>

static struct ofw_compat_data compat_data[] = {
	{"broadcom,bcm2835-pwm",	1},
	{"brcm,bcm2835-pwm",		1},
	{NULL,				0}
};

struct bcm_pwm_softc {
	device_t		sc_dev;

	struct resource *	sc_mem_res;
	bus_space_tag_t		sc_m_bst;
	bus_space_handle_t	sc_m_bsh;

	struct resource *	sc_clk_res;
	bus_space_tag_t		sc_c_bst;
	bus_space_handle_t	sc_c_bsh;

	uint32_t		freq;
	uint32_t		period;
	uint32_t		ratio;
	uint32_t		mode;

};

#define BCM_PWM_MEM_WRITE(_sc, _off, _val)		\
    bus_space_write_4(_sc->sc_m_bst, _sc->sc_m_bsh, _off, _val)
#define BCM_PWM_MEM_READ(_sc, _off)			\
    bus_space_read_4(_sc->sc_m_bst, _sc->sc_m_bsh, _off)
#define BCM_PWM_CLK_WRITE(_sc, _off, _val)		\
    bus_space_write_4(_sc->sc_c_bst, _sc->sc_c_bsh, _off, _val)
#define BCM_PWM_CLK_READ(_sc, _off)			\
    bus_space_read_4(_sc->sc_c_bst, _sc->sc_c_bsh, _off)

#define W_CTL(_sc, _val) BCM_PWM_MEM_WRITE(_sc, 0x00, _val)
#define R_CTL(_sc) BCM_PWM_MEM_READ(_sc, 0x00)
#define W_STA(_sc, _val) BCM_PWM_MEM_WRITE(_sc, 0x04, _val)
#define R_STA(_sc) BCM_PWM_MEM_READ(_sc, 0x04)
#define W_RNG(_sc, _val) BCM_PWM_MEM_WRITE(_sc, 0x10, _val)
#define R_RNG(_sc) BCM_PWM_MEM_READ(_sc, 0x10)
#define W_DAT(_sc, _val) BCM_PWM_MEM_WRITE(_sc, 0x14, _val)
#define R_DAT(_sc) BCM_PWM_MEM_READ(_sc, 0x14)

#define W_CMCLK(_sc, _val) BCM_PWM_CLK_WRITE(_sc, 0x00, 0x5a000000 | (_val))
#define R_CMCLK(_sc) BCM_PWM_CLK_READ(_sc, 0x00)
#define W_CMDIV(_sc, _val) BCM_PWM_CLK_WRITE(_sc, 0x04, 0x5a000000 | (_val))
#define R_CMDIV(_s) BCM_PWM_CLK_READ(_sc, 0x04)

static int
bcm_pwm_reconf(struct bcm_pwm_softc *sc)
{
	int i;
	uint32_t u;
	device_t gpio;

	/* Disable PWM */
	W_CTL(sc, 0);

	/* Stop PWM clock */
	W_CMCLK(sc, 6);
	for (i = 0; i < 10; i++) {
		u = R_CMCLK(sc);
		if (!(u&0x80))
			break;
		DELAY(1000);
	}
	if (u&0x80) {
		device_printf(sc->sc_dev, "Failed to stop clock\n");
		return(EIO);
	}

	if (sc->mode == 0) {
		// XXX: GPIO cfg ?
		return (0);
	}

	/* Ask GPIO0 to set ALT0 for pin 12 */
	gpio = devclass_get_device(devclass_find("gpio"), 0);
	if (!gpio) {
		device_printf(sc->sc_dev, "cannot find gpio0\n");
		return (ENXIO);
	}
	bcm_gpio_set_alternate(gpio, 12, BCM_GPIO_ALT0);

	/* Configure divider */
	u = 500000000/sc->freq;
	if (u < 4) {
		device_printf(sc->sc_dev, "Freq too high (max 125MHz)\n");
		return(EINVAL);
	}
	if (u > 0xfff) {
		device_printf(sc->sc_dev, "Freq too low (min 123Hz)\n");
		return(EINVAL);
	}
	sc->freq = 500000000/u;
	W_CMDIV(sc, u << 12);

	/* Start PWM clock */
	W_CMCLK(sc, 0x16);
	for (i = 0; i < 10; i++) {
		u = R_CMCLK(sc);
		if ((u&0x80))
			break;
		DELAY(1000);
	}
	if (!(u&0x80)) {
		device_printf(sc->sc_dev, "Failed to start clock\n");
		return(EIO);
	}

	/* Config PWM */
	W_RNG(sc, sc->period);
	if (sc->ratio > sc->period)
		sc->ratio = sc->period;
	W_DAT(sc, sc->ratio);

	/* Start PWM */
	if (sc->mode == 1)
		W_CTL(sc, 0x81);
	else
		W_CTL(sc, 0x1);

	return (0);
}

static int
bcm_pwm_pwm_freq_proc(SYSCTL_HANDLER_ARGS)
{
	struct bcm_pwm_softc *sc;
	uint32_t r;
	int error;

	sc = (struct bcm_pwm_softc *)arg1;
	if (sc->mode == 1)
		r = sc->freq / sc->period;
	else
		r = 0;
	error = sysctl_handle_int(oidp, &r, sizeof(r), req);
	return (error);
}


static int
bcm_pwm_mode_proc(SYSCTL_HANDLER_ARGS)
{
	struct bcm_pwm_softc *sc;
	uint32_t r;
	int error;

	sc = (struct bcm_pwm_softc *)arg1;
	r = sc->mode;
	error = sysctl_handle_int(oidp, &r, sizeof(r), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (r > 2)
		return (EINVAL);
	sc->mode = r;
	return (bcm_pwm_reconf(sc));
}

static int
bcm_pwm_freq_proc(SYSCTL_HANDLER_ARGS)
{
	struct bcm_pwm_softc *sc;
	uint32_t r;
	int error;

	sc = (struct bcm_pwm_softc *)arg1;
	r = sc->freq;
	error = sysctl_handle_int(oidp, &r, sizeof(r), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (r > 125000000)
		return (EINVAL);
	sc->freq = r;
	return (bcm_pwm_reconf(sc));
}

static int
bcm_pwm_period_proc(SYSCTL_HANDLER_ARGS)
{
	struct bcm_pwm_softc *sc;
	int error;

	sc = (struct bcm_pwm_softc *)arg1;
	error = sysctl_handle_int(oidp, &sc->period, sizeof(sc->period), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	return (bcm_pwm_reconf(sc));
}

static int
bcm_pwm_ratio_proc(SYSCTL_HANDLER_ARGS)
{
	struct bcm_pwm_softc *sc;
	uint32_t r;
	int error;

	sc = (struct bcm_pwm_softc *)arg1;
	r = sc->ratio;
	error = sysctl_handle_int(oidp, &r, sizeof(r), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (r > sc->period)			// XXX >= ?
		return (EINVAL);
	sc->ratio = r;
	BCM_PWM_MEM_WRITE(sc, 0x14, sc->ratio);
	return (0);
}

static int
bcm_pwm_reg_proc(SYSCTL_HANDLER_ARGS)
{
	struct bcm_pwm_softc *sc;
	uint32_t reg;
	int error;

	sc = (struct bcm_pwm_softc *)arg1;
	if (arg2 & 0x100)
		reg = BCM_PWM_CLK_READ(sc, arg2 & 0xff);
	else
		reg = BCM_PWM_MEM_READ(sc, arg2 & 0xff);

	error = sysctl_handle_int(oidp, &reg, sizeof(reg), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (arg2 & 0x100)
		BCM_PWM_CLK_WRITE(sc, arg2 & 0xff, reg);
	else
		BCM_PWM_MEM_WRITE(sc, arg2, reg);
	return (0);
}

static void
bcm_pwm_sysctl_init(struct bcm_pwm_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;

	/*
	 * Add system sysctl tree/handlers.
	 */
	ctx = device_get_sysctl_ctx(sc->sc_dev);
	tree_node = device_get_sysctl_tree(sc->sc_dev);
	tree = SYSCTL_CHILDREN(tree_node);
	if (bootverbose) {
#define RR(x,y)							\
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, y,			\
	    CTLFLAG_RW | CTLTYPE_UINT, sc, 0x##x,		\
	    bcm_pwm_reg_proc, "IU", "Register 0x" #x " " y);

		RR(100, "PWMCTL")
		RR(104, "PWMDIV")
		RR(24, "DAT2")
		RR(20, "RNG2")
		RR(18, "FIF1")
		RR(14, "DAT1")
		RR(10, "RNG1")
		RR(08, "DMAC")
		RR(04, "STA")
		RR(00, "CTL")
#undef RR
	}

	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "pwm_freq",
	    CTLFLAG_RD | CTLTYPE_UINT, sc, 0,
	    bcm_pwm_pwm_freq_proc, "IU", "PWM frequency (Hz)");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "period",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, 0,
	    bcm_pwm_period_proc, "IU", "PWM period (#clocks)");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "ratio",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, 0,
	    bcm_pwm_ratio_proc, "IU", "PWM ratio (0...period)");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "freq",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, 0,
	    bcm_pwm_freq_proc, "IU", "PWM clock (Hz)");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "mode",
	    CTLFLAG_RW | CTLTYPE_UINT, sc, 0,
	    bcm_pwm_mode_proc, "IU", "PWM mode (0=off, 1=pwm, 2=dither)");
}

static int
bcm_pwm_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "BCM2708/2835 PWM controller");

	return (BUS_PROBE_DEFAULT);
}

static int
bcm_pwm_attach(device_t dev)
{
	struct bcm_pwm_softc *sc;
	int rid;

	if (device_get_unit(dev) != 0) {
		device_printf(dev, "only one PWM controller supported\n");
		return (ENXIO);
	}

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->sc_m_bst = rman_get_bustag(sc->sc_mem_res);
	sc->sc_m_bsh = rman_get_bushandle(sc->sc_mem_res);

	rid = 1;
	sc->sc_clk_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_clk_res) {
		device_printf(dev, "cannot allocate clock window\n");
		return (ENXIO);
	}
	sc->sc_c_bst = rman_get_bustag(sc->sc_clk_res);
	sc->sc_c_bsh = rman_get_bushandle(sc->sc_clk_res);

	/* Add sysctl nodes. */
	bcm_pwm_sysctl_init(sc);

	sc->freq = 125000000;
	sc->period = 10000;
	sc->ratio = 2500;


	return (bus_generic_attach(dev));
}

static int
bcm_pwm_detach(device_t dev)
{
	struct bcm_pwm_softc *sc;

	bus_generic_detach(dev);

	sc = device_get_softc(dev);
	sc->mode = 0;	
	(void)bcm_pwm_reconf(sc);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
	if (sc->sc_clk_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_clk_res);

	return (0);
}

static phandle_t
bcm_pwm_get_node(device_t bus, device_t dev)
{

	/* We only have one child, the SPI bus, which needs our own node. */
	return (ofw_bus_get_node(bus));
}

static device_method_t bcm_pwm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bcm_pwm_probe),
	DEVMETHOD(device_attach,	bcm_pwm_attach),
	DEVMETHOD(device_detach,	bcm_pwm_detach),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	bcm_pwm_get_node),

	DEVMETHOD_END
};

static devclass_t bcm_pwm_devclass;

static driver_t bcm_pwm_driver = {
	"pwm",
	bcm_pwm_methods,
	sizeof(struct bcm_pwm_softc),
};

DRIVER_MODULE(bcm2835_pwm, simplebus, bcm_pwm_driver, bcm_pwm_devclass, 0, 0);
