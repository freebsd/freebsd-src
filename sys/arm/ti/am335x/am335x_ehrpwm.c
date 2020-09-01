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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "pwmbus_if.h"

#include "am335x_pwm.h"

/*******************************************************************************
 * Enhanced resolution PWM driver.  Many of the advanced featues of the hardware
 * are not supported by this driver.  What is implemented here is simple
 * variable-duty-cycle PWM output.
 *
 * Note that this driver was historically configured using a set of sysctl
 * variables/procs, and later gained support for the PWM(9) API.  The sysctl
 * code is still present to support existing apps, but that interface is
 * considered deprecated.
 *
 * An important caveat is that the original sysctl interface and the new PWM API
 * cannot both be used at once.  If both interfaces are used to change
 * configuration, it's quite likely you won't get the expected results.  Also,
 * reading the sysctl values after configuring via PWM will not return the right
 * results.
 ******************************************************************************/

/* In ticks */
#define	DEFAULT_PWM_PERIOD	1000
#define	PWM_CLOCK		100000000UL

#define	NS_PER_SEC		1000000000

#define	PWM_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	PWM_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	PWM_LOCK_ASSERT(_sc)    mtx_assert(&(_sc)->sc_mtx, MA_OWNED)
#define	PWM_LOCK_INIT(_sc)	mtx_init(&(_sc)->sc_mtx, \
    device_get_nameunit(_sc->sc_dev), "am335x_ehrpwm softc", MTX_DEF)
#define	PWM_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_mtx)

#define	EPWM_READ2(_sc, reg)	bus_read_2((_sc)->sc_mem_res, reg)
#define	EPWM_WRITE2(_sc, reg, value)	\
    bus_write_2((_sc)->sc_mem_res, reg, value)

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
#define		AQCSFRC_OFF		0
#define		AQCSFRC_LO		1
#define		AQCSFRC_HI		2
#define		AQCSFRC_MASK		3
#define		AQCSFRC(chan, hilo)	((hilo) << (2 * chan))

/* Trip-Zone module */
#define	EPWM_TZCTL		0x28
#define	EPWM_TZFLG		0x2C
/* High-Resolution PWM */
#define	EPWM_HRCTL		0x40
#define		HRCTL_DELMODE_BOTH	3
#define		HRCTL_DELMODE_FALL	2
#define		HRCTL_DELMODE_RISE	1

static device_probe_t am335x_ehrpwm_probe;
static device_attach_t am335x_ehrpwm_attach;
static device_detach_t am335x_ehrpwm_detach;

