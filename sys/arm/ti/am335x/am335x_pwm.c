/*-
 * Copyright (c) 2013 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/ti/ti_prcm.h>
#include <arm/ti/ti_scm.h>

#include "am335x_pwm.h"
#include "am335x_scm.h"

#define	PWM_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	PWM_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	PWM_LOCK_INIT(_sc)	mtx_init(&(_sc)->sc_mtx, \
    device_get_nameunit(_sc->sc_dev), "am335x_pwm softc", MTX_DEF)
#define	PWM_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_mtx);

static struct resource_spec am335x_pwm_mem_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE }, /* PWMSS */
	{ SYS_RES_MEMORY, 1, RF_ACTIVE }, /* eCAP */
	{ SYS_RES_MEMORY, 2, RF_ACTIVE }, /* eQEP */
	{ SYS_RES_MEMORY, 3, RF_ACTIVE }, /*ePWM */
	{ -1, 0, 0 }
};

#define	PWMSS_READ4(_sc, reg)	bus_read_4((_sc)->sc_mem_res[0], reg);
#define	PWMSS_WRITE4(_sc, reg, value)	\
    bus_write_4((_sc)->sc_mem_res[0], reg, value);

#define	ECAP_READ2(_sc, reg)	bus_read_2((_sc)->sc_mem_res[1], reg);
#define	ECAP_WRITE2(_sc, reg, value)	\
    bus_write_2((_sc)->sc_mem_res[1], reg, value);
#define	ECAP_READ4(_sc, reg)	bus_read_4((_sc)->sc_mem_res[1], reg);
#define	ECAP_WRITE4(_sc, reg, value)	\
    bus_write_4((_sc)->sc_mem_res[1], reg, value);

#define	EPWM_READ2(_sc, reg)	bus_read_2((_sc)->sc_mem_res[3], reg);
#define	EPWM_WRITE2(_sc, reg, value)	\
    bus_write_2((_sc)->sc_mem_res[3], reg, value);

#define	PWMSS_IDVER		0x00
#define	PWMSS_SYSCONFIG		0x04
#define	PWMSS_CLKCONFIG		0x08
#define		CLKCONFIG_EPWMCLK_EN	(1 << 8)
#define	PWMSS_CLKSTATUS		0x0C

#define	ECAP_TSCTR		0x00
#define	ECAP_CAP1		0x08
#define	ECAP_CAP2		0x0C
#define	ECAP_CAP3		0x10
#define	ECAP_CAP4		0x14
#define	ECAP_ECCTL2		0x2A
#define		ECCTL2_MODE_APWM		(1 << 9)
#define		ECCTL2_SYNCO_SEL		(3 << 6)
#define		ECCTL2_TSCTRSTOP_FREERUN	(1 << 4)

#define	EPWM_TBCTL		0x00
#define		TBCTL_PHDIR_UP		(1 << 13)
#define		TBCTL_PHDIR_DOWN	(0 << 13)
#define		TBCTL_CLKDIV(x)		((x) << 10)
#define		TBCTL_HSPCLKDIV(x)	((x) << 7)
#define		TBCTL_SYNCOSEL_DISABLED	(3 << 4)
#define		TBCTL_PRDLD_SHADOW	(0 << 3)
#define		TBCTL_PRDLD_IMMEDIATE	(0 << 3)
#define		TBCTL_PHSEN_ENABLED	(1 << 2)
#define		TBCTL_PHSEN_DISABLED	(0 << 2)
#define	EPWM_TBSTS		0x02
#define	EPWM_TBPHSHR		0x04
#define	EPWM_TBPHS		0x06
#define	EPWM_TBCNT		0x08
#define	EPWM_TBPRD		0x0a
/* Counter-compare */
#define	EPWM_CMPCTL		0x0e
#define		CMPCTL_SHDWBMODE_SHADOW		(1 << 6)
#define		CMPCTL_SHDWBMODE_IMMEDIATE	(0 << 6)
#define		CMPCTL_SHDWAMODE_SHADOW		(1 << 4)
#define		CMPCTL_SHDWAMODE_IMMEDIATE	(0 << 4)
#define		CMPCTL_LOADBMODE_ZERO		(0 << 2)
#define		CMPCTL_LOADBMODE_PRD		(1 << 2)
#define		CMPCTL_LOADBMODE_EITHER		(2 << 2)
#define		CMPCTL_LOADBMODE_FREEZE		(3 << 2)
#define		CMPCTL_LOADAMODE_ZERO		(0 << 0)
#define		CMPCTL_LOADAMODE_PRD		(1 << 0)
#define		CMPCTL_LOADAMODE_EITHER		(2 << 0)
#define		CMPCTL_LOADAMODE_FREEZE		(3 << 0)
#define	EPWM_CMPAHR		0x10
#define	EPWM_CMPA		0x12
#define	EPWM_CMPB		0x14
/* CMPCTL_LOADAMODE_ZERO */
#define	EPWM_AQCTLA		0x16
#define	EPWM_AQCTLB		0x18
#define		AQCTL_CAU_NONE		(0 << 0)
#define		AQCTL_CAU_CLEAR		(1 << 0)
#define		AQCTL_CAU_SET		(2 << 0)
#define		AQCTL_CAU_TOGGLE	(3 << 0)
#define		AQCTL_ZRO_NONE		(0 << 0)
#define		AQCTL_ZRO_CLEAR		(1 << 0)
#define		AQCTL_ZRO_SET		(2 << 0)
#define		AQCTL_ZRO_TOGGLE	(3 << 0)
#define	EPWM_AQSFRC		0x1a
#define	EPWM_AQCSFRC		0x1c

