/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

/*
 * This is a pinmux/gpio controller for the IPQ4018/IPQ4019.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <dev/gpio/gpiobusvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/fdt_pinctrl.h>

#include "qcom_tlmm_var.h"
#include "qcom_tlmm_pin.h"
#include "qcom_tlmm_debug.h"

#include "qcom_tlmm_ipq4018_reg.h"
#include "qcom_tlmm_ipq4018_hw.h"

#include "gpio_if.h"

#define	DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | \
	    GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN)

/* 100 GPIO pins, 0..99 */
#define QCOM_TLMM_IPQ4018_GPIO_PINS     100

static const struct qcom_tlmm_gpio_mux gpio_muxes[] = {
	GDEF(0, "jtag_tdi", "smart0", "i2s_rx_bclk"),
	GDEF(1, "jtag_tck", "smart0", "i2s_rx_fsync"),
	GDEF(2, "jtag_tms", "smart0", "i2s_rxd"),
	GDEF(3, "jtag_tdo"),
	GDEF(4, "jtag_rst"),
	GDEF(5, "jtag_trst"),
	GDEF(6, "mdio0", NULL, "wcss0_dbg18", "wcss1_dbg18", NULL,
	    "qdss_tracedata_a"),
	GDEF(7, "mdc", NULL, "wcss0_dbg19", "wcss1_dbg19", NULL,
	    "qdss_tracedata_a"),
	GDEF(8, "blsp_uart1", "wifi0_uart", "wifi1_uart", "smart1", NULL,
	    "wcss0_dbg20", "wcss1_dbg20", NULL, "qdss_tracedata_a"),
	GDEF(9, "blsp_uart1", "wifi0_uart0", "wifi1_uart0", "smart1",
	    "wifi0_uart", NULL, "wcss0_dbg21", "wcss1_dbg21", NULL,
	    "qdss_tracedata_a"),

	GDEF(10, "blsp_uart1", "wifi0_uart0", "wifi1_uart0", "blsp_i2c0",
	    NULL, "wcss0_dbg22", "wcss1_dbg22", NULL, "qdss_tracedata_a"),
	GDEF(11, "blsp_uart1", "wifi0_uart", "wifi1_uart", "blsp_i2c0",
	    NULL, "wcss0_dbg23", "wcss1_dbg23", NULL, "qdss_tracedata_a"),
	GDEF(12, "blsp_spi0", "blsp_i2c1", NULL, "wcss0_dbg24",
	    "wcss1_dbg24"),
	GDEF(13, "blsp_spi0", "blsp_i2c1", NULL, "wcss0_dbg25",
	    "wcss1_dbg25"),
	GDEF(14, "blsp_spi0", NULL, "wcss0_dbg26", "wcss1_dbg26"),
	GDEF(15, "blsp_spi0", NULL, "wcss0_dbg", "wcss1_dbg"),
	GDEF(16, "blsp_uart0", "led0", "smart1", NULL, "wcss0_dbg28",
	    "wcss1_dbg28", NULL, "qdss_tracedata_a"),
	GDEF(17, "blsp_uart0", "led1", "smart1", NULL, "wcss0_dbg29",
	    "wcss1_dbg29", NULL, "qdss_tracedata_a"),
	GDEF(18, "wifi0_uart1", "wifi1_uart1", NULL, "wcss0_dbg30",
	    "wcss1_dbg30"),
	GDEF(19, "wifi0_uart", "wifi1_uart", NULL, "wcss0_dbg31",
	    "wcss1_dbg31"),

	GDEF(20, "blsp_i2c0", "i2s_rx_mclk", NULL, "wcss0_dbg16",
	    "wcss1_dbg16"),
	GDEF(21, "blsp_i2c0", "i2s_rx_bclk", NULL, "wcss0_dbg17",
	    "wcss1_dbg17"),
	GDEF(22, "rgmii0", "i2s_rx_fsync", NULL, "wcss0_dbg18",
	    "wcss1_dbg18"),
	GDEF(23, "sdio0", "rgmii1", "i2s_rxd", NULL, "wcss0_dbg19",
	    "wcss1_dbg19"),
	GDEF(24, "sdio1", "rgmii2", "i2s_tx_mclk", NULL, "wcss0_dbg20",
	    "wcss1_dbg20"),
	GDEF(25, "sdio2", "rgmii3", "i2s_tx_bclk", NULL, "wcss0_dbg21",
	    "wcss1_dbg21"),
	GDEF(26, "sdio3", "rgmii_rx", "i2s_tx_fsync", NULL, "wcss0_dbg22",
	    "wcss1_dbg22"),
	GDEF(27, "sdio_clk", "rgmii_txc", "i2s_tdl", NULL, "wcss0_dbg23",
	    "wcss1_dbg23"),
	GDEF(28, "sdio_cmd", "rgmii0", "i2s_td2", NULL, "wcss0_dbg24",
	    "wcss1_dbg24"),
	GDEF(29, "sdio4", "rgmii1", "i2s_td3", NULL, "wcss0_dbg25",
	    "wcss1_dbg25"),

	GDEF(30, "sdio5", "rgmii2", "audio_pwm0", NULL, "wcss0_dbg26",
	    "wcss1_dbg26"),
	GDEF(31, "sdio6", "rgmii3", "audio_pwm1", NULL, "wcss0_dbg27",
	    "wcss1_dbg27"),
	GDEF(32, "sdio7", "rgmii_rxc", "audio_pwm2", NULL, "wcss0_dbg28",
	    "wcss1_dbg28"),
	GDEF(33, "rgmii_tx", "audio_pwm3", NULL, "wcss0_dbg29",
	    "wcss1_dbg29", NULL, "boot2"),
	GDEF(34, "blsp_i2c1", "i2s_spdif_in", NULL, "wcss0_dbg30",
	    "wcss1_dbg30"),
	GDEF(35, "blsp_i2c1", "i2s_spdif_out", NULL, "wcss0_dbg31",
	    "wcss1_dbg31"),
	GDEF(36, "rmii00", "led2", "led0"),
	GDEF(37, "rmii01", "wifi0_wci", "wifi1_wci", "led1", NULL, NULL,
	    "wcss0_dbg16", "wcss1_dbg16", NULL, "qdss_tracedata_a", "boot4"),
	GDEF(38, "rmii0_tx", "led2", NULL, NULL, "wcss0_dbg17",
	    "wcss1_dbg17", NULL, "qdss_tracedata_a", "boot5"),
	GDEF(39, "rmii0_rx", "pcie_clk1", "led3", NULL, NULL, "wcss0_dbg18",
	    "wcss1_dbg18", NULL, NULL, "qdss_tracedata_a"),

	GDEF(40, "rmii0_refclk", "wifi0_rfsilent0", "wifi1_rfsilent0",
	    "smart2", "led4", NULL, NULL, "wcss0_dbg19", "wcss1_dbg19", NULL,
	    NULL, "qdss_tracedata_a"),
	GDEF(41, "rmii00", "wifi0_cal", "wifi1_cal", "smart2", NULL, NULL,
	    "wcss0_dbg20", "wcss1_dbg20", NULL, NULL, "qdss_tracedata_a"),
	GDEF(42, "rmii01", "wifi_wci0", NULL, NULL, "wcss0_dbg21",
	    "wcss1_dbg21", NULL, NULL, "qdss_tracedata_a"),
	GDEF(43, "rmii0_dv", "wifi_wci1", NULL, NULL, "wcss0_dbg22",
	    "wcss1_dbg22", NULL, NULL, "qdss_tracedata_a"),
	GDEF(44, "rmii1_refclk", "blsp_spi1", "smart0", "led5", NULL, NULL,
	    "wcss0_dbg23", "wcss1_dbg23"),
	GDEF(45, "rmii10", "blsp_spi1", "smart0", "led6", NULL, NULL,
	    "wcss0_dbg24", "wcss1_dbg24"),
	GDEF(46, "rmii11", "blsp_spi1", "smart0", "led7", NULL, NULL,
	    "wcss0_dbg25", "wcss1_dbg25"),
	GDEF(47, "rmii1_dv", "blsp_spi1", "smart0", "led8", NULL, NULL,
	    "wcss0_dbg26", "wcss1_dbg26"),
	GDEF(48, "rmii1_tx", "aud_pin", "smart2", "led9", NULL, NULL,
	    "wcss0_dbg27", "wcss1_dbg27"),
	GDEF(49, "rmii1_rx", "aud_pin", "smart2", "led10", NULL, NULL,
	    "wcss0_dbg28", "wcss1_dbg28"),

	GDEF(50, "rmii10", "aud_pin", "wifi0_rfsilent1", "wifi1_rfsilent1",
	    "led11", NULL, NULL, "wcss0_dbg29", "wcss1_dbg29"),
	GDEF(51, "rmii11", "aud_pin", "wifi0_cal", "wifi1_cal", NULL, NULL,
	    "wcss0_dbg30", "wcss1_dbg30", NULL, "boot7"),
	GDEF(52, "qpic_pad", "mdc", "pcie_clk", "i2s_tx_mclk", NULL, NULL,
	    "wcss0_dbg31", "tm_clk0", "wifi00", "wifi10"),
	GDEF(53, "qpic_pad", "mdio1", "i2s_tx_bclk", "prng_rsoc", "dbg_out",
	    "tm0", "wifi01", "wifi11"),
	GDEF(54, "qpic_pad", "blsp_spi0", "i2s_tdl", "atest_char3", "pmu0",
	    NULL, NULL, "boot8", "tm1"),
	GDEF(55, "qpic_pad", "blsp_spi0", "i2s_td2", "atest_char2", "pmu1",
	    NULL, NULL, "boot9", "tm2"),
	GDEF(56, "qpic_pad", "blsp_spi0", "i2s_td3", "atest_char1", NULL,
	    "tm_ack", "wifi03", "wifi13"),
	GDEF(57, "qpic_pad4", "blsp_spi0", "i2s_tx_fsync", "atest_char0",
	    NULL, "tm3", "wifi02", "wifi12"),
	GDEF(58, "qpic_pad5", "led2", "blsp_i2c0", "smart3", "smart1",
	    "i2s_rx_mclk", NULL, "wcss0_dbg14", "tm4", "wifi04", "wifi14"),
	GDEF(59, "qpic_pad6", "blsp_i2c0", "smart3", "smart1", "i2c_spdif_in",
	    NULL, NULL, "wcss0_dbg15", "qdss_tracectl_a", "boot18", "tm5" ),

	GDEF(60, "qpic_pad7", "blsp_uart0", "smart1", "smart3", "led0",
	    "i2s_tx_bclk", "i2s_rx_bclk", "atest_char", NULL, "wcss0_dbg4",
	    "qdss_traceclk_a", "boot19", "tm6" ),
	GDEF(61, "qpic_pad", "blsp_uart0", "smart1", "smart3", "led1",
	    "i2s_tx_fsync", "i2s_rx_fsync", NULL, NULL, "wcss0_dbg5",
	    "qdss_cti_trig_out_a0", "boot14", "tm7"),
	GDEF(62, "qpic_pad", "chip_rst", "wifi0_uart", "wifi1_uart",
	    "i2s_spdif_out", NULL, NULL, "wcss0_dbg6", "qdss_cti_trig_out_b0",
	    "boot11", "tm8"),
	GDEF(63, "qpic_pad", "wifi0_uart1", "wifi1_uart1", "wifi1_uart",
	    "i2s_tdl", "i2s_rxd", "i2s_spdif_out", "i2s_spdif_in", NULL,
	    "wcss0_dbg7", "wcss1_dbg7", "boot20", "tm9"),
	GDEF(64, "qpic_pad1", "audio_pwm0", NULL, "wcss0_dbg8", "wcss1_dbg8"),
	GDEF(65, "qpic_pad2", "audio_pwm1", NULL, "wcss0_dbg9",
	    "wcss1_dbg9" ),
	GDEF(66, "qpic_pad3", "audio_pwm2", NULL, "wcss0_dbg10",
	    "wcss1_dbg10"),
	GDEF(67, "qpic_pad0", "audio_pwm3", NULL, "wcss0_dbg11",
	   "wcss1_dbg11"),
	GDEF(68, "qpic_pad8", NULL, "wcss0_dbg12", "wcss1_dbg12"),
	GDEF(69, "qpic_pad", NULL, "wcss0_dbg"),

	GDEF(70),
	GDEF(71),
	GDEF(72),
	GDEF(73),
	GDEF(74),
	GDEF(75),
	GDEF(76),
	GDEF(77),
	GDEF(78),
	GDEF(79),

	GDEF(80),
	GDEF(81),
	GDEF(82),
	GDEF(83),
	GDEF(84),
	GDEF(85),
	GDEF(86),
	GDEF(87),
	GDEF(88),
	GDEF(89),

	GDEF(90),
	GDEF(91),
	GDEF(92),
	GDEF(93),
	GDEF(94),
	GDEF(95),
	GDEF(96),
	GDEF(97),
	GDEF(98, "wifi034", "wifi134"),
	GDEF(99),

	GDEF(-1),
};