static int am335x_ehrpwm_clkdiv[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

struct ehrpwm_channel {
	u_int	duty;		/* on duration, in ns */
	bool	enabled;	/* channel enabled? */
	bool	inverted;	/* signal inverted? */
};
#define	NUM_CHANNELS	2

struct am335x_ehrpwm_softc {
	device_t		sc_dev;
	device_t		sc_busdev;
	struct mtx		sc_mtx;
	struct resource		*sc_mem_res;
	int			sc_mem_rid;

	/* Things used for configuration via sysctl [deprecated]. */
	int			sc_pwm_clkdiv;
	int			sc_pwm_freq;
	struct sysctl_oid	*sc_clkdiv_oid;
	struct sysctl_oid	*sc_freq_oid;
	struct sysctl_oid	*sc_period_oid;
	struct sysctl_oid	*sc_chanA_oid;
	struct sysctl_oid	*sc_chanB_oid;
	uint32_t		sc_pwm_period;
	uint32_t		sc_pwm_dutyA;
	uint32_t		sc_pwm_dutyB;

	/* Things used for configuration via pwm(9) api. */
	u_int			sc_clkfreq; /* frequency in Hz */
	u_int			sc_clktick; /* duration in ns */
	u_int			sc_period;  /* duration in ns */
	struct ehrpwm_channel	sc_channels[NUM_CHANNELS];
};

static struct ofw_compat_data compat_data[] = {
	{"ti,am33xx-ehrpwm",    true},
	{NULL,                  false},
};
SIMPLEBUS_PNP_INFO(compat_data);

static void
am335x_ehrpwm_cfg_duty(struct am335x_ehrpwm_softc *sc, u_int chan, u_int duty)
{
	u_int tbcmp;

	if (duty == 0)
		tbcmp = 0;
	else
		tbcmp = max(1, duty / sc->sc_clktick);

	sc->sc_channels[chan].duty = tbcmp * sc->sc_clktick;

	PWM_LOCK_ASSERT(sc);
	EPWM_WRITE2(sc, (chan == 0) ? EPWM_CMPA : EPWM_CMPB, tbcmp);
}

static void
am335x_ehrpwm_cfg_enable(struct am335x_ehrpwm_softc *sc, u_int chan, bool enable)
{
	uint16_t regval;

	sc->sc_channels[chan].enabled = enable;

	/*
	 * Turn off any existing software-force of the channel, then force
	 * it in the right direction (high or low) if it's not being enabled.
	 */
	PWM_LOCK_ASSERT(sc);
	regval = EPWM_READ2(sc, EPWM_AQCSFRC);
	regval &= ~AQCSFRC(chan, AQCSFRC_MASK);
	if (!sc->sc_channels[chan].enabled) {
		if (sc->sc_channels[chan].inverted)
			regval |= AQCSFRC(chan, AQCSFRC_HI);
		else
			regval |= AQCSFRC(chan, AQCSFRC_LO);
	}
	EPWM_WRITE2(sc, EPWM_AQCSFRC, regval);
}

static bool
am335x_ehrpwm_cfg_period(struct am335x_ehrpwm_softc *sc, u_int period)
{
	uint16_t regval;
	u_int clkdiv, hspclkdiv, pwmclk, pwmtick, tbprd;

	/* Can't do a period shorter than 2 clock ticks. */
	if (period < 2 * NS_PER_SEC / PWM_CLOCK) {
		sc->sc_clkfreq = 0;
		sc->sc_clktick = 0;
		sc->sc_period  = 0;
		return (false);
	}

	/*
	 * Figure out how much we have to divide down the base 100MHz clock so
	 * that we can express the requested period as a 16-bit tick count.
	 */
	tbprd = 0;
	for (clkdiv = 0; clkdiv < 8; ++clkdiv) {
		const u_int cd = 1 << clkdiv;
		for (hspclkdiv = 0; hspclkdiv < 8; ++hspclkdiv) {
			const u_int cdhs = max(1, hspclkdiv * 2);
			pwmclk = PWM_CLOCK / (cd * cdhs);
			pwmtick = NS_PER_SEC / pwmclk;
			if (period / pwmtick < 65536) {
				tbprd = period / pwmtick;
				break;
			}
		}
		if (tbprd != 0)
			break;
	}

	/* Handle requested period too long for available clock divisors. */
	if (tbprd == 0)
		return (false);

	/*
	 * If anything has changed from the current settings, reprogram the
	 * clock divisors and period register.
	 */
	if (sc->sc_clkfreq != pwmclk || sc->sc_clktick != pwmtick ||
	    sc->sc_period != tbprd * pwmtick) {
		sc->sc_clkfreq = pwmclk;
		sc->sc_clktick = pwmtick;
		sc->sc_period  = tbprd * pwmtick;

		PWM_LOCK_ASSERT(sc);
		regval = EPWM_READ2(sc, EPWM_TBCTL);
		regval &= ~(TBCTL_CLKDIV_MASK | TBCTL_HSPCLKDIV_MASK);
		regval |= TBCTL_CLKDIV(clkdiv) | TBCTL_HSPCLKDIV(hspclkdiv);
		EPWM_WRITE2(sc, EPWM_TBCTL, regval);
		EPWM_WRITE2(sc, EPWM_TBPRD, tbprd - 1);
#if 0
		device_printf(sc->sc_dev, "clkdiv %u hspclkdiv %u tbprd %u "
		    "clkfreq %u Hz clktick %u ns period got %u requested %u\n",
		    clkdiv, hspclkdiv, tbprd - 1,
		    sc->sc_clkfreq, sc->sc_clktick, sc->sc_period, period);
#endif
		/*
		 * If the period changed, that invalidates the current CMP
		 * registers (duty values), just zero them out.
		 */
		am335x_ehrpwm_cfg_duty(sc, 0, 0);
		am335x_ehrpwm_cfg_duty(sc, 1, 0);
	}

	return (true);
}

static void
am335x_ehrpwm_freq(struct am335x_ehrpwm_softc *sc)
{
	int clkdiv;

	clkdiv = am335x_ehrpwm_clkdiv[sc->sc_pwm_clkdiv];
	sc->sc_pwm_freq = PWM_CLOCK / (1 * clkdiv) / sc->sc_pwm_period;
}

static int
am335x_ehrpwm_sysctl_freq(SYSCTL_HANDLER_ARGS)
{
	int clkdiv, error, freq, i, period;
	struct am335x_ehrpwm_softc *sc;
	uint32_t reg;

	sc = (struct am335x_ehrpwm_softc *)arg1;

	PWM_LOCK(sc);
	freq = sc->sc_pwm_freq;
	PWM_UNLOCK(sc);

	error = sysctl_handle_int(oidp, &freq, sizeof(freq), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (freq > PWM_CLOCK)
		freq = PWM_CLOCK;

	PWM_LOCK(sc);
	if (freq != sc->sc_pwm_freq) {
		for (i = nitems(am335x_ehrpwm_clkdiv) - 1; i >= 0; i--) {
			clkdiv = am335x_ehrpwm_clkdiv[i];
			period = PWM_CLOCK / clkdiv / freq;
			if (period > USHRT_MAX)
				break;
			sc->sc_pwm_clkdiv = i;
			sc->sc_pwm_period = period;
		}
		/* Reset the duty cycle settings. */
		sc->sc_pwm_dutyA = 0;
		sc->sc_pwm_dutyB = 0;
		EPWM_WRITE2(sc, EPWM_CMPA, sc->sc_pwm_dutyA);
		EPWM_WRITE2(sc, EPWM_CMPB, sc->sc_pwm_dutyB);
		/* Update the clkdiv settings. */
		reg = EPWM_READ2(sc, EPWM_TBCTL);
		reg &= ~TBCTL_CLKDIV_MASK;
		reg |= TBCTL_CLKDIV(sc->sc_pwm_clkdiv);
		EPWM_WRITE2(sc, EPWM_TBCTL, reg);
		/* Update the period settings. */
		EPWM_WRITE2(sc, EPWM_TBPRD, sc->sc_pwm_period - 1);
		am335x_ehrpwm_freq(sc);
	}
	PWM_UNLOCK(sc);

	return (0);
}

static int
am335x_ehrpwm_sysctl_clkdiv(SYSCTL_HANDLER_ARGS)
{
	int error, i, clkdiv;
	struct am335x_ehrpwm_softc *sc;
	uint32_t reg;

	sc = (struct am335x_ehrpwm_softc *)arg1;

	PWM_LOCK(sc);
	clkdiv = am335x_ehrpwm_clkdiv[sc->sc_pwm_clkdiv];
	PWM_UNLOCK(sc);

	error = sysctl_handle_int(oidp, &clkdiv, sizeof(clkdiv), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	PWM_LOCK(sc);
	if (clkdiv != am335x_ehrpwm_clkdiv[sc->sc_pwm_clkdiv]) {
		for (i = 0; i < nitems(am335x_ehrpwm_clkdiv); i++)
			if (clkdiv >= am335x_ehrpwm_clkdiv[i])
				sc->sc_pwm_clkdiv = i;

		reg = EPWM_READ2(sc, EPWM_TBCTL);
		reg &= ~TBCTL_CLKDIV_MASK;
		reg |= TBCTL_CLKDIV(sc->sc_pwm_clkdiv);
		EPWM_WRITE2(sc, EPWM_TBCTL, reg);
		am335x_ehrpwm_freq(sc);
	}
	PWM_UNLOCK(sc);

	return (0);
}

static int
am335x_ehrpwm_sysctl_duty(SYSCTL_HANDLER_ARGS)
{
	struct am335x_ehrpwm_softc *sc = (struct am335x_ehrpwm_softc*)arg1;
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
am335x_ehrpwm_sysctl_period(SYSCTL_HANDLER_ARGS)
{
	struct am335x_ehrpwm_softc *sc = (struct am335x_ehrpwm_softc*)arg1;
	int error;
	uint32_t period;

	period = sc->sc_pwm_period;
	error = sysctl_handle_int(oidp, &period, 0, req);

	if (error != 0 || req->newptr == NULL)
		return (error);

	if (period < 1)
		return (EINVAL);

	if (period > USHRT_MAX)
		period = USHRT_MAX;

	PWM_LOCK(sc);
	/* Reset the duty cycle settings. */
	sc->sc_pwm_dutyA = 0;
	sc->sc_pwm_dutyB = 0;
	EPWM_WRITE2(sc, EPWM_CMPA, sc->sc_pwm_dutyA);
	EPWM_WRITE2(sc, EPWM_CMPB, sc->sc_pwm_dutyB);
	/* Update the period settings. */
	sc->sc_pwm_period = period;
	EPWM_WRITE2(sc, EPWM_TBPRD, period - 1);
	am335x_ehrpwm_freq(sc);
	PWM_UNLOCK(sc);

	return (error);
}

static int
am335x_ehrpwm_channel_count(device_t dev, u_int *nchannel)
{

	*nchannel = NUM_CHANNELS;

	return (0);
}

static int
am335x_ehrpwm_channel_config(device_t dev, u_int channel, u_int period, u_int duty)
{
	struct am335x_ehrpwm_softc *sc;
	bool status;

	if (channel >= NUM_CHANNELS)
		return (EINVAL);

	sc = device_get_softc(dev);

	PWM_LOCK(sc);
	status = am335x_ehrpwm_cfg_period(sc, period);
	if (status)
		am335x_ehrpwm_cfg_duty(sc, channel, duty);
	PWM_UNLOCK(sc);

	return (status ? 0 : EINVAL);
}

static int
am335x_ehrpwm_channel_get_config(device_t dev, u_int channel, 
    u_int *period, u_int *duty)
{
	struct am335x_ehrpwm_softc *sc;

	if (channel >= NUM_CHANNELS)
		return (EINVAL);

	sc = device_get_softc(dev);
	*period = sc->sc_period;
	*duty = sc->sc_channels[channel].duty;
	return (0);
}

static int
am335x_ehrpwm_channel_enable(device_t dev, u_int channel, bool enable)
{
	struct am335x_ehrpwm_softc *sc;

	if (channel >= NUM_CHANNELS)
		return (EINVAL);

	sc = device_get_softc(dev);

	PWM_LOCK(sc);
	am335x_ehrpwm_cfg_enable(sc, channel, enable);
	PWM_UNLOCK(sc);

	return (0);
}

static int
am335x_ehrpwm_channel_is_enabled(device_t dev, u_int channel, bool *enabled)
{
	struct am335x_ehrpwm_softc *sc;

	if (channel >= NUM_CHANNELS)
		return (EINVAL);

	sc = device_get_softc(dev);

	*enabled = sc->sc_channels[channel].enabled;

	return (0);
}

static int
am335x_ehrpwm_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "AM335x EHRPWM");

	return (BUS_PROBE_DEFAULT);
}

static int
am335x_ehrpwm_attach(device_t dev)
{
	struct am335x_ehrpwm_softc *sc;
	uint32_t reg;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	PWM_LOCK_INIT(sc);

	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_mem_rid, RF_ACTIVE);
	if (sc->sc_mem_res == NULL) {
		device_printf(dev, "cannot allocate memory resources\n");
		goto fail;
	}

	/* Init sysctl interface */
	ctx = device_get_sysctl_ctx(sc->sc_dev);
	tree = device_get_sysctl_tree(sc->sc_dev);

	sc->sc_clkdiv_oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "clkdiv", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    am335x_ehrpwm_sysctl_clkdiv, "I", "PWM clock prescaler");

	sc->sc_freq_oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "freq", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    am335x_ehrpwm_sysctl_freq, "I", "PWM frequency");

	sc->sc_period_oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "period", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    am335x_ehrpwm_sysctl_period, "I", "PWM period");

	sc->sc_chanA_oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dutyA", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    am335x_ehrpwm_sysctl_duty, "I", "Channel A duty cycles");

	sc->sc_chanB_oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dutyB", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    am335x_ehrpwm_sysctl_duty, "I", "Channel B duty cycles");

	/* CONFIGURE EPWM1 */
	reg = EPWM_READ2(sc, EPWM_TBCTL);
	reg &= ~(TBCTL_CLKDIV_MASK | TBCTL_HSPCLKDIV_MASK);
	EPWM_WRITE2(sc, EPWM_TBCTL, reg);

	sc->sc_pwm_period = DEFAULT_PWM_PERIOD;
	sc->sc_pwm_dutyA = 0;
	sc->sc_pwm_dutyB = 0;
	am335x_ehrpwm_freq(sc);

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

	if ((sc->sc_busdev = device_add_child(dev, "pwmbus", -1)) == NULL) {
		device_printf(dev, "Cannot add child pwmbus\n");
		// This driver can still do things even without the bus child.
	}

	bus_generic_probe(dev);
	return (bus_generic_attach(dev));
