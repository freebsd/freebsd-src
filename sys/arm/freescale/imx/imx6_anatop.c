/*-
 * Copyright (c) 2013 Ian Lepore <ian@freebsd.org>
 * Copyright (c) 2014 Steven Lawrance <stl@koffein.net>
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

/*
 * Analog PLL and power regulator driver for Freescale i.MX6 family of SoCs.
 * Also, temperature montoring and cpu frequency control.  It was Freescale who
 * kitchen-sinked this device, not us. :)
 *
 * We don't really do anything with analog PLLs, but the registers for
 * controlling them belong to the same block as the power regulator registers.
 * Since the newbus hierarchy makes it hard for anyone other than us to get at
 * them, we just export a couple public functions to allow the imx6 CCM clock
 * driver to read and write those registers.
 *
 * We also don't do anything about power regulation yet, but when the need
 * arises, this would be the place for that code to live.
 *
 * I have no idea where the "anatop" name comes from.  It's in the standard DTS
 * source describing i.MX6 SoCs, and in the linux and u-boot code which comes
 * from Freescale, but it's not in the SoC manual.
 *
 * Note that temperature values throughout this code are handled in two types of
 * units.  Items with '_cnt' in the name use the hardware temperature count
 * units (higher counts are lower temperatures).  Items with '_val' in the name
 * are deci-Celcius, which are converted to/from deci-Kelvins in the sysctl
 * handlers (dK is the standard unit for temperature in sysctl).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

#include <arm/freescale/fsl_ocotpreg.h>
#include <arm/freescale/fsl_ocotpvar.h>
#include <arm/freescale/imx/imx6_anatopreg.h>
#include <arm/freescale/imx/imx6_anatopvar.h>

static struct resource_spec imx6_anatop_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};
#define	MEMRES	0
#define	IRQRES	1

struct imx6_anatop_softc {
	device_t	dev;
	struct resource	*res[2];
	uint32_t	cpu_curhz;
	uint32_t	cpu_curmhz;
	uint32_t	cpu_minhz;
	uint32_t	cpu_maxhz;
	uint32_t	refosc_hz;
	void		*temp_intrhand;
	uint32_t	temp_high_val;
	uint32_t	temp_high_cnt;
	uint32_t	temp_last_cnt;
	uint32_t	temp_room_cnt;
	struct callout	temp_throttle_callout;
	sbintime_t	temp_throttle_delay;
	uint32_t	temp_throttle_reset_cnt;
	uint32_t	temp_throttle_trigger_cnt;
	uint32_t	temp_throttle_val;
};

static struct imx6_anatop_softc *imx6_anatop_sc;

/*
 * Table of CPU max frequencies.  This is indexed by the max frequency value
 * (0-3) from the ocotp CFG3 register.
 */
static uint32_t imx6_cpu_maxhz_tab[] = {
	 792000000, 852000000, 996000000, 1200000000
};

#define	TZ_ZEROC	2732	/* deci-Kelvin <-> deci-Celcius offset. */

uint32_t
imx6_anatop_read_4(bus_size_t offset)
{

	KASSERT(imx6_anatop_sc != NULL, ("imx6_anatop_read_4 sc NULL"));

	return (bus_read_4(imx6_anatop_sc->res[MEMRES], offset));
}

void
imx6_anatop_write_4(bus_size_t offset, uint32_t value)
{

	KASSERT(imx6_anatop_sc != NULL, ("imx6_anatop_write_4 sc NULL"));

	bus_write_4(imx6_anatop_sc->res[MEMRES], offset, value);
}

static inline uint32_t
cpufreq_hz_from_div(struct imx6_anatop_softc *sc, uint32_t div)
{

	return (sc->refosc_hz * (div / 2));
}

static inline uint32_t
cpufreq_hz_to_div(struct imx6_anatop_softc *sc, uint32_t cpu_hz)
{

	return (cpu_hz / (sc->refosc_hz / 2));
}

static inline uint32_t
cpufreq_actual_hz(struct imx6_anatop_softc *sc, uint32_t cpu_hz)
{

	return (cpufreq_hz_from_div(sc, cpufreq_hz_to_div(sc, cpu_hz)));
}

