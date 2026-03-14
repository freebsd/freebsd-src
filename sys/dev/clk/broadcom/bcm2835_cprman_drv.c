/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Perdixky <3293789706@qq.com>
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <dev/clk/broadcom/bcm_cprman.h>
#include <dev/fdt/simplebus.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <contrib/device-tree/include/dt-bindings/clock/bcm2835.h>

/*
 * BCM2835 peripheral clock SRC field (CTL[3:0]) maps to these parents
 * in order.  Only "osc" is guaranteed to be registered as a fixed clock
 * from the DTB; the PLL names will be resolved once PLL nodes are added.
 */
static const char *bcm2835_peri_parents[] = {
	"gnd",	      /* 0 */
	"osc",	      /* 1: 19.2 MHz crystal */
	"testdebug0", /* 2 */
	"testdebug1", /* 3 */
	"plla_per",   /* 4 */
	"pllc_per",   /* 5 */
	"plld_per",   /* 6 */
	"hdmi_aux",   /* 7 */
};

/*
 * BCM2835 peripheral clock register layout (both CTL and DIV):
 *   CTL: [31:24] PASSWD  [10:9] MASH  [7] BUSY  [4] ENAB  [3:0] SRC
 *   DIV: [31:24] PASSWD  [23:12] DIVI  [11:0] DIVF
 */
#define BCM2835_PERIPH_CLK(_name, _id, _ctl, _div) \
	{							\
		.clkdef = {						\
			.name		= (_name),			\
			.id		= (_id),			\
			.parent_names	= bcm2835_peri_parents,		\
			.parent_cnt	= nitems(bcm2835_peri_parents),	\
			.flags		= 0,				\
		},							\
		.ctl_offset	= (_ctl),				\
		.div_offset	= (_div),				\
		.passwd_shift	= 24,					\
		.passwd_width	= 8,					\
		.passwd		= 0x5a,					\
		.mash_shift	= 9,					\
		.mash_width	= 2,					\
		.busy_shift	= 7,					\
		.enable_shift	= 4,					\
		.src_shift	= 0,					\
		.src_width	= 4,					\
		.div_int_shift	= 12,					\
		.div_int_width	= 12,					\
		.div_frac_shift	= 0,					\
		.div_frac_width	= 12,					\
	}

static const struct bcm_clk_periph_def bcm2835_clkdefs[] = {
	BCM2835_PERIPH_CLK("gp0", BCM2835_CLOCK_GP0, 0x070, 0x074),
	BCM2835_PERIPH_CLK("gp1", BCM2835_CLOCK_GP1, 0x078, 0x07c),
	BCM2835_PERIPH_CLK("gp2", BCM2835_CLOCK_GP2, 0x080, 0x084),
	BCM2835_PERIPH_CLK("pwm", BCM2835_CLOCK_PWM, 0x0a0, 0x0a4),
	BCM2835_PERIPH_CLK("pcm", BCM2835_CLOCK_PCM, 0x098, 0x09c),
};

/*
 * Sanity-check at compile time: the chip definition array must match
 * the fixed-size clks[] array in bcm_cprman_softc.
 */
CTASSERT(nitems(bcm2835_clkdefs) ==
	nitems(((struct bcm_cprman_softc *)NULL)->clks));

static struct ofw_compat_data compat_data[] = {
	{ "brcm,bcm2835-cprman", 1 },
	{ "brcm,bcm2711-cprman", 1 },
	{ NULL, 0 }
};

static int
bcm2835_cprman_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "BCM2835 CPRMAN Clock Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
bcm2835_cprman_attach(device_t dev)
{
	struct bcm_cprman_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * Copy the BCM2835-specific clock definitions into softc before
	 * calling the shared bcm_cprman_attach(), which iterates over
	 * sc->clks[] to register them with the clock domain.
	 */
	memcpy(sc->clks, bcm2835_clkdefs, sizeof(sc->clks));

	return (bcm_cprman_attach(dev));
}

static device_method_t bcm2835_cprman_methods[] = {
	DEVMETHOD(device_probe,		bcm2835_cprman_probe),
	DEVMETHOD(device_attach,	bcm2835_cprman_attach),
	DEVMETHOD_END
};

/*
 * DEFINE_CLASS_1: inherit bcm_cprman_driver's clkdev methods
 * (clkdev_write_4 / read_4 / modify_4 / device_lock / device_unlock).
 * This driver only adds probe + attach on top.
 */
DEFINE_CLASS_1(bcm2835_cprman, bcm2835_cprman_driver, bcm2835_cprman_methods,
    sizeof(struct bcm_cprman_softc), bcm_cprman_driver);

/*
 * Clock drivers must be initialized early (BUS_PASS_BUS) so that
 * peripheral drivers can find their clocks during their own attach.
 */
EARLY_DRIVER_MODULE(bcm2835_cprman, simplebus, bcm2835_cprman_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(bcm2835_cprman, 1);
