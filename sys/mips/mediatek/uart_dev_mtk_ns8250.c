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

#include <mips/mediatek/mtk_soc.h>
#include <mips/mediatek/mtk_sysctl.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_cpu_fdt.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_dev_ns8250.h>

#include "uart_if.h"

/*
 * High-level UART interface.
 */
static struct uart_class uart_mtk_ns8250_class;
static int mtk_ns8250_bus_probe(struct uart_softc *);

static kobj_method_t mtk_ns8250_methods[] = {
	KOBJMETHOD(uart_probe,		mtk_ns8250_bus_probe),

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

static struct uart_class uart_mtk_ns8250_class = {
	"mtk8250",
	mtk_ns8250_methods,
	sizeof(struct ns8250_softc),
	.uc_ops = &uart_ns8250_ops,
	.uc_range = 1, /* use hinted range */
	.uc_rclk = 0,
	.uc_rshift = 2
};

static struct ofw_compat_data compat_data[] = {
	{ "mtk,ns16550a",	(uintptr_t)&uart_mtk_ns8250_class },
	{ "ns16550a",		(uintptr_t)&uart_mtk_ns8250_class },
	{ NULL,			(uintptr_t)NULL },
};
UART_FDT_CLASS_AND_DEVICE(compat_data);

static int
mtk_ns8250_bus_probe(struct uart_softc *sc)
{       
	int status;
       
	if (!ofw_bus_status_okay(sc->sc_dev))
		return (ENXIO);
        
	if (ofw_bus_search_compatible(sc->sc_dev, compat_data)->ocd_data ==
	    (uintptr_t)NULL)
		return (ENXIO);
        
	sc->sc_bas.rclk = mtk_soc_get_uartclk();
        
	status = ns8250_bus_probe(sc);
	if (status == 0)
		device_set_desc(sc->sc_dev, "MTK UART Controller (ns16550a)");
        
	return (status);
}
