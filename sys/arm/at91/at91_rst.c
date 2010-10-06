/*-
 * Copyright (c) 2010 Greg Ansley.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91_rstreg.h>
#include <arm/at91/at91board.h>

#define RST_TIMEOUT (5)	/* Seconds to hold NRST for hard reset */
#define RST_TICK (20) 	/* sample NRST at hz/RST_TICK intervals */

static int rst_intr(void *arg);

static struct rst_softc {
	struct resource	*mem_res;	/* Memory resource */
	struct resource	*irq_res;	/* IRQ resource */
	void		*intrhand;	/* Interrupt handle */
	struct callout	tick_ch;	/* Tick callout */
	device_t	sc_dev;
	u_int		shutdown;	/* Shutdown in progress */
} *rst_sc;

static inline uint32_t
RD4(struct rst_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->mem_res, off));
}

static inline void
WR4(struct rst_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}

static int
at91_rst_probe(device_t dev)
{

	if (at91_is_sam9()) {
		device_set_desc(dev, "AT91SAM9 Reset Controller");
		return (0);
	}
	return (ENXIO);
}

static int
at91_rst_attach(device_t dev)
{
	struct rst_softc *sc;
	const char *cause;
	int rid, err;

	rst_sc = sc = device_get_softc(dev);
	sc->sc_dev = dev;

	callout_init(&sc->tick_ch, 0);

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "could not allocate memory resources.\n");
		err = ENOMEM;
		goto out;
	}
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "could not allocate interrupt resources.\n");
		err = ENOMEM;
		goto out;
	}

	/* Activate the interrupt. */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    rst_intr, NULL, sc, &sc->intrhand);
	if (err)
		device_printf(dev, "could not establish interrupt handler.\n");

	WR4(rst_sc, RST_MR, RST_MR_ERSTL(0xd) | RST_MR_URSIEN | RST_MR_KEY);

	switch (RD4(sc, RST_SR) & RST_SR_RST_MASK) {
		case	RST_SR_RST_POW:
			cause = "Power On";
			break;
		case	RST_SR_RST_WAKE:
			cause = "Wake Up";
			break;
		case	RST_SR_RST_WDT:	
			cause = "Watchdog";
			break;
		case	RST_SR_RST_SOFT:
			cause = "Software Request";
			break;
		case	RST_SR_RST_USR:
			cause = "External (User)";
			break;
		default:
			cause = "Unknown";
			break;
	}

	device_printf(dev, "Reset cause: %s.\n", cause);
	/* cpu_reset_addr = cpu_reset; */

out:
	return (err);
}

static void
rst_tick(void *argp)
{
	struct rst_softc *sc = argp;

	if (sc->shutdown++ >= RST_TIMEOUT * RST_TICK) {
		/* User released the button in morre than RST_TIMEOUT */
		cpu_reset();
	} else if ((RD4(sc, RST_SR) & RST_SR_NRSTL)) {
		/* User released the button in less than RST_TIMEOUT */
		sc->shutdown = 0; 
		device_printf(sc->sc_dev, "shutting down...\n");
		shutdown_nice(0);
	} else {
		callout_reset(&sc->tick_ch, hz/RST_TICK, rst_tick, sc);
	}
}

static int
rst_intr(void *argp)
{
	struct rst_softc *sc = argp;

	if (RD4(sc, RST_SR) & RST_SR_URSTS) {
		if (sc->shutdown == 0) 
			callout_reset(&sc->tick_ch, hz/RST_TICK, rst_tick, sc);
		return (FILTER_HANDLED);
	}
	return (FILTER_STRAY);
}

static device_method_t at91_rst_methods[] = {
	DEVMETHOD(device_probe, at91_rst_probe),
	DEVMETHOD(device_attach, at91_rst_attach),
	{0,0},
};

static driver_t at91_rst_driver = {
	"at91_rst",
	at91_rst_methods,
	sizeof(struct rst_softc),
};

static devclass_t at91_rst_devclass;

DRIVER_MODULE(at91_rst, atmelarm, at91_rst_driver, at91_rst_devclass, 0, 0);

void cpu_reset_sam9g20(void) __attribute__((weak));
void cpu_reset_sam9g20(void) {}

void
cpu_reset(void)
{

	if (rst_sc) {

		cpu_reset_sam9g20(); /* May be null */

		WR4(rst_sc, RST_MR,
		    RST_MR_ERSTL(0xd) | RST_MR_URSTEN | RST_MR_KEY);

		WR4(rst_sc, RST_CR,
		    RST_CR_PROCRST |
		    RST_CR_PERRST  |
		    RST_CR_EXTRST  |
		    RST_CR_KEY);
	}

	for(;;) ;
}