static int
qcom_tlmm_ipq4018_probe(device_t dev)
{

	if (! ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "qcom,ipq4019-pinctrl") == 0)
		return (ENXIO);

	device_set_desc(dev,
	    "Qualcomm Atheross TLMM IPQ4018/IPQ4019 GPIO/Pinmux driver");
	return (0);
}

static int
qcom_tlmm_ipq4018_detach(device_t dev)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);

	KASSERT(mtx_initialized(&sc->gpio_mtx), ("gpio mutex not initialized"));

	gpiobus_detach_bus(dev);
	if (sc->gpio_ih)
		bus_teardown_intr(dev, sc->gpio_irq_res, sc->gpio_ih);
	if (sc->gpio_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, sc->gpio_irq_rid,
		    sc->gpio_irq_res);
	if (sc->gpio_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->gpio_mem_rid,
		    sc->gpio_mem_res);
	if (sc->gpio_pins)
		free(sc->gpio_pins, M_DEVBUF);
	mtx_destroy(&sc->gpio_mtx);

	return(0);
}



static int
qcom_tlmm_ipq4018_attach(device_t dev)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	int i;

	KASSERT((device_get_unit(dev) == 0),
	    ("qcom_tlmm_ipq4018: Only one gpio module supported"));

	mtx_init(&sc->gpio_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	/* Map control/status registers. */
	sc->gpio_mem_rid = 0;
	sc->gpio_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->gpio_mem_rid, RF_ACTIVE);

	if (sc->gpio_mem_res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		qcom_tlmm_ipq4018_detach(dev);
		return (ENXIO);
	}

	if ((sc->gpio_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->gpio_irq_rid, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(dev, "unable to allocate IRQ resource\n");
		qcom_tlmm_ipq4018_detach(dev);
		return (ENXIO);
	}

	if ((bus_setup_intr(dev, sc->gpio_irq_res, INTR_TYPE_MISC,
	    qcom_tlmm_filter, qcom_tlmm_intr, sc, &sc->gpio_ih))) {
		device_printf(dev,
		    "WARNING: unable to register interrupt handler\n");
		qcom_tlmm_ipq4018_detach(dev);
		return (ENXIO);
	}

	sc->dev = dev;
	sc->gpio_npins = QCOM_TLMM_IPQ4018_GPIO_PINS;
	sc->gpio_muxes = &gpio_muxes[0];
	sc->sc_debug = 0;

	qcom_tlmm_debug_sysctl_attach(sc);

	/* Allocate local pin state for all of our pins */
	sc->gpio_pins = malloc(sizeof(*sc->gpio_pins) * sc->gpio_npins,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	/* Note: direct map between gpio pin and gpio_pin[] entry */
	for (i = 0; i < sc->gpio_npins; i++) {
		snprintf(sc->gpio_pins[i].gp_name, GPIOMAXNAME,
		    "gpio%d", i);
		sc->gpio_pins[i].gp_pin = i;
		sc->gpio_pins[i].gp_caps = DEFAULT_CAPS;
		(void) qcom_tlmm_pin_getflags(dev, i,
		    &sc->gpio_pins[i].gp_flags);
	}

	fdt_pinctrl_register(dev, NULL);
	fdt_pinctrl_configure_by_name(dev, "default");

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "%s: failed to attach bus\n", __func__);
		qcom_tlmm_ipq4018_detach(dev);
		return (ENXIO);
	}

	return (0);
}