static void 
cpufreq_set_clock(struct imx6_anatop_softc * sc, uint32_t cpu_newhz)
{
	uint32_t div, timeout, wrk32;
	const uint32_t mindiv =  54;
	const uint32_t maxdiv = 108;

	/*
	 * Clip the requested frequency to the configured max, then clip the
	 * resulting divisor to the documented min/max values.
	 */
	cpu_newhz = min(cpu_newhz, sc->cpu_maxhz);
	div = cpufreq_hz_to_div(sc, cpu_newhz);
	if (div < mindiv)
		div = mindiv;
	else if (div > maxdiv)
		div = maxdiv;
	sc->cpu_curhz = cpufreq_hz_from_div(sc, div);
	sc->cpu_curmhz = sc->cpu_curhz / 1000000;

	/*
	 * I can't find a documented procedure for changing the ARM PLL divisor,
	 * but some trial and error came up with this:
	 *  - Set the bypass clock source to REF_CLK_24M (source #0).
	 *  - Set the PLL into bypass mode; cpu should now be running at 24mhz.
	 *  - Change the divisor.
	 *  - Wait for the LOCK bit to come on; it takes ~50 loop iterations.
	 *  - Turn off bypass mode; cpu should now be running at cpu_newhz.
	 */
	imx6_anatop_write_4(IMX6_ANALOG_CCM_PLL_ARM_CLR, 
	    IMX6_ANALOG_CCM_PLL_ARM_CLK_SRC_MASK);
	imx6_anatop_write_4(IMX6_ANALOG_CCM_PLL_ARM_SET, 
	    IMX6_ANALOG_CCM_PLL_ARM_BYPASS);

	wrk32 = imx6_anatop_read_4(IMX6_ANALOG_CCM_PLL_ARM);
	wrk32 &= ~IMX6_ANALOG_CCM_PLL_ARM_DIV_MASK;
	wrk32 |= div;
	imx6_anatop_write_4(IMX6_ANALOG_CCM_PLL_ARM, wrk32);

	timeout = 10000;
	while ((imx6_anatop_read_4(IMX6_ANALOG_CCM_PLL_ARM) &
	    IMX6_ANALOG_CCM_PLL_ARM_LOCK) == 0)
		if (--timeout == 0)
			panic("imx6_set_cpu_clock(): PLL never locked");

	imx6_anatop_write_4(IMX6_ANALOG_CCM_PLL_ARM_CLR, 
	    IMX6_ANALOG_CCM_PLL_ARM_BYPASS);
}

static void
cpufreq_initialize(struct imx6_anatop_softc *sc)
{
	uint32_t cfg3speed;
	struct sysctl_ctx_list *ctx;

	ctx = device_get_sysctl_ctx(sc->dev);
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)),
	    OID_AUTO, "cpu_mhz", CTLFLAG_RD, &sc->cpu_curmhz, 0, 
	    "CPU frequency in MHz");

	/*
	 * XXX 24mhz shouldn't be hard-coded, should get this from imx6_ccm
	 * (even though in the real world it will always be 24mhz).  Oh wait a
	 * sec, I never wrote imx6_ccm.
	 */
	sc->refosc_hz = 24000000;

	/*
	 * Get the maximum speed this cpu can be set to.  The values in the
	 * OCOTP CFG3 register are not documented in the reference manual.
	 * The following info was in an archived email found via web search:
	 *   - 2b'11: 1200000000Hz;
	 *   - 2b'10: 996000000Hz;
	 *   - 2b'01: 852000000Hz; -- i.MX6Q Only, exclusive with 996MHz.
	 *   - 2b'00: 792000000Hz;
	 */
	cfg3speed = (fsl_ocotp_read_4(FSL_OCOTP_CFG3) & 
	    FSL_OCOTP_CFG3_SPEED_MASK) >> FSL_OCOTP_CFG3_SPEED_SHIFT;

	sc->cpu_minhz = cpufreq_actual_hz(sc, imx6_cpu_maxhz_tab[0]);
	sc->cpu_maxhz = cpufreq_actual_hz(sc, imx6_cpu_maxhz_tab[cfg3speed]);

	/*
	 * Set the CPU to maximum speed.
	 *
	 * We won't have thermal throttling until interrupts are enabled, but we
	 * want to run at full speed through all the device init stuff.  This
	 * basically assumes that a single core can't overheat before interrupts
	 * are enabled; empirical testing shows that to be a safe assumption.
	 */
	cpufreq_set_clock(sc, sc->cpu_maxhz);
	device_printf(sc->dev, "CPU frequency %uMHz\n", sc->cpu_curmhz);
}

static inline uint32_t
temp_from_count(struct imx6_anatop_softc *sc, uint32_t count)
{

	return (((sc->temp_high_val - (count - sc->temp_high_cnt) *
	    (sc->temp_high_val - 250) / 
	    (sc->temp_room_cnt - sc->temp_high_cnt))));
}

