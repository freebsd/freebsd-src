/*-
 * Copyright (c) 2009, Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * All rights reserved.
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
 * Watchdog driver for Cavium Octeon
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/watchdog.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/rman.h>
#include <sys/smp.h>

#include <contrib/octeon-sdk/cvmx.h>
#include <contrib/octeon-sdk/cvmx-interrupt.h>

#define	DEFAULT_TIMER_VAL	65535

struct octeon_wdog_softc {
	device_t dev;
	/* XXX: replace with repscive CVMX_ constant */
	struct resource *irq_res[16];
	void *intr_hdl[16];
	int armed;
	int debug;
};

extern void octeon_wdog_nmi_handler(void);
void octeon_wdog_nmi(void);

static void octeon_watchdog_arm_core(int core, unsigned long timer_val);
static void octeon_watchdog_disarm_core(int core);
static int octeon_wdog_attach(device_t dev);
static void octeon_wdog_identify(driver_t *drv, device_t parent);
static int octeon_wdog_intr(void *);;
static int octeon_wdog_probe(device_t dev);
static void octeon_wdog_setup(struct octeon_wdog_softc *sc, int cpu);
static void octeon_wdog_sysctl(device_t dev);
static void octeon_wdog_watchdog_fn(void *private, u_int cmd, int *error);

void
octeon_wdog_nmi()
{

	/* XXX: Add something useful here */
	printf("NMI detected\n");

	/* 
	 * This is the end 
	 * Beautiful friend 
	 *
	 * Just wait for Soft Reset to come and take us
	 */
	for (;;)
		;
}

static void 
octeon_watchdog_arm_core(int core, unsigned long timer_val)
{
	cvmx_ciu_wdogx_t ciu_wdog;

	/* Poke it! */
	cvmx_write_csr(CVMX_CIU_PP_POKEX(core), 1);

	ciu_wdog.u64 = 0;
	ciu_wdog.s.len = timer_val;
	ciu_wdog.s.mode = 3;
	cvmx_write_csr(CVMX_CIU_WDOGX(core), ciu_wdog.u64);
}

static void 
octeon_watchdog_disarm_core(int core)
{

	cvmx_write_csr(CVMX_CIU_WDOGX(core), 0);
}



static void
octeon_wdog_watchdog_fn(void *private, u_int cmd, int *error)
{
	struct octeon_wdog_softc *sc = private;
	uint64_t timer_val = 0;

	cmd &= WD_INTERVAL;
	if (sc->debug)
		device_printf(sc->dev, "octeon_wdog_watchdog_fn: cmd: %x\n", cmd);
	if (cmd > 0) {
		if (sc->debug)
			device_printf(sc->dev, "octeon_wdog_watchdog_fn: programming timer: %jx\n", (uintmax_t) timer_val);
		/* 
		 * XXX: This should be done for every core and with value 
		 * calculated based on CPU frquency
		 */
		octeon_watchdog_arm_core(cvmx_get_core_num(), DEFAULT_TIMER_VAL);
		sc->armed = 1;
		*error = 0;
	} else {
		if (sc->debug)
			device_printf(sc->dev, "octeon_wdog_watchdog_fn: disarming\n");
		if (sc->armed) {
			sc->armed = 0;
		 	/* XXX: This should be done for every core */
			octeon_watchdog_disarm_core(cvmx_get_core_num());
		}
	}
}

static void
octeon_wdog_sysctl(device_t dev)
{
	struct octeon_wdog_softc *sc = device_get_softc(dev);

        struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->dev);
        struct sysctl_oid *tree = device_get_sysctl_tree(sc->dev);

        SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
                "debug", CTLFLAG_RW, &sc->debug, 0,
                "enable watchdog debugging");
        SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
                "armed", CTLFLAG_RD, &sc->armed, 0,
                "whether the watchdog is armed");
}

static void
octeon_wdog_setup(struct octeon_wdog_softc *sc, int cpu)
{
	int core, rid, err;

	/* XXX: map cpu id to core here ? */
	core = cvmx_get_core_num();

	/* Interrupt part */
	rid = 0;
	sc->irq_res[core] =
		bus_alloc_resource(sc->dev, SYS_RES_IRQ, &rid,
			CVMX_IRQ_WDOG0+core, 
			CVMX_IRQ_WDOG0+core, 1, RF_ACTIVE);
	if (!(sc->irq_res[core]))
		goto error;

	err = bus_setup_intr(sc->dev, sc->irq_res[core], INTR_TYPE_MISC,
	    octeon_wdog_intr, NULL, sc, &sc->intr_hdl[core]);
	if (err)
		goto error;

	/* XXX: pin interrupt handler to the respective core */

	/* Disarm by default */
	octeon_watchdog_disarm_core(core);

	return;

error:
	panic("failed to setup watchdog interrupt for core %d", core);
}


static int
octeon_wdog_intr(void *sc)
{

	/* Poke it! */
	cvmx_write_csr(CVMX_CIU_PP_POKEX(cvmx_get_core_num()), 1);

	return (FILTER_HANDLED);
}

static int
octeon_wdog_probe(device_t dev)
{

	device_set_desc(dev, "Cavium Octeon watchdog timer");
	return (0);
}

static int
octeon_wdog_attach(device_t dev)
{
	struct octeon_wdog_softc *sc = device_get_softc(dev);
	int i;
	uint64_t *nmi_handler = (uint64_t*)octeon_wdog_nmi_handler;
	
	/* Initialise */
	sc->armed = 0;
	sc->debug = 0;

	sc->dev = dev;
	EVENTHANDLER_REGISTER(watchdog_list, octeon_wdog_watchdog_fn, sc, 0);
	octeon_wdog_sysctl(dev);

	for (i = 0; i < 16; i++) {
		cvmx_write_csr(CVMX_MIO_BOOT_LOC_ADR, i * 8);
		cvmx_write_csr(CVMX_MIO_BOOT_LOC_DAT, nmi_handler[i]);
        }

	cvmx_write_csr(CVMX_MIO_BOOT_LOC_CFGX(0), 0x81fc0000);

	/* XXX: This should be done for every core */
	octeon_wdog_setup(sc, cvmx_get_core_num());
	return (0);
}

static void
octeon_wdog_identify(driver_t *drv, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, "octeon_wdog", 0);
}

static device_method_t octeon_wdog_methods[] = {
	DEVMETHOD(device_identify, octeon_wdog_identify),

	DEVMETHOD(device_probe, octeon_wdog_probe),
	DEVMETHOD(device_attach, octeon_wdog_attach),
	{0, 0},
};

static driver_t octeon_wdog_driver = {
	"octeon_wdog",
	octeon_wdog_methods,
	sizeof(struct octeon_wdog_softc),
};
static devclass_t octeon_wdog_devclass;

DRIVER_MODULE(octeon_wdog, ciu, octeon_wdog_driver, octeon_wdog_devclass, 0, 0);
