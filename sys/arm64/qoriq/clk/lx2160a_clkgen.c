/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
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

/*
 * Clock driver for LX2160A SoC.
 */
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk_fixed.h>

#include <arm64/qoriq/clk/qoriq_clkgen.h>

#define	PLL(_id1, _id2, cname, o, d)					\
{									\
	.clkdef.id = QORIQ_CLK_ID(_id1, _id2),				\
	.clkdef.name = cname,						\
	.clkdef.flags = 0,						\
	.offset = o,							\
	.shift  = 1,							\
	.mask = 0xFE,							\
	.dividers = d,							\
	.flags = QORIQ_CLK_PLL_HAS_KILL_BIT,				\
}

static const uint8_t plt_divs[] =
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0};
static const uint8_t cga_divs[] =  {2, 4, 0};
static const uint8_t cgb_divs[] =  {2, 3, 4, 0};

static struct qoriq_clk_pll_def pltfrm_pll =
    PLL(QORIQ_TYPE_PLATFORM_PLL, 0, "platform_pll", 0x60080, plt_divs);
static struct qoriq_clk_pll_def cga_pll1 =
    PLL(QORIQ_TYPE_INTERNAL, 0, "cga_pll1", 0x80, cga_divs);
static struct qoriq_clk_pll_def cga_pll2 =
    PLL(QORIQ_TYPE_INTERNAL, 0, "cga_pll2", 0xA0, cga_divs);
static struct qoriq_clk_pll_def cgb_pll1 =
    PLL(QORIQ_TYPE_INTERNAL, 0, "cgb_pll1", 0x10080, cgb_divs);
static struct qoriq_clk_pll_def cgb_pll2 =
    PLL(QORIQ_TYPE_INTERNAL, 0, "cgb_pll2", 0x100A0, cgb_divs);

static struct qoriq_clk_pll_def *cg_plls[] = {
	&cga_pll1,
	&cga_pll2,
	&cgb_pll1,
	&cgb_pll2,
};

#if 0
static struct qoriq_clk_pll_def *cg_plls[] = {
	&(struct qoriq_clk_pll_def)
	    {PLL(QORIQ_TYPE_INTERNAL, 0, "cga_pll1", 0x80, cg_divs)},
	&(struct qoriq_clk_pll_def)
	    {PLL(QORIQ_TYPE_INTERNAL, 0, "cga_pll2", 0xA0, cg_divs)},
	&(struct qoriq_clk_pll_def)
	    {PLL(QORIQ_TYPE_INTERNAL, 0, "cgb_pll1", 0x10080, cg_divs)},
	&(struct qoriq_clk_pll_def)
	    {PLL(QORIQ_TYPE_INTERNAL, 0, "cgb_pll2", 0x100A0, cg_divs)},
};
#endif

static const char *cmuxa_plist[] = {
	"cga_pll1",
	"cga_pll1_div2",
	"cga_pll1_div4",
	NULL,
	"cga_pll2",
	"cga_pll2_div2",
	"cga_pll2_div4",
};

static const char *cmuxb_plist[] = {
	"cgb_pll1",
	"cgb_pll1_div2",
	"cgb_pll1_div4",
	NULL,
	"cgb_pll2",
	"cgb_pll2_div2",
	"cgb_pll2_div4",
};

#define	MUX(_id1, _id2, cname, plist, o)				\
{									\
	.clkdef.id = QORIQ_CLK_ID(_id1, _id2),				\
	.clkdef.name = cname,						\
	.clkdef.parent_names = plist,					\
	.clkdef.parent_cnt = nitems(plist),				\
	.clkdef.flags = 0,						\
	.offset = o,							\
	.width  = 4,							\
	.shift = 27,							\
	.mux_flags = 0,							\
}
static struct clk_mux_def cmux0 =
   MUX(QORIQ_TYPE_CMUX, 0, "cg-cmux0", cmuxa_plist, 0x70000);
static struct clk_mux_def cmux1 =
   MUX(QORIQ_TYPE_CMUX, 1, "cg-cmux1", cmuxa_plist, 0x70020);
static struct clk_mux_def cmux2 =
   MUX(QORIQ_TYPE_CMUX, 2, "cg-cmux2", cmuxa_plist, 0x70040);
static struct clk_mux_def cmux3 =
   MUX(QORIQ_TYPE_CMUX, 3, "cg-cmux3", cmuxa_plist, 0x70060);
static struct clk_mux_def cmux4 =
   MUX(QORIQ_TYPE_CMUX, 4, "cg-cmux4", cmuxb_plist, 0x70080);
static struct clk_mux_def cmux5 =
   MUX(QORIQ_TYPE_CMUX, 5, "cg-cmux5", cmuxb_plist, 0x700A0);
static struct clk_mux_def cmux6 =
   MUX(QORIQ_TYPE_CMUX, 6, "cg-cmux6", cmuxb_plist,  0x700C0);
static struct clk_mux_def cmux7 =
   MUX(QORIQ_TYPE_CMUX, 7, "cg-cmux7", cmuxb_plist,  0x700E0);

static struct clk_mux_def *mux_nodes[] = {
	&cmux0,
	&cmux1,
	&cmux2,
	&cmux3,
	&cmux4,
	&cmux5,
	&cmux6,
	&cmux7,
};

static int
lx2160a_clkgen_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if(!ofw_bus_is_compatible(dev, "fsl,lx2160a-clockgen"))
		return (ENXIO);

	device_set_desc(dev, "LX2160A clockgen");
	return (BUS_PROBE_DEFAULT);
}

static int
lx2160a_clkgen_attach(device_t dev)
{
	struct qoriq_clkgen_softc *sc;
	int rv;

	sc = device_get_softc(dev);

	sc->pltfrm_pll_def = &pltfrm_pll;
	sc->cga_pll = cg_plls;
	sc->cga_pll_num = nitems(cg_plls);
	sc->mux = mux_nodes;
	sc->mux_num = nitems(mux_nodes);
	sc->flags = QORIQ_LITTLE_ENDIAN;

	rv = qoriq_clkgen_attach(dev);

	printf(" %s: offset: 0x%08X, val: 0x%08X\n", __func__, 0x00080, bus_read_4(sc->res, 0x00080));
	printf(" %s: offset: 0x%08X, val: 0x%08X\n", __func__, 0x000A0, bus_read_4(sc->res, 0x000A0));
	printf(" %s: offset: 0x%08X, val: 0x%08X\n", __func__, 0x10080, bus_read_4(sc->res, 0x10080));
	printf(" %s: offset: 0x%08X, val: 0x%08X\n", __func__, 0x100A0, bus_read_4(sc->res, 0x100A0));
	printf(" %s: offset: 0x%08X, val: 0x%08X\n", __func__, 0x60080, bus_read_4(sc->res, 0x60080));
	printf(" %s: offset: 0x%08X, val: 0x%08X\n", __func__, 0x600A0, bus_read_4(sc->res, 0x600A0));
	return (rv);
}

static device_method_t lx2160a_clkgen_methods[] = {
	DEVMETHOD(device_probe,		lx2160a_clkgen_probe),
	DEVMETHOD(device_attach,	lx2160a_clkgen_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(lx2160a_clkgen, lx2160a_clkgen_driver, lx2160a_clkgen_methods,
    sizeof(struct qoriq_clkgen_softc), qoriq_clkgen_driver);
EARLY_DRIVER_MODULE(lx2160a_clkgen, simplebus, lx2160a_clkgen_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
