/*-
 * Copyright (c) 2013 Ian Lepore
 * All rights reserved.
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

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <machine/bus.h>

#include <arm/ti/ti_prcm.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_dev_ns8250.h>

#include "uart_if.h"

/*
 * High-level UART interface.
 */
struct ti8250_softc {
	struct ns8250_softc ns8250_base;
	/*uint32_t	mystuff;*/
};

#define	MDR1_REG		8
#define	  MDR1_MODE_UART	0
#define	  MDR1_MODE_DISABLE	7
#define	SYSCC_REG		15
#define	  SYSCC_SOFTRESET	(1 << 1)
#define	SYSS_REG		16
#define	  SYSS_STATUS_RESETDONE	(1 << 0)

static int
ti8250_bus_probe(struct uart_softc *sc)
{
	int status;
	int devid;
	clk_ident_t clkid;
	pcell_t prop;
	phandle_t node;

	/*
	 * Get the device id from FDT.  If it's not there we can't turn on the
	 * right clocks, so bail, unless we're doing unit 0.  We assume that's
	 * the serial console, whose clock isn't controllable anyway, and we
	 * sure don't want to break the console because of a config error.
	 */
	node = ofw_bus_get_node(sc->sc_dev);
	if ((OF_getprop(node, "uart-device-id", &prop, sizeof(prop))) <= 0) {
		device_printf(sc->sc_dev, 
		    "missing uart-device-id attribute in FDT\n");
		if (device_get_unit(sc->sc_dev) != 0)
			return (ENXIO);
		devid = 0;
	} else
		devid = fdt32_to_cpu(prop);

	/* Enable clocks for this device.  We can't continue if that fails.  */
	clkid = UART0_CLK + devid;
	if ((status = ti_prcm_clk_enable(clkid)) != 0)
		return (status);

	/*
	 * Set the hardware to disabled mode, do a full device reset, then set
	 * it to uart mode.  Most devices will be reset-and-disabled already,
	 * but you never know what a bootloader might have done.
	 */
	uart_setreg(&sc->sc_bas, MDR1_REG, MDR1_MODE_DISABLE);
	uart_setreg(&sc->sc_bas, SYSCC_REG, SYSCC_SOFTRESET);
	while (uart_getreg(&sc->sc_bas, SYSS_REG) & SYSS_STATUS_RESETDONE)
		continue;
	uart_setreg(&sc->sc_bas, MDR1_REG, MDR1_MODE_UART);

	status = ns8250_bus_probe(sc); 
	if (status == 0)
		device_set_desc(sc->sc_dev, "TI UART (16550 compatible)");

	return (status);
}

static kobj_method_t ti8250_methods[] = {
	KOBJMETHOD(uart_probe,		ti8250_bus_probe),

	KOBJMETHOD(uart_attach,		ns8250_bus_attach),
	KOBJMETHOD(uart_detach,		ns8250_bus_detach),
	KOBJMETHOD(uart_flush,		ns8250_bus_flush),
	KOBJMETHOD(uart_getsig,		ns8250_bus_getsig),
	KOBJMETHOD(uart_ioctl,		ns8250_bus_ioctl),
	KOBJMETHOD(uart_ipend,		ns8250_bus_ipend),
	KOBJMETHOD(uart_param,		ns8250_bus_param),
	KOBJMETHOD(uart_receive,	ns8250_bus_receive),
	KOBJMETHOD(uart_setsig,		ns8250_bus_setsig),
	KOBJMETHOD(uart_transmit,	ns8250_bus_transmit),
	KOBJMETHOD_END
};

struct uart_class uart_ti8250_class = {
	"ti8250",
	ti8250_methods,
	sizeof(struct ti8250_softc),
	.uc_ops = &uart_ns8250_ops,
	.uc_range = 0x88,
	.uc_rclk = 48000000
};