static device_method_t qcom_tlmm_ipq4018_methods[] = {
	/* Driver */
	DEVMETHOD(device_probe, qcom_tlmm_ipq4018_probe),
	DEVMETHOD(device_attach, qcom_tlmm_ipq4018_attach),
	DEVMETHOD(device_detach, qcom_tlmm_ipq4018_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus, qcom_tlmm_get_bus),
	DEVMETHOD(gpio_pin_max, qcom_tlmm_pin_max),
	DEVMETHOD(gpio_pin_getname, qcom_tlmm_pin_getname),
	DEVMETHOD(gpio_pin_getflags, qcom_tlmm_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps, qcom_tlmm_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags, qcom_tlmm_pin_setflags),
	DEVMETHOD(gpio_pin_get, qcom_tlmm_pin_get),
	DEVMETHOD(gpio_pin_set, qcom_tlmm_pin_set),
	DEVMETHOD(gpio_pin_toggle, qcom_tlmm_pin_toggle),

	/* OFW */
	DEVMETHOD(ofw_bus_get_node, qcom_tlmm_pin_get_node),

	/* fdt_pinctrl interface */
	DEVMETHOD(fdt_pinctrl_configure, qcom_tlmm_pinctrl_configure),

	{0, 0},
};

static driver_t qcom_tlmm_ipq4018_driver = {
	"gpio",
	qcom_tlmm_ipq4018_methods,
	sizeof(struct qcom_tlmm_softc),
};

EARLY_DRIVER_MODULE(qcom_tlmm_ipq4018, simplebus, qcom_tlmm_ipq4018_driver,
    NULL, NULL, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
EARLY_DRIVER_MODULE(qcom_tlmm_ipq4018, ofwbus, qcom_tlmm_ipq4018_driver,
    NULL, NULL, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
MODULE_VERSION(qcom_tlmm_ipq4018, 1);