static device_probe_t am335x_pwm_probe;
static device_attach_t am335x_pwm_attach;
static device_detach_t am335x_pwm_detach;

struct am335x_pwm_softc {
	device_t		sc_dev;
	struct mtx		sc_mtx;
	struct resource		*sc_mem_res[4];
	int			sc_id;
};

static device_method_t am335x_pwm_methods[] = {
	DEVMETHOD(device_probe,		am335x_pwm_probe),
	DEVMETHOD(device_attach,	am335x_pwm_attach),
	DEVMETHOD(device_detach,	am335x_pwm_detach),

	DEVMETHOD_END
};

static driver_t am335x_pwm_driver = {
	"am335x_pwm",
	am335x_pwm_methods,
	sizeof(struct am335x_pwm_softc),
};

static devclass_t am335x_pwm_devclass;

/*
 * API function to set period/duty cycles for ECASx 
 */
int
am335x_pwm_config_ecas(int unit, int period, int duty)
{
	device_t dev;
	struct am335x_pwm_softc *sc;
	uint16_t reg;

	dev = devclass_get_device(am335x_pwm_devclass, unit);
	if (dev == NULL)
		return (ENXIO);

	if (duty > period)
		return (EINVAL);

	if (period == 0)
		return (EINVAL);

	sc = device_get_softc(dev);
	PWM_LOCK(sc);

	reg = ECAP_READ2(sc, ECAP_ECCTL2);
	reg |= ECCTL2_MODE_APWM | ECCTL2_TSCTRSTOP_FREERUN | ECCTL2_SYNCO_SEL;
	ECAP_WRITE2(sc, ECAP_ECCTL2, reg);

	/* CAP3 in APWM mode is APRD shadow register */
	ECAP_WRITE4(sc, ECAP_CAP3, period - 1);

	/* CAP4 in APWM mode is ACMP shadow register */
	ECAP_WRITE4(sc, ECAP_CAP4, duty);
	/* Restart counter */
	ECAP_WRITE4(sc, ECAP_TSCTR, 0);

	PWM_UNLOCK(sc);

	return (0);
}

static int
am335x_pwm_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "ti,am335x-pwm"))
		return (ENXIO);

	device_set_desc(dev, "AM335x PWM");

	return (BUS_PROBE_DEFAULT);
}

static int
am335x_pwm_attach(device_t dev)
{
	struct am335x_pwm_softc *sc;
	int err;
	uint32_t reg;
	phandle_t node;
	pcell_t did;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	/* Get the PWM module id */
	node = ofw_bus_get_node(dev);
	if ((OF_getprop(node, "pwm-device-id", &did, sizeof(did))) <= 0) {
		device_printf(dev, "missing pwm-device-id attribute in FDT\n");
		return (ENXIO);
	}
	sc->sc_id = fdt32_to_cpu(did);

	PWM_LOCK_INIT(sc);

	err = bus_alloc_resources(dev, am335x_pwm_mem_spec,
	    sc->sc_mem_res);
	if (err) {
		device_printf(dev, "cannot allocate memory resources\n");
		goto fail;
	}

	ti_prcm_clk_enable(PWMSS0_CLK + sc->sc_id);
	ti_scm_reg_read_4(SCM_PWMSS_CTRL, &reg);
	reg |= (1 << sc->sc_id);
	ti_scm_reg_write_4(SCM_PWMSS_CTRL, reg);

	return (0);
fail:
	PWM_LOCK_DESTROY(sc);
	if (sc->sc_mem_res[0])
		bus_release_resources(dev, am335x_pwm_mem_spec,
		    sc->sc_mem_res);

	return(ENXIO);
}

static int
am335x_pwm_detach(device_t dev)
{
	struct am335x_pwm_softc *sc;

	sc = device_get_softc(dev);

	PWM_LOCK(sc);
	if (sc->sc_mem_res[0])
		bus_release_resources(dev, am335x_pwm_mem_spec,
		    sc->sc_mem_res);
	PWM_UNLOCK(sc);

	PWM_LOCK_DESTROY(sc);

	return (0);
}

DRIVER_MODULE(am335x_pwm, simplebus, am335x_pwm_driver, am335x_pwm_devclass, 0, 0);
MODULE_VERSION(am335x_pwm, 1);
MODULE_DEPEND(am335x_pwm, simplebus, 1, 1, 1);