fail:
	PWM_LOCK_DESTROY(sc);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->sc_mem_rid, sc->sc_mem_res);

	return(ENXIO);
}

static int
am335x_ehrpwm_detach(device_t dev)
{
	struct am335x_ehrpwm_softc *sc;
	int error;

	sc = device_get_softc(dev);

	if ((error = bus_generic_detach(sc->sc_dev)) != 0)
		return (error);

	PWM_LOCK(sc);

	if (sc->sc_busdev != NULL)
		device_delete_child(dev, sc->sc_busdev);

	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->sc_mem_rid, sc->sc_mem_res);

	PWM_UNLOCK(sc);

	PWM_LOCK_DESTROY(sc);

	return (0);
}

static phandle_t
am335x_ehrpwm_get_node(device_t bus, device_t dev)
{

	/*
	 * Share our controller node with our pwmbus child; it instantiates
	 * devices by walking the children contained within our node.
	 */
	return ofw_bus_get_node(bus);
}

static device_method_t am335x_ehrpwm_methods[] = {
	DEVMETHOD(device_probe,		am335x_ehrpwm_probe),
	DEVMETHOD(device_attach,	am335x_ehrpwm_attach),
	DEVMETHOD(device_detach,	am335x_ehrpwm_detach),

	/* ofw_bus_if */
	DEVMETHOD(ofw_bus_get_node,	am335x_ehrpwm_get_node),

	/* pwm interface */
	DEVMETHOD(pwmbus_channel_count,		am335x_ehrpwm_channel_count),
	DEVMETHOD(pwmbus_channel_config,	am335x_ehrpwm_channel_config),
	DEVMETHOD(pwmbus_channel_get_config,	am335x_ehrpwm_channel_get_config),
	DEVMETHOD(pwmbus_channel_enable,	am335x_ehrpwm_channel_enable),
	DEVMETHOD(pwmbus_channel_is_enabled,	am335x_ehrpwm_channel_is_enabled),

	DEVMETHOD_END
};

static driver_t am335x_ehrpwm_driver = {
	"pwm",
	am335x_ehrpwm_methods,
	sizeof(struct am335x_ehrpwm_softc),
};

static devclass_t am335x_ehrpwm_devclass;

DRIVER_MODULE(am335x_ehrpwm, am335x_pwmss, am335x_ehrpwm_driver, am335x_ehrpwm_devclass, 0, 0);
MODULE_VERSION(am335x_ehrpwm, 1);
MODULE_DEPEND(am335x_ehrpwm, am335x_pwmss, 1, 1, 1);
MODULE_DEPEND(am335x_ehrpwm, pwmbus, 1, 1, 1);