static inline uint32_t
temp_to_count(struct imx6_anatop_softc *sc, uint32_t temp)
{

	return ((sc->temp_room_cnt - sc->temp_high_cnt) * 
	    (sc->temp_high_val - temp) / (sc->temp_high_val - 250) + 
	    sc->temp_high_cnt);
}

static void
temp_update_count(struct imx6_anatop_softc *sc)
{
	uint32_t val;

	val = imx6_anatop_read_4(IMX6_ANALOG_TEMPMON_TEMPSENSE0);
	if (!(val & IMX6_ANALOG_TEMPMON_TEMPSENSE0_VALID))
		return;
	sc->temp_last_cnt =
	    (val & IMX6_ANALOG_TEMPMON_TEMPSENSE0_TEMP_CNT_MASK) >>
	    IMX6_ANALOG_TEMPMON_TEMPSENSE0_TEMP_CNT_SHIFT;
}

static int
temp_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	struct imx6_anatop_softc *sc = arg1;
	uint32_t t;

	temp_update_count(sc);

	t = temp_from_count(sc, sc->temp_last_cnt) + TZ_ZEROC;

	return (sysctl_handle_int(oidp, &t, 0, req));
}

static int
temp_throttle_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	struct imx6_anatop_softc *sc = arg1;
	int err;
	uint32_t temp;

	temp = sc->temp_throttle_val + TZ_ZEROC;
	err = sysctl_handle_int(oidp, &temp, 0, req);
	if (temp < TZ_ZEROC)
		return (ERANGE);
	temp -= TZ_ZEROC;
	if (err != 0 || req->newptr == NULL || temp == sc->temp_throttle_val)
		return (err);

	/* Value changed, update counts in softc and hardware. */
	sc->temp_throttle_val = temp;
	sc->temp_throttle_trigger_cnt = temp_to_count(sc, sc->temp_throttle_val);
	sc->temp_throttle_reset_cnt = temp_to_count(sc, sc->temp_throttle_val - 100);
	imx6_anatop_write_4(IMX6_ANALOG_TEMPMON_TEMPSENSE0_CLR,
	    IMX6_ANALOG_TEMPMON_TEMPSENSE0_ALARM_MASK);
	imx6_anatop_write_4(IMX6_ANALOG_TEMPMON_TEMPSENSE0_SET,
	    (sc->temp_throttle_trigger_cnt <<
	     IMX6_ANALOG_TEMPMON_TEMPSENSE0_ALARM_SHIFT));
	return (err);
}

static void
tempmon_gofast(struct imx6_anatop_softc *sc)
{

	if (sc->cpu_curhz < sc->cpu_maxhz) {
		cpufreq_set_clock(sc, sc->cpu_maxhz);
	}
}

static void
tempmon_goslow(struct imx6_anatop_softc *sc)
{

	if (sc->cpu_curhz > sc->cpu_minhz) {
		cpufreq_set_clock(sc, sc->cpu_minhz);
	}
}

static int
tempmon_intr(void *arg)
{
	struct imx6_anatop_softc *sc = arg;

	/*
	 * XXX Note that this code doesn't currently run (for some mysterious
	 * reason we just never get an interrupt), so the real monitoring is
	 * done by tempmon_throttle_check().
	 */
	tempmon_goslow(sc);
	/* XXX Schedule callout to speed back up eventually. */
	return (FILTER_HANDLED);
}

static void
tempmon_throttle_check(void *arg)
{
	struct imx6_anatop_softc *sc = arg;

	/* Lower counts are higher temperatures. */
	if (sc->temp_last_cnt < sc->temp_throttle_trigger_cnt)
		tempmon_goslow(sc);
	else if (sc->temp_last_cnt > (sc->temp_throttle_reset_cnt))
		tempmon_gofast(sc);

	callout_reset_sbt(&sc->temp_throttle_callout, sc->temp_throttle_delay,
		0, tempmon_throttle_check, sc, 0);

}

