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
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/ti/ti_prcm.h>
#include <arm/ti/ti_scm.h>

#include "am335x_pwm.h"
#include "am335x_scm.h"

/* In ticks */
#define	DEFAULT_PWM_PERIOD	1000

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
#define		TBCTL_FREERUN		(2 << 14)
#define		TBCTL_PHDIR_UP		(1 << 13)
#define		TBCTL_PHDIR_DOWN	(0 << 13)
#define		TBCTL_CLKDIV(x)		((x) << 10)
#define		TBCTL_CLKDIV_MASK	(3 << 10)
#define		TBCTL_HSPCLKDIV(x)	((x) << 7)
#define		TBCTL_HSPCLKDIV_MASK	(3 << 7)
#define		TBCTL_SYNCOSEL_DISABLED	(3 << 4)
#define		TBCTL_PRDLD_SHADOW	(0 << 3)
#define		TBCTL_PRDLD_IMMEDIATE	(0 << 3)
#define		TBCTL_PHSEN_ENABLED	(1 << 2)
#define		TBCTL_PHSEN_DISABLED	(0 << 2)
#define		TBCTL_CTRMODE_MASK	(3)
#define		TBCTL_CTRMODE_UP	(0 << 0)
#define		TBCTL_CTRMODE_DOWN	(1 << 0)
#define		TBCTL_CTRMODE_UPDOWN	(2 << 0)
#define		TBCTL_CTRMODE_FREEZE	(3 << 0)

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
#define		AQCTL_CBU_NONE		(0 << 8)
#define		AQCTL_CBU_CLEAR		(1 << 8)
#define		AQCTL_CBU_SET		(2 << 8)
#define		AQCTL_CBU_TOGGLE	(3 << 8)
#define		AQCTL_CAU_NONE		(0 << 4)
#define		AQCTL_CAU_CLEAR		(1 << 4)
#define		AQCTL_CAU_SET		(2 << 4)
#define		AQCTL_CAU_TOGGLE	(3 << 4)
#define		AQCTL_ZRO_NONE		(0 << 0)
#define		AQCTL_ZRO_CLEAR		(1 << 0)
#define		AQCTL_ZRO_SET		(2 << 0)
#define		AQCTL_ZRO_TOGGLE	(3 << 0)
#define	EPWM_AQSFRC		0x1a
#define	EPWM_AQCSFRC		0x1c

/* Trip-Zone module */
#define	EPWM_TZCTL		0x28
#define	EPWM_TZFLG		0x2C
/* High-Resolution PWM */
#define	EPWM_HRCTL		0x40
#define		HRCTL_DELMODE_BOTH	3
#define		HRCTL_DELMODE_FALL	2
#define		HRCTL_DELMODE_RISE	1

static device_probe_t am335x_pwm_probe;
static device_attach_t am335x_pwm_attach;
static device_detach_t am335x_pwm_detach;

struct am335x_pwm_softc {
	device_t		sc_dev;
	struct mtx		sc_mtx;
	struct resource		*sc_mem_res[4];
	int			sc_id;
	/* sysctl for configuration */
	struct sysctl_oid	*sc_period_oid;
	struct sysctl_oid	*sc_chanA_oid;
	struct sysctl_oid	*sc_chanB_oid;
	uint32_t		sc_pwm_period;
	uint32_t		sc_pwm_dutyA;
	uint32_t		sc_pwm_dutyB;
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
am335x_pwm_sysctl_duty(SYSCTL_HANDLER_ARGS)
{
	struct am335x_pwm_softc *sc = (struct am335x_pwm_softc*)arg1;
	int error;
	uint32_t duty;
       
	if (oidp == sc->sc_chanA_oid)
		duty = sc->sc_pwm_dutyA;
	else
		duty = sc->sc_pwm_dutyB;
	error = sysctl_handle_int(oidp, &duty, 0, req);

	if (error != 0 || req->newptr == NULL)
		return (error);

	if (duty > sc->sc_pwm_period) {
		device_printf(sc->sc_dev, "Duty cycle can't be greater then period\n");
		return (EINVAL);
	}

	PWM_LOCK(sc);
	if (oidp == sc->sc_chanA_oid) {
		sc->sc_pwm_dutyA = duty;
		EPWM_WRITE2(sc, EPWM_CMPA, sc->sc_pwm_dutyA);
	}
	else {
		sc->sc_pwm_dutyB = duty;
		EPWM_WRITE2(sc, EPWM_CMPB, sc->sc_pwm_dutyB);
	}
	PWM_UNLOCK(sc);

	return (error);
}

static int
am335x_pwm_sysctl_period(SYSCTL_HANDLER_ARGS)
{
	struct am335x_pwm_softc *sc = (struct am335x_pwm_softc*)arg1;
	int error;
	uint32_t period;
       
	period = sc->sc_pwm_period;
	error = sysctl_handle_int(oidp, &period, 0, req);

	if (error != 0 || req->newptr == NULL)
		return (error);

	if (period < 1)
		return (EINVAL);

	if ((period < sc->sc_pwm_dutyA) || (period < sc->sc_pwm_dutyB)) {
		device_printf(sc->sc_dev, "Period can't be less then duty cycle\n");
		return (EINVAL);
	}


	PWM_LOCK(sc);
	sc->sc_pwm_period = period;
	EPWM_WRITE2(sc, EPWM_TBPRD, period - 1);
	PWM_UNLOCK(sc);

	return (error);
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
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;

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

	/* Init backlight interface */
	ctx = device_get_sysctl_ctx(sc->sc_dev);
	tree = device_get_sysctl_tree(sc->sc_dev);

	sc->sc_period_oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "period", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
	    am335x_pwm_sysctl_period, "I", "PWM period");

	sc->sc_chanA_oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dutyA", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
	    am335x_pwm_sysctl_duty, "I", "Channel A duty cycles");

	sc->sc_chanB_oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dutyB", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
	    am335x_pwm_sysctl_duty, "I", "Channel B duty cycles");


	/* CONFIGURE EPWM1 */
	reg = EPWM_READ2(sc, EPWM_TBCTL);
	reg &= ~(TBCTL_CLKDIV_MASK | TBCTL_HSPCLKDIV_MASK);
	EPWM_WRITE2(sc, EPWM_TBCTL, reg);

	sc->sc_pwm_period = DEFAULT_PWM_PERIOD;
	sc->sc_pwm_dutyA = 0;
	sc->sc_pwm_dutyB = 0;

	EPWM_WRITE2(sc, EPWM_TBPRD, sc->sc_pwm_period - 1);
	EPWM_WRITE2(sc, EPWM_CMPA, sc->sc_pwm_dutyA);
	EPWM_WRITE2(sc, EPWM_CMPB, sc->sc_pwm_dutyB);

	EPWM_WRITE2(sc, EPWM_AQCTLA, (AQCTL_ZRO_SET | AQCTL_CAU_CLEAR));
	EPWM_WRITE2(sc, EPWM_AQCTLB, (AQCTL_ZRO_SET | AQCTL_CBU_CLEAR));

	/* START EPWM */
	reg &= ~TBCTL_CTRMODE_MASK;
	reg |= TBCTL_CTRMODE_UP | TBCTL_FREERUN;
	EPWM_WRITE2(sc, EPWM_TBCTL, reg);

	EPWM_WRITE2(sc, EPWM_TZCTL, 0xf);
	reg = EPWM_READ2(sc, EPWM_TZFLG);

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