static void
initialize_tempmon(struct imx6_anatop_softc *sc)
{
	uint32_t cal;
	struct sysctl_ctx_list *ctx;

	/*
	 * Fetch calibration data: a sensor count at room temperature (25C),
	 * a sensor count at a high temperature, and that temperature
	 */
	cal = fsl_ocotp_read_4(FSL_OCOTP_ANA1);
	sc->temp_room_cnt = (cal & 0xFFF00000) >> 20;
	sc->temp_high_cnt = (cal & 0x000FFF00) >> 8;
	sc->temp_high_val = (cal & 0x000000FF) * 10;

	/*
	 * Throttle to a lower cpu freq at 10C below the "hot" temperature, and
	 * reset back to max cpu freq at 5C below the trigger.
	 */
	sc->temp_throttle_val = sc->temp_high_val - 100;
	sc->temp_throttle_trigger_cnt =
	    temp_to_count(sc, sc->temp_throttle_val);
	sc->temp_throttle_reset_cnt = 
	    temp_to_count(sc, sc->temp_throttle_val - 50);

	/*
	 * Set the sensor to sample automatically at 16Hz (32.768KHz/0x800), set
	 * the throttle count, and begin making measurements.
	 */
	imx6_anatop_write_4(IMX6_ANALOG_TEMPMON_TEMPSENSE1, 0x0800);
	imx6_anatop_write_4(IMX6_ANALOG_TEMPMON_TEMPSENSE0,
	    (sc->temp_throttle_trigger_cnt << 
	    IMX6_ANALOG_TEMPMON_TEMPSENSE0_ALARM_SHIFT) |
	    IMX6_ANALOG_TEMPMON_TEMPSENSE0_MEASURE);

	/*
	 * XXX Note that the alarm-interrupt feature isn't working yet, so
	 * we'll use a callout handler to check at 10Hz.  Make sure we have an
	 * initial temperature reading before starting up the callouts so we
	 * don't get a bogus reading of zero.
	 */
	while (sc->temp_last_cnt == 0)
		temp_update_count(sc);
	sc->temp_throttle_delay = 100 * SBT_1MS;
	callout_init(&sc->temp_throttle_callout, 0);
	callout_reset_sbt(&sc->temp_throttle_callout, sc->temp_throttle_delay, 
	    0, tempmon_throttle_check, sc, 0);

	ctx = device_get_sysctl_ctx(sc->dev);
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)),
	    OID_AUTO, "temperature", CTLTYPE_INT | CTLFLAG_RD, sc, 0,
	    temp_sysctl_handler, "IK", "Current die temperature");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)),
	    OID_AUTO, "throttle_temperature", CTLTYPE_INT | CTLFLAG_RW, sc,
	    0, temp_throttle_sysctl_handler, "IK", 
	    "Throttle CPU when exceeding this temperature");
}

static int
imx6_anatop_detach(device_t dev)
{

	return (EBUSY);
}

static int
imx6_anatop_attach(device_t dev)
{
	struct imx6_anatop_softc *sc;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Allocate bus_space resources. */
	if (bus_alloc_resources(dev, imx6_anatop_spec, sc->res)) {
		device_printf(dev, "Cannot allocate resources\n");
		err = ENXIO;
		goto out;
	}

	err = bus_setup_intr(dev, sc->res[IRQRES], INTR_TYPE_MISC | INTR_MPSAFE,
	    tempmon_intr, NULL, sc, &sc->temp_intrhand);
	if (err != 0)
		goto out;

	imx6_anatop_sc = sc;

	/*
	 * Other code seen on the net sets this SELFBIASOFF flag around the same
	 * time the temperature sensor is set up, although it's unclear how the
	 * two are related (if at all).
	 */
	imx6_anatop_write_4(IMX6_ANALOG_PMU_MISC0_SET, 
	    IMX6_ANALOG_PMU_MISC0_SELFBIASOFF);

	cpufreq_initialize(sc);
	initialize_tempmon(sc);

	err = 0;

out:

	if (err != 0) {
		bus_release_resources(dev, imx6_anatop_spec, sc->res);
	}

	return (err);
}

static int
imx6_anatop_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "fsl,imx6q-anatop") == 0)
		return (ENXIO);

	device_set_desc(dev, "Freescale i.MX6 Analog PLLs and Power");

	return (BUS_PROBE_DEFAULT);
}

uint32_t 
imx6_get_cpu_clock()
{
	uint32_t div;

	div = imx6_anatop_read_4(IMX6_ANALOG_CCM_PLL_ARM) &
	    IMX6_ANALOG_CCM_PLL_ARM_DIV_MASK;
	return (cpufreq_hz_from_div(imx6_anatop_sc, div));
}

static device_method_t imx6_anatop_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,  imx6_anatop_probe),
	DEVMETHOD(device_attach, imx6_anatop_attach),
	DEVMETHOD(device_detach, imx6_anatop_detach),

	DEVMETHOD_END
};

static driver_t imx6_anatop_driver = {
	"imx6_anatop",
	imx6_anatop_methods,
	sizeof(struct imx6_anatop_softc)
};

static devclass_t imx6_anatop_devclass;

DRIVER_MODULE(imx6_anatop, simplebus, imx6_anatop_driver, imx6_anatop_devclass, 0, 0);

