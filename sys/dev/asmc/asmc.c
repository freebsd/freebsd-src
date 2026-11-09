/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007, 2008 Rui Paulo <rpaulo@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Driver for Apple's System Management Console (SMC).
 * SMC can be found on the MacBook, MacBook Pro and Mac Mini.
 *
 * Inspired by the Linux applesmc driver.
 */

#include "opt_asmc.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/rman.h>

#include <machine/resource.h>
#include <netinet/in.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>
#include <dev/asmc/asmcvar.h>
#include <dev/asmc/asmcmmio.h>
#include <dev/asmc/asmc_pio.h>
#include <dev/asmc/asmc_debug.h>

#include <dev/backlight/backlight.h>
#include "backlight_if.h"

/*
 * Device interface.
 */
static int 	asmc_probe(device_t dev);
static int 	asmc_attach(device_t dev);
static int 	asmc_detach(device_t dev);
static int 	asmc_resume(device_t dev);
static void	asmc_shutdown(void *arg, int howto);

/*
 * Backlight interface.
 */
static int	asmc_backlight_update_status(device_t dev,
    struct backlight_props *props);
static int	asmc_backlight_get_status(device_t dev,
    struct backlight_props *props);
static int	asmc_backlight_get_info(device_t dev, struct backlight_info *info);

/*
 * SMC functions.
 */
static int 	asmc_init(device_t dev);
static int 	asmc_fan_count(device_t dev);
static int 	asmc_fan_getvalue(device_t dev, const char *key, int fan);
static int 	asmc_fan_setvalue(device_t dev, const char *key, int fan, int speed);
static int 	asmc_temp_getvalue(device_t dev, const char *key);
static int 	asmc_sms_read(device_t, const char *key, int16_t *val);
static void 	asmc_sms_calibrate(device_t dev);
static int 	asmc_sms_intrfast(void *arg);
static void 	asmc_sms_printintr(device_t dev, uint8_t);
static void 	asmc_sms_task(void *arg, int pending);
static void	asmc_sms_init(device_t dev);
static void	asmc_detect_capabilities(device_t dev);

/*
 * Sysctl handlers.
 */
static int 	asmc_mb_sysctl_fanid(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fanspeed(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fanmanual(SYSCTL_HANDLER_ARGS);
static int 	asmc_temp_sysctl(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_sms(SYSCTL_HANDLER_ARGS);
static int 	asmc_mbp_sysctl_light(SYSCTL_HANDLER_ARGS);
static int 	asmc_mbp_sysctl_light_control(SYSCTL_HANDLER_ARGS);
static int 	asmc_mbp_sysctl_light_left_10byte(SYSCTL_HANDLER_ARGS);
static int	asmc_aupo_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_sil_sysctl(SYSCTL_HANDLER_ARGS);

/* System state / board identity sysctls */
static int	asmc_cause_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_msal_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_clkt_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_msps_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_rplt_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_rgen_sysctl(SYSCTL_HANDLER_ARGS);

/* Voltage/Current/Power/Light sensor support */
static int	asmc_sensor_read(device_t, const char *, int *);
static int	asmc_sensor_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_detect_sensors(device_t);
static int	asmc_key_search(device_t, const char *, unsigned int *);
static const char *asmc_temp_desc(const char *key);

/*
 * SMC temperature key descriptions.
 * These are universal across all Intel Apple hardware.
 */
static const struct {
	const char	*key;
	const char	*desc;
} asmc_temp_descs[] = {
	/* Ambient / airflow */
	{ "TA0P", "Ambient" },
	{ "TA0S", "PCIe Slot 1 Ambient" },
	{ "TA0p", "Ambient Air" },
	{ "TA1P", "Ambient 2" },
	{ "TA1S", "PCIe Slot 1 PCB" },
	{ "TA1p", "Ambient Air 2" },
	{ "TA2P", "Ambient 3" },
	{ "TA2S", "PCIe Slot 2 Ambient" },
	{ "TA3S", "PCIe Slot 2 PCB" },
	{ "TA0V", "Ambient" },
	{ "TALP", "Ambient Light Proximity" },
	{ "TaLC", "Airflow Left" },
	{ "TaRC", "Airflow Right" },
	{ "Ta0P", "Airflow Proximity" },
	/* Battery / enclosure */
	{ "TB0T", "Enclosure Bottom" },
	{ "TB1T", "Battery 1" },
	{ "TB2T", "Battery 2" },
	{ "TB3T", "Battery 3" },
	{ "TBXT", "Battery" },
	{ "Tb0P", "BLC Proximity" },
	/* CPU */
	{ "TC0C", "CPU Core 1" },
	{ "TC0D", "CPU Die" },
	{ "TC0E", "CPU 1" },
	{ "TC0F", "CPU 2" },
	{ "TC0G", "CPU Package GPU" },
	{ "TC0H", "CPU Heatsink" },
	{ "TC0h", "CPU Heatsink" },
	{ "TC0J", "CPU" },
	{ "TC0P", "CPU Proximity" },
	{ "TC0c", "CPU Core 1 PECI" },
	{ "TC0d", "CPU Die PECI" },
	{ "TC0p", "CPU Proximity" },
	{ "TC1C", "CPU Core 2" },
	{ "TC1c", "CPU Core 2 PECI" },
	{ "TC1P", "CPU Proximity 2" },
	{ "TC2C", "CPU Core 3" },
	{ "TC2P", "CPU Proximity 3" },
	{ "TC2c", "CPU Core 3 PECI" },
	{ "TC3C", "CPU Core 4" },
	{ "TC3P", "CPU Proximity 4" },
	{ "TC3c", "CPU Core 4 PECI" },
	{ "TC4C", "CPU Core 5" },
	{ "TC5C", "CPU Core 6" },
	{ "TC6C", "CPU Core 7" },
	{ "TC7C", "CPU Core 8" },
	{ "TC8C", "CPU Core 9" },
	{ "TCGC", "PECI GPU" },
	{ "TCGc", "PECI GPU" },
	{ "TCHP", "Charger Proximity" },
	{ "TCSA", "PECI SA" },
	{ "TCSC", "PECI SA" },
	{ "TCSc", "PECI SA" },
	{ "TCTD", "CPU DTS" },
	{ "TCXC", "PECI CPU" },
	{ "TCXc", "PECI CPU" },
	{ "TCPG", "CPU Package GPU" },
	{ "TCXR", "CPU PECI DTS" },
	/* CPU dual-socket (Mac Pro) */
	{ "TCAG", "CPU A Package" },
	{ "TCAH", "CPU A Heatsink" },
	{ "TCBG", "CPU B Package" },
	{ "TCBH", "CPU B Heatsink" },
	/* GPU */
	{ "TG0C", "GPU Core" },
	{ "TG0D", "GPU Diode" },
	{ "TG0H", "GPU Heatsink" },
	{ "TG0M", "GPU Memory" },
	{ "TG0P", "GPU Proximity" },
	{ "TG0T", "GPU Diode" },
	{ "TG0V", "GPU" },
	{ "TG0d", "GPU Die" },
	{ "TG0h", "GPU Heatsink" },
	{ "TG0p", "GPU Proximity" },
	{ "TGTV", "GPU" },
	{ "TG1D", "GPU 2 Diode" },
	{ "TG1H", "GPU 2 Heatsink" },
	{ "TG1P", "GPU 2 Proximity" },
	{ "TG1d", "GPU 2 Die" },
	{ "TGVP", "GPU Memory Proximity" },
	/* Storage */
	{ "TH0A", "SSD A" },
	{ "TH0B", "SSD B" },
	{ "TH0C", "SSD C" },
	{ "TH0F", "SSD" },
	{ "TH0O", "HDD" },
	{ "TH0P", "HDD Proximity" },
	{ "TH0R", "SSD" },
	{ "TH0V", "SSD" },
	{ "TH0a", "SSD A" },
	{ "TH0b", "SSD B" },
	{ "TH0c", "SSD C" },
	{ "TH1O", "HDD 2" },
	{ "TH1P", "HDD Bay 2" },
	{ "TH2P", "HDD Bay 3" },
	{ "TH3P", "HDD Bay 4" },
	{ "Th0H", "Heatpipe 1" },
	{ "Th0N", "SSD" },
	{ "Th1H", "Heatpipe 2" },
	{ "Th2H", "Heatpipe 3" },
	/* Thunderbolt */
	{ "THSP", "Thunderbolt Proximity" },
	{ "TI0P", "Thunderbolt 1" },
	{ "TI0p", "Thunderbolt 1" },
	{ "TI1P", "Thunderbolt 2" },
	{ "TI1p", "Thunderbolt 2" },
	{ "TTLD", "Thunderbolt Left" },
	{ "TTRD", "Thunderbolt Right" },
	{ "Te0T", "Thunderbolt Diode" },
	{ "Te0t", "Thunderbolt Diode" },
	/* LCD */
	{ "TL0P", "LCD Proximity" },
	{ "TL0V", "LCD" },
	{ "TL0p", "LCD Proximity" },
	{ "TL1P", "LCD Panel 1" },
	{ "TL1V", "LCD 1" },
	{ "TL1p", "LCD Panel 1" },
	{ "TL1v", "LCD 1" },
	{ "TL2V", "LCD 2" },
	{ "TLAV", "LCD" },
	{ "TLBV", "LCD" },
	{ "TLCV", "LCD" },
	/* Memory */
	{ "TM0P", "Memory Proximity" },
	{ "TM0S", "Memory Slot 1" },
	{ "TM0p", "Memory Proximity" },
	{ "TM1P", "Memory Riser A 2" },
	{ "TM1S", "Memory Slot 2" },
	{ "Tm0P", "Memory Proximity" },
	{ "Tm0p", "Memory Proximity" },
	{ "Tm1P", "Memory Proximity 2" },
	{ "TMBS", "Memory Bank" },
	{ "TMCD", "Memory DIMM" },
	/* Northbridge / MCH */
	{ "TN0C", "Northbridge Core" },
	{ "TN0D", "Northbridge Diode" },
	{ "TN0H", "MCH Heatsink" },
	{ "TN0P", "Northbridge Proximity" },
	{ "TN1D", "MCH Die 2" },
	{ "TN1P", "Northbridge Proximity 2" },
	/* PCH */
	{ "TP0P", "PCH Proximity" },
	{ "TP0p", "PCH Proximity" },
	{ "TPCD", "PCH Die" },
	{ "TPCd", "PCH Die" },
	/* Optical drive */
	{ "TO0P", "Optical Drive" },
	{ "TO0p", "Optical Drive" },
	/* Power supply */
	{ "Tp0C", "Power Supply" },
	{ "Tp0P", "Power Supply Proximity" },
	{ "Tp1C", "Power Supply 2" },
	{ "Tp1P", "Power Supply Component" },
	{ "Tp1p", "Power Supply Component" },
	{ "Tp2P", "Power Supply 2" },
	{ "Tp2h", "Power Supply 2" },
	{ "Tp2H", "Power Supply 2" },
	{ "Tp3P", "Power Supply 3 Inlet" },
	{ "Tp3h", "Power Supply 3" },
	{ "Tp3H", "Power Supply 3" },
	{ "Tp4P", "Power Supply 4" },
	{ "Tp5P", "Power Supply 5" },
	/* Palm rest / trackpad */
	{ "Ts0P", "Palm Rest" },
	{ "Ts0S", "Memory Proximity" },
	{ "Ts1P", "Palm Rest 2" },
	{ "Ts1S", "Palm Rest 2" },
	/* Wireless */
	{ "TW0P", "Wireless Proximity" },
	{ "TW0p", "Wireless Proximity" },
	{ "TBLR", "Bluetooth" },
	/* Camera */
	{ "TS2P", "Camera Proximity" },
	{ "TS2V", "Camera" },
	{ "TS2p", "Camera Proximity" },
	/* Expansion */
	{ "TS0C", "Expansion Slots" },
	{ "TS0P", "Expansion Proximity" },
	{ "TS0V", "Expansion" },
	{ "TS0p", "Expansion Proximity" },
	/* Air vent */
	{ "TV0P", "Air Vent" },
	/* VRM */
	{ "Tv0S", "VRM 1" },
	{ "Tv1S", "VRM 2" },
	/* Misc */
	{ "TTF0", "Fan" },
	{ "TMLB", "Logic Board" },
};

static const char *
asmc_temp_desc(const char *key)
{
	unsigned int i;

	for (i = 0; i < nitems(asmc_temp_descs); i++) {
		if (strcmp(asmc_temp_descs[i].key, key) == 0)
			return (asmc_temp_descs[i].desc);
	}
	return ("Temperature");
}

/*
 * Driver methods.
 */
static device_method_t	asmc_methods[] = {
	DEVMETHOD(device_probe,		asmc_probe),
	DEVMETHOD(device_attach,	asmc_attach),
	DEVMETHOD(device_detach,	asmc_detach),
	DEVMETHOD(device_resume,	asmc_resume),

	/* Backlight interface */
	DEVMETHOD(backlight_update_status, asmc_backlight_update_status),
	DEVMETHOD(backlight_get_status, asmc_backlight_get_status),
	DEVMETHOD(backlight_get_info, asmc_backlight_get_info),

	DEVMETHOD_END
};

static driver_t	asmc_driver = {
	"asmc",
	asmc_methods,
	sizeof(struct asmc_softc)
};

/*
 * Debugging
 */
#define	_COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("ASMC")

/* NB: can't be const */
static char *asmc_ids[] = { "APP0001", NULL };

ACPI_PNP_INFO(asmc_ids);
DRIVER_MODULE(asmc, acpi, asmc_driver, NULL, NULL);
MODULE_DEPEND(asmc, acpi, 1, 1, 1);
MODULE_DEPEND(asmc, backlight, 1, 1, 1);

static int
asmc_probe(device_t dev)
{
	char *product;
	int rv;

	if (resource_disabled("asmc", 0))
		return (ENXIO);
	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, asmc_ids, NULL);
	if (rv > 0)
		return (rv);
	product = kern_getenv("smbios.system.product");
	device_set_descf(dev, "Apple %s", product ? product : "SMC");
	freeenv(product);
	return (rv);
}

/*
 * Try MMIO first; the legacy PIO range can be claimable but dead.
 * Fall back to PIO if MMIO probe fails or the resource is absent.
 */
static int
asmc_try_probe(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);

	sc->sc_rid_mem = 0;
	sc->sc_iomem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_rid_mem, RF_ACTIVE);
	if (sc->sc_iomem != NULL) {
		if (asmc_mmio_probe(dev) == 0) {
			sc->sc_is_mmio = true;
			if (bootverbose)
				device_printf(dev, "using MMIO backend\n");
			return (0);
		}
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->sc_rid_mem, sc->sc_iomem);
		sc->sc_iomem = NULL;
	}

	sc->sc_ioport = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
	    &sc->sc_rid_port, RF_ACTIVE);
	if (sc->sc_ioport != NULL)
		return (0);

	device_printf(dev, "unable to allocate IO port or MMIO\n");
	return (ENOMEM);
}

/*
 * The per-fan speed keys, selected by the high byte of a fan sysctl's arg2.
 * Writable keys accept a new speed; the rest are reported only.
 */
#define	ASMC_FAN_ARG2(which, fan)	(((which) << 8) | (fan))
#define	ASMC_FAN_ARG2_WHICH(arg2)	(((arg2) >> 8) & 0xff)
#define	ASMC_FAN_ARG2_FAN(arg2)		((arg2) & 0xff)

#define	ASMC_FANSPEED_CUR	0
#define	ASMC_FANSPEED_SAFE	1
#define	ASMC_FANSPEED_MIN	2
#define	ASMC_FANSPEED_MAX	3
#define	ASMC_FANSPEED_TARGET	4

static const struct {
	const char	*key;
	bool		writable;
} asmc_fan_speed_keys[] = {
	[ASMC_FANSPEED_CUR]	= { ASMC_KEY_FANSPEED,		false },
	[ASMC_FANSPEED_SAFE]	= { ASMC_KEY_FANSAFESPEED,	false },
	[ASMC_FANSPEED_MIN]	= { ASMC_KEY_FANMINSPEED,	true },
	[ASMC_FANSPEED_MAX]	= { ASMC_KEY_FANMAXSPEED,	true },
	[ASMC_FANSPEED_TARGET]	= { ASMC_KEY_FANTARGETSPEED,	true },
};

/* The dev.asmc.n.fan.m leaves, in registration order. */
static const struct {
	const char	*name;
	const char	*desc;
	int		ctlflags;
	int		(*handler)(SYSCTL_HANDLER_ARGS);
	int		which;
	bool		needs_safespeed;
} asmc_fan_sysctls[] = {
	{ "id", "Fan ID", CTLTYPE_STRING | CTLFLAG_RD,
	    asmc_mb_sysctl_fanid, 0, false },
	{ "speed", "Fan speed in RPM", CTLTYPE_INT | CTLFLAG_RD,
	    asmc_mb_sysctl_fanspeed, ASMC_FANSPEED_CUR, false },
	{ "safespeed", "Fan safe speed in RPM", CTLTYPE_INT | CTLFLAG_RD,
	    asmc_mb_sysctl_fanspeed, ASMC_FANSPEED_SAFE, true },
	{ "minspeed", "Fan minimum speed in RPM", CTLTYPE_INT | CTLFLAG_RW,
	    asmc_mb_sysctl_fanspeed, ASMC_FANSPEED_MIN, false },
	{ "maxspeed", "Fan maximum speed in RPM", CTLTYPE_INT | CTLFLAG_RW,
	    asmc_mb_sysctl_fanspeed, ASMC_FANSPEED_MAX, false },
	{ "targetspeed", "Fan target speed in RPM", CTLTYPE_INT | CTLFLAG_RW,
	    asmc_mb_sysctl_fanspeed, ASMC_FANSPEED_TARGET, false },
	{ "manual", "Fan manual mode (0=auto, 1=manual)",
	    CTLTYPE_INT | CTLFLAG_RW, asmc_mb_sysctl_fanmanual, 0, false },
};

static void
asmc_sysctl_add_fan(device_t dev, struct sysctl_ctx_list *ctx)
{
	struct asmc_softc *sc = device_get_softc(dev);
	char name[2];

	sc->sc_fan_tree[0] = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "fan",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Fan Root Tree");

	for (int i = 1; i <= sc->sc_nfan; i++) {
		int j = i - 1;

		name[0] = '0' + j;
		name[1] = 0;
		sc->sc_fan_tree[i] = SYSCTL_ADD_NODE(ctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[0]), OID_AUTO, name,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Fan Subtree");

		for (unsigned int k = 0; k < nitems(asmc_fan_sysctls); k++) {
			if (asmc_fan_sysctls[k].needs_safespeed &&
			    !sc->sc_has_safespeed)
				continue;

			/* SYSCTL_ADD_PROC() asserts flags at compile time. */
			sysctl_add_oid(ctx,
			    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
			    OID_AUTO, asmc_fan_sysctls[k].name,
			    asmc_fan_sysctls[k].ctlflags | CTLFLAG_MPSAFE,
			    dev, ASMC_FAN_ARG2(asmc_fan_sysctls[k].which, j),
			    asmc_fan_sysctls[k].handler, "I",
			    asmc_fan_sysctls[k].desc, NULL);
		}
	}
}

/* The dev.asmc.n.sms axes, selected by arg2. */
static const struct {
	const char	*name;
	const char	*key;
	const char	*desc;
} asmc_sms_sysctls[] = {
	{ "x", ASMC_KEY_SMS_X, "Sudden Motion Sensor X value" },
	{ "y", ASMC_KEY_SMS_Y, "Sudden Motion Sensor Y value" },
	{ "z", ASMC_KEY_SMS_Z, "Sudden Motion Sensor Z value" },
};

static void
asmc_sysctl_add_sms(device_t dev, struct sysctl_ctx_list *ctx)
{
	struct asmc_softc *sc = device_get_softc(dev);

	sc->sc_sms_tree = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "sms",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Sudden Motion Sensor");

	for (unsigned int k = 0; k < nitems(asmc_sms_sysctls); k++) {
		SYSCTL_ADD_PROC(ctx,
		    SYSCTL_CHILDREN(sc->sc_sms_tree),
		    OID_AUTO, asmc_sms_sysctls[k].name,
		    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
		    dev, k, asmc_mb_sysctl_sms, "I",
		    asmc_sms_sysctls[k].desc);
	}
}

/* Every key asmc_sms_init() touches; all must be present for SMS. */
static const char * const asmc_sms_required_keys[] = {
	ASMC_KEY_SMS,
	ASMC_KEY_SMS_X,
	ASMC_KEY_SMS_Y,
	ASMC_KEY_SMS_Z,
	ASMC_KEY_SMS_LOW,
	ASMC_KEY_SMS_HIGH,
	ASMC_KEY_SMS_LOW_INT,
	ASMC_KEY_SMS_HIGH_INT,
	ASMC_KEY_SMS_FLAG,
	ASMC_KEY_INTOK,
};

/* The ambient light sensors, selected by arg2. */
#define	ASMC_LIGHT_LEFT		0
#define	ASMC_LIGHT_RIGHT	1

static const char * const asmc_light_keys[] = {
	[ASMC_LIGHT_LEFT]	= ASMC_KEY_LIGHTLEFT,
	[ASMC_LIGHT_RIGHT]	= ASMC_KEY_LIGHTRIGHT,
};

/* The dev.asmc.n.system leaves, in registration order; len_min 0 = any. */
static const struct {
	const char	*key;
	const char	*name;
	const char	*desc;
	const char	*fmt;
	int		ctlflags;
	int		arg2;
	int		(*handler)(SYSCTL_HANDLER_ARGS);
	uint8_t		len_min;
	uint8_t		len_max;
} asmc_system_sysctls[] = {
	{ ASMC_KEY_MSSD, "shutdown_cause", "Last shutdown cause (MSSD)",
	    "A", CTLTYPE_STRING | CTLFLAG_RD, 0, asmc_cause_sysctl, 0, 0 },
	{ ASMC_KEY_MSSP, "sleep_cause", "Last sleep cause (MSSP)",
	    "A", CTLTYPE_STRING | CTLFLAG_RD, 1, asmc_cause_sysctl, 0, 0 },
	{ ASMC_KEY_MSAL, "thermal_status",
	    "Thermal subsystem status flags (MSAL)",
	    "A", CTLTYPE_STRING | CTLFLAG_RD, 0, asmc_msal_sysctl, 0, 0 },
	{ ASMC_KEY_CLKT, "time_of_day",
	    "Seconds since midnight per SMC clock (CLKT)",
	    "IU", CTLTYPE_UINT | CTLFLAG_RD, 0, asmc_clkt_sysctl, 0, 0 },
	{ ASMC_KEY_MSPS, "power_state", "SMC power state index (MSPS)",
	    "IU", CTLTYPE_UINT | CTLFLAG_RD, 0, asmc_msps_sysctl, 1, 2 },
	{ ASMC_KEY_RPLT, "board_id", "Apple internal board codename (RPlt)",
	    "A", CTLTYPE_STRING | CTLFLAG_RD, 0, asmc_rplt_sysctl, 0, 0 },
	{ ASMC_KEY_RGEN, "chip_gen",
	    "Apple security chip generation (RGEN; 3=T2)",
	    "IU", CTLTYPE_UINT | CTLFLAG_RD, 0, asmc_rgen_sysctl, 0, 0 },
};

static int
asmc_sysctl_add_system(device_t dev, struct sysctl_ctx_list *ctx)
{
	struct sysctl_oid *sys_tree;

	sys_tree = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "system", CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
	    "System state and board identity");
	if (sys_tree == NULL) {
		device_printf(dev, "failed to create system sysctl node\n");
		return (ENXIO);
	}

	for (unsigned int k = 0; k < nitems(asmc_system_sysctls); k++) {
		uint8_t len = 0;

		if (asmc_key_getinfo(dev, asmc_system_sysctls[k].key,
		    &len, NULL) != 0)
			continue;
		if (asmc_system_sysctls[k].len_min != 0 &&
		    (len < asmc_system_sysctls[k].len_min ||
		    len > asmc_system_sysctls[k].len_max))
			continue;

		/* SYSCTL_ADD_PROC() asserts flags at compile time. */
		sysctl_add_oid(ctx, SYSCTL_CHILDREN(sys_tree),
		    OID_AUTO, asmc_system_sysctls[k].name,
		    asmc_system_sysctls[k].ctlflags | CTLFLAG_MPSAFE,
		    dev, asmc_system_sysctls[k].arg2,
		    asmc_system_sysctls[k].handler,
		    asmc_system_sysctls[k].fmt,
		    asmc_system_sysctls[k].desc, NULL);
	}

	return (0);
}

/* Platform toggles under dev.asmc.n, each present only if its key reads. */
static const struct {
	const char	*key;
	const char	*name;
	const char	*desc;
	int		(*handler)(SYSCTL_HANDLER_ARGS);
} asmc_platform_sysctls[] = {
	{ ASMC_KEY_AUPO, "auto_poweron",
	    "Auto power-on after AC power loss (0=off, 1=on)",
	    asmc_aupo_sysctl },
	{ ASMC_KEY_MSLD, "sil", "Sleep indicator LED (0=off, 1=on)",
	    asmc_sil_sysctl },
};

static void
asmc_sysctl_add_platform(device_t dev, struct sysctl_ctx_list *ctx)
{
	uint8_t buf[1];

	for (unsigned int k = 0; k < nitems(asmc_platform_sysctls); k++) {
		if (asmc_key_read(dev, asmc_platform_sysctls[k].key,
		    buf, 1) != 0)
			continue;

		SYSCTL_ADD_PROC(ctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, asmc_platform_sysctls[k].name,
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
		    dev, 0, asmc_platform_sysctls[k].handler, "I",
		    asmc_platform_sysctls[k].desc);
	}
}

static int
asmc_attach(device_t dev)
{
	int ret;
	struct asmc_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	struct sysctl_oid *sysctlnode;

	ret = asmc_try_probe(dev);
	if (ret != 0)
		goto err;

	sysctlctx = device_get_sysctl_ctx(dev);
	sysctlnode = device_get_sysctl_tree(dev);

	/* Mutex may already be initialized by asmc_mmio_probe() */
	if (!mtx_initialized(&sc->sc_mtx))
		mtx_init(&sc->sc_mtx, "asmc", NULL, MTX_SPIN);

	/* Read SMC revision, key count, fan count */
	ret = asmc_init(dev);
	if (ret != 0) {
		device_printf(dev, "SMC not responding\n");
		goto err;
	}

	/* Probe SMC keys to detect capabilities */
	asmc_detect_capabilities(dev);

	/* Auto-detect and register voltage/current/power/ambient/temp sensors */
	asmc_detect_sensors(dev);

	asmc_sysctl_add_fan(dev, sysctlctx);

	/*
	 * dev.asmc.n.temp tree.
	 */
	sc->sc_temp_tree = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode), OID_AUTO, "temp",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Temperature sensors");

	for (int i = 0; i < sc->sc_temp_count; i++) {
		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_temp_tree),
		    OID_AUTO, sc->sc_temp_sensors[i],
		    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, i,
		    asmc_temp_sysctl, "I",
		    asmc_temp_desc(sc->sc_temp_sensors[i]));
	}

	/*
	 * dev.asmc.n.light
	 */
	if (sc->sc_has_light) {
		sc->sc_light_tree = SYSCTL_ADD_NODE(sysctlctx,
		    SYSCTL_CHILDREN(sysctlnode), OID_AUTO, "light",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
		    "Keyboard backlight sensors");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_light_tree),
		    OID_AUTO, "left",
		    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
		    dev, ASMC_LIGHT_LEFT,
		    sc->sc_light_len == ASMC_LIGHT_LONGLEN ?
		        asmc_mbp_sysctl_light_left_10byte :
		        asmc_mbp_sysctl_light,
		    "I", "Keyboard backlight left sensor");

		if (sc->sc_light_len != ASMC_LIGHT_LONGLEN &&
		    asmc_key_getinfo(dev, ASMC_KEY_LIGHTRIGHT,
		    NULL, NULL) == 0) {
			SYSCTL_ADD_PROC(sysctlctx,
			    SYSCTL_CHILDREN(sc->sc_light_tree),
			    OID_AUTO, "right",
			    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
			    dev, ASMC_LIGHT_RIGHT,
			    asmc_mbp_sysctl_light, "I",
			    "Keyboard backlight right sensor");
		}

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_light_tree),
		    OID_AUTO, "control",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY | CTLFLAG_MPSAFE,
		    dev, 0, asmc_mbp_sysctl_light_control, "I",
		    "Keyboard backlight brightness control");

		sc->sc_kbd_bkl = backlight_register("asmc", dev);
		if (sc->sc_kbd_bkl == NULL) {
			device_printf(dev, "Can not register backlight\n");
			ret = ENXIO;
			goto err;
		}
	}

	asmc_debug_sysctl_add(dev, sysctlctx);

	/*
	 * Battery charge limit (T2 Macs).
	 */
	if (sc->sc_is_t2 &&
	    asmc_key_getinfo(dev, ASMC_KEY_BCLM, NULL, NULL) == 0) {
		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sysctlnode), OID_AUTO, "battery_charge_limit",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
		    dev, 0, asmc_bclm_sysctl, "I",
		    "Battery charge limit (0-100)");
	}

	/* System state / board identity subtree. */
	if (asmc_sysctl_add_system(dev, sysctlctx) != 0)
		goto nosms;

	if (asmc_key_getinfo(dev, ASMC_KEY_MBSS, NULL, NULL) == 0)
		sc->sc_shutdown_tag = EVENTHANDLER_REGISTER(shutdown_pre_sync,
		    asmc_shutdown, dev, SHUTDOWN_PRI_FIRST);

	if (!sc->sc_has_sms)
		goto nosms;

	/*
	 * Initialize SMS hardware.
	 */
	asmc_sms_init(dev);

	asmc_sysctl_add_sms(dev, sysctlctx);

	/*
	 * Need a taskqueue to send devctl_notify() events
	 * when the SMS interrupt us.
	 *
	 * PI_REALTIME is used due to the sensitivity of the
	 * interrupt. An interrupt from the SMS means that the
	 * disk heads should be turned off as quickly as possible.
	 *
	 * We only need to do this for the non INTR_FILTER case.
	 */
	sc->sc_sms_tq = NULL;
	TASK_INIT(&sc->sc_sms_task, 0, asmc_sms_task, sc);
	sc->sc_sms_tq = taskqueue_create_fast("asmc_taskq", M_WAITOK,
	    taskqueue_thread_enqueue, &sc->sc_sms_tq);
	taskqueue_start_threads(&sc->sc_sms_tq, 1, PI_REALTIME, "%s sms taskq",
	    device_get_nameunit(dev));
	/*
	 * Allocate an IRQ for the SMS.
	 */
	sc->sc_rid_irq = 0;
	sc->sc_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->sc_rid_irq,
	    RF_ACTIVE);
	if (sc->sc_irq == NULL) {
		device_printf(dev, "unable to allocate IRQ resource\n");
		ret = ENXIO;
		goto err;
	}

	ret = bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_MISC | INTR_MPSAFE,
	    asmc_sms_intrfast, NULL, dev, &sc->sc_cookie);
	if (ret) {
		device_printf(dev, "unable to setup SMS IRQ\n");
		goto err;
	}

nosms:
	return (0);

err:
	asmc_detach(dev);

	return (ret);
}

static int
asmc_detach(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);

	if (sc->sc_shutdown_tag != NULL) {
		EVENTHANDLER_DEREGISTER(shutdown_pre_sync, sc->sc_shutdown_tag);
		sc->sc_shutdown_tag = NULL;
	}

	if (sc->sc_kbd_bkl != NULL)
		backlight_destroy(sc->sc_kbd_bkl);

	/* Free temperature sensor key arrays */
	for (int i = 0; i < sc->sc_temp_count; i++)
		free(sc->sc_temp_sensors[i], M_DEVBUF);

	/* Free sensor key arrays */
	for (int i = 0; i < sc->sc_voltage_count; i++)
		free(sc->sc_voltage_sensors[i], M_DEVBUF);
	for (int i = 0; i < sc->sc_current_count; i++)
		free(sc->sc_current_sensors[i], M_DEVBUF);
	for (int i = 0; i < sc->sc_power_count; i++)
		free(sc->sc_power_sensors[i], M_DEVBUF);
	for (int i = 0; i < sc->sc_light_count; i++)
		free(sc->sc_light_sensors[i], M_DEVBUF);

	if (sc->sc_sms_tq) {
		taskqueue_drain(sc->sc_sms_tq, &sc->sc_sms_task);
		taskqueue_free(sc->sc_sms_tq);
		sc->sc_sms_tq = NULL;
	}
	if (sc->sc_cookie) {
		bus_teardown_intr(dev, sc->sc_irq, sc->sc_cookie);
		sc->sc_cookie = NULL;
	}
	if (sc->sc_irq) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_rid_irq,
		    sc->sc_irq);
		sc->sc_irq = NULL;
	}
	if (sc->sc_ioport) {
		bus_release_resource(dev, SYS_RES_IOPORT, sc->sc_rid_port,
		    sc->sc_ioport);
		sc->sc_ioport = NULL;
	}
	asmc_mmio_detach(dev, sc);
	if (mtx_initialized(&sc->sc_mtx)) {
		mtx_destroy(&sc->sc_mtx);
	}

	return (0);
}

static int
asmc_resume(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t buf[2];

	buf[0] = sc->sc_light_control;
	buf[1] = 0x00;
	asmc_key_write(dev, ASMC_KEY_LIGHTVALUE, buf, sizeof(buf));

	return (0);
}

static void
asmc_shutdown(void *arg, int howto)
{
	device_t dev = (device_t)arg;
	uint8_t buf[2];

	be16enc(buf, (howto & (RB_POWEROFF | RB_HALT)) != 0 ?
	    ASMC_MBSS_POWEROFF : ASMC_MBSS_RESTART);
	asmc_key_write(dev, ASMC_KEY_MBSS, buf, sizeof(buf));
}

/*
 * Initialize SMC: read revision, key count, fan count.
 * SMS initialization is handled separately in asmc_sms_init().
 */
static int
asmc_init(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	uint8_t buf[6];
	int error;

	sysctlctx = device_get_sysctl_ctx(dev);

	error = asmc_key_read(dev, ASMC_KEY_REV, buf, 6);
	if (error != 0) {
		/*
		 * Could not read REV key; T2 Macs may not have it.
		 * Use #KEY as a liveness check instead.
		 */
		if (sc->sc_is_t2) {
			error = asmc_key_read(dev, ASMC_NKEYS, buf, 4);
			if (error != 0)
				goto out;
			device_printf(dev, "T2 SMC: %d keys\n",
			    be32dec(buf));
		} else {
			goto out;
		}
	} else {
		device_printf(dev, "SMC revision: %x.%x%x%x\n",
		    buf[0], buf[1], buf[2],
		    ntohs(*(uint16_t *)buf + 4));
	}

	asmc_sysctl_add_platform(dev, sysctlctx);

	sc->sc_nfan = asmc_fan_count(dev);
	if (sc->sc_nfan > ASMC_MAXFANS) {
		device_printf(dev,
		    "more than %d fans were detected. Please report this.\n",
		    ASMC_MAXFANS);
		sc->sc_nfan = ASMC_MAXFANS;
	}

	/*
	 * Read and cache the number of SMC keys (32 bit buffer)
	 */
	if (asmc_key_read(dev, ASMC_NKEYS, buf, 4) == 0) {
		sc->sc_nkeys = be32dec(buf);
		if (bootverbose)
			device_printf(dev, "number of keys: %d\n",
			    sc->sc_nkeys);
	} else {
		sc->sc_nkeys = 0;
	}

out:
	asmc_dumpall(dev);
	return (error);
}

/*
 * Initialize the Sudden Motion Sensor hardware.
 * Called from asmc_attach() after capabilities are detected.
 */
static void
asmc_sms_init(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t buf[2];
	int i;

	/*
	 * We are ready to receive interrupts from the SMS.
	 */
	buf[0] = 0x01;
	ASMC_DPRINTF(("intok key\n"));
	asmc_key_write(dev, ASMC_KEY_INTOK, buf, 1);
	DELAY(50);

	/*
	 * Initiate the polling intervals.
	 */
	buf[0] = 20; /* msecs */
	ASMC_DPRINTF(("low int key\n"));
	asmc_key_write(dev, ASMC_KEY_SMS_LOW_INT, buf, 1);
	DELAY(200);

	buf[0] = 20; /* msecs */
	ASMC_DPRINTF(("high int key\n"));
	asmc_key_write(dev, ASMC_KEY_SMS_HIGH_INT, buf, 1);
	DELAY(200);

	buf[0] = 0x00;
	buf[1] = 0x60;
	ASMC_DPRINTF(("sms low key\n"));
	asmc_key_write(dev, ASMC_KEY_SMS_LOW, buf, 2);
	DELAY(200);

	buf[0] = 0x01;
	buf[1] = 0xc0;
	ASMC_DPRINTF(("sms high key\n"));
	asmc_key_write(dev, ASMC_KEY_SMS_HIGH, buf, 2);
	DELAY(200);

	/*
	 * I'm not sure what this key does, but it seems to be
	 * required.
	 */
	buf[0] = 0x01;
	ASMC_DPRINTF(("sms flag key\n"));
	asmc_key_write(dev, ASMC_KEY_SMS_FLAG, buf, 1);
	DELAY(100);

	sc->sc_sms_intr_works = 0;

	/*
	 * Retry SMS initialization 1000 times
	 * (takes approx. 2 seconds in worst case)
	 */
	for (i = 0; i < 1000; i++) {
		if (asmc_key_read(dev, ASMC_KEY_SMS, buf, 2) == 0 &&
		    (buf[0] == ASMC_SMS_INIT1 && buf[1] == ASMC_SMS_INIT2)) {
			sc->sc_sms_intr_works = 1;
			goto done;
		}
		buf[0] = ASMC_SMS_INIT1;
		buf[1] = ASMC_SMS_INIT2;
		ASMC_DPRINTF(("sms key\n"));
		asmc_key_write(dev, ASMC_KEY_SMS, buf, 2);
		DELAY(50);
	}
	device_printf(dev, "WARNING: Sudden Motion Sensor not initialized!\n");

done:
	asmc_sms_calibrate(dev);
}

/*
 * Probe SMC keys to detect hardware capabilities.
 */
static void
asmc_detect_capabilities(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t len;
	char type[ASMC_TYPELEN + 1];

	/* SMS: require all keys used by asmc_sms_init() */
	sc->sc_has_sms = 1;
	for (unsigned int k = 0; k < nitems(asmc_sms_required_keys); k++) {
		if (asmc_key_getinfo(dev, asmc_sms_required_keys[k],
		    &len, type) != 0) {
			sc->sc_has_sms = 0;
			break;
		}
	}

	/* Light sensor: require ALV0 (len 6 or 10) and LKSB */
	if (asmc_key_getinfo(dev, ASMC_KEY_LIGHTLEFT,
	    &len, type) == 0 &&
	    (len == ASMC_LIGHT_SHORTLEN || len == ASMC_LIGHT_LONGLEN) &&
	    asmc_key_getinfo(dev, ASMC_KEY_LIGHTVALUE,
	    NULL, NULL) == 0) {
		sc->sc_has_light = 1;
		sc->sc_light_len = len;
	} else {
		sc->sc_has_light = 0;
		sc->sc_light_len = 0;
	}

	/* Fan safe speed */
	sc->sc_has_safespeed =
	    (asmc_key_getinfo(dev, ASMC_KEY_FANSAFESPEED0,
	    &len, type) == 0);

	/* Ambient light interrupt source */
	sc->sc_has_alsl =
	    (asmc_key_getinfo(dev, ASMC_KEY_LIGHTSRC,
	    &len, type) == 0);

	if (bootverbose)
		device_printf(dev,
		    "capabilities: sms=%d light=%d (len=%d) safespeed=%d alsl=%d\n",
		    sc->sc_has_sms, sc->sc_has_light, sc->sc_light_len,
		    sc->sc_has_safespeed, sc->sc_has_alsl);
}

int
asmc_key_read(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	struct asmc_softc *sc = device_get_softc(dev);

	if (sc->sc_is_mmio)
		return (asmc_mmio_key_read(dev, key, buf, len));

	return (asmc_pio_key_read(dev, key, buf, len));
}

int
asmc_key_getinfo(device_t dev, const char *key, uint8_t *len, char *type)
{
	struct asmc_softc *sc = device_get_softc(dev);

	if (sc->sc_is_mmio)
		return (asmc_mmio_key_getinfo(dev, key, len, type));

	return (asmc_pio_key_getinfo(dev, key, len, type));
}

/* SMC sensor type table: type string to fixed-point divisor. */
static const struct {
	const char	type[5];
	int		divisor;
} asmc_sensor_types[] = {
	{ "sp78",  256 },
	{ "sp87",  128 },
	{ "sp4b", 2048 },
	{ "sp5a", 1024 },
	{ "sp69",  512 },
	{ "sp96",   64 },
	{ "sp2d", 8192 },
	{ "ui16",    1 },
	{ "",        0 },
};

/* Convert a 2-byte SMC value to milli-units. */
static bool
asmc_sensor_convert(const char *type, const uint8_t *buf, int *millivalue)
{
	int i;

	for (i = 0; asmc_sensor_types[i].divisor != 0; i++) {
		if (strncmp(type, asmc_sensor_types[i].type, 4) != 0)
			continue;
		if (asmc_sensor_types[i].divisor == 1)
			*millivalue = be16dec(buf);
		else
			*millivalue = ((int)(int16_t)be16dec(buf) * 1000) /
			    asmc_sensor_types[i].divisor;
		return (true);
	}
	return (false);
}

static bool
asmc_sensor_type_supported(const char *type)
{
	int i;

	for (i = 0; asmc_sensor_types[i].divisor != 0; i++)
		if (strncmp(type, asmc_sensor_types[i].type, 4) == 0)
			return (true);
	return (false);
}

/*
 * Generic sensor value reader with automatic type conversion.
 * Reads an SMC key, detects its type, and converts to millivalue.
 */
static int
asmc_sensor_read(device_t dev, const char *key, int *millivalue)
{
	uint8_t buf[2];
	char type[ASMC_TYPELEN + 1];
	uint8_t len;
	int error;

	error = asmc_key_getinfo(dev, key, &len, type);
	if (error != 0)
		return (error);

	if (len != 2) {
		if (bootverbose)
			device_printf(dev,
			    "%s: key %s unexpected length %d\n",
			    __func__, key, len);
		return (ENXIO);
	}

	error = asmc_key_read(dev, key, buf, sizeof(buf));
	if (error != 0)
		return (error);

	if (!asmc_sensor_convert(type, buf, millivalue)) {
		if (bootverbose)
			device_printf(dev,
			    "%s: unknown type '%s' for key %s\n",
			    __func__, type, key);
		return (ENXIO);
	}

	return (0);
}

/*
 * Generic sensor sysctl handler for voltage/current/power/light sensors.
 * arg2 encodes: sensor_type (high byte) | sensor_index (low byte)
 * Sensor types: 'V'=voltage, 'I'=current, 'P'=power, 'L'=light
 */
static int
asmc_sensor_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	struct asmc_softc *sc = device_get_softc(dev);
	int error, val;
	int sensor_type = (arg2 >> 8) & 0xFF;
	int sensor_idx = arg2 & 0xFF;
	const char *key = NULL;

	/* Select sensor based on type and index */
	switch (sensor_type) {
	case 'V':  /* Voltage */
		if (sensor_idx < sc->sc_voltage_count)
			key = sc->sc_voltage_sensors[sensor_idx];
		break;
	case 'I':  /* Current */
		if (sensor_idx < sc->sc_current_count)
			key = sc->sc_current_sensors[sensor_idx];
		break;
	case 'P':  /* Power */
		if (sensor_idx < sc->sc_power_count)
			key = sc->sc_power_sensors[sensor_idx];
		break;
	case 'L':  /* Light */
		if (sensor_idx < sc->sc_light_count)
			key = sc->sc_light_sensors[sensor_idx];
		break;
	default:
		return (EINVAL);
	}

	if (key == NULL)
		return (ENOENT);

	error = asmc_sensor_read(dev, key, &val);
	if (error != 0)
		return (error);

	return (sysctl_handle_int(oidp, &val, 0, req));
}

static bool
asmc_match_sp78(const char *key __unused, const char *type)
{

	return (strncmp(type, "sp78", 4) == 0);
}

static bool
asmc_match_supported(const char *key __unused, const char *type)
{

	return (asmc_sensor_type_supported(type));
}

static bool
asmc_match_light(const char *key, const char *type)
{

	return (key[1] == 'L' && (key[2] == 'V' || key[2] == 'S') &&
	    asmc_sensor_type_supported(type));
}

/* Scan key indices for 2-byte keys passing prefix (if set) and match. */
static void
asmc_scan_sensor_range(device_t dev, unsigned int start,
    unsigned int end, char prefix,
    bool (*match)(const char *key, const char *type), int *countp,
    char **sensors, int maxcount)
{
	char key[ASMC_KEYLEN + 1];
	char type[ASMC_TYPELEN + 1];
	uint8_t len;
	char *sensor_key;

	for (unsigned int i = start; i < end; i++) {
		if (asmc_key_dump_by_index(dev, i, key, type, &len))
			continue;
		if (len != 2 || (prefix != '\0' && key[0] != prefix))
			continue;
		if (!match(key, type))
			continue;
		if (*countp >= maxcount)
			break;
		sensor_key = malloc(ASMC_KEYLEN + 1,
		    M_DEVBUF, M_WAITOK);
		memcpy(sensor_key, key, ASMC_KEYLEN + 1);
		sensors[(*countp)++] = sensor_key;
	}
}

static int
asmc_detect_sensors(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	struct sysctl_oid *tree_node;
	unsigned int start, end, i;
	int error;

	sc->sc_voltage_count = 0;
	sc->sc_current_count = 0;
	sc->sc_power_count = 0;
	sc->sc_light_count = 0;
	sc->sc_temp_count = 0;

	if (sc->sc_nkeys == 0)
		return (0);

	/*
	 * Temperature sensors: binary search for T..U range,
	 * then filter by type sp78.
	 */
	error = asmc_key_search(dev, "T\0\0\0", &start);
	if (error == 0)
		error = asmc_key_search(dev, "U\0\0\0", &end);
	if (error == 0)
		asmc_scan_sensor_range(dev, start, end, '\0', asmc_match_sp78,
		    &sc->sc_temp_count, sc->sc_temp_sensors, ASMC_TEMP_MAX);

	/* Voltage/Current/Power sensors */
	static const struct {
		const char	*range_start;
		const char	*range_end;
		char		prefix;
	} sensor_ranges[] = {
		{ "V\0\0\0", "W\0\0\0", 'V' },	/* Voltage */
		{ "I\0\0\0", "J\0\0\0", 'I' },	/* Current */
		{ "P\0\0\0", "Q\0\0\0", 'P' },	/* Power */
	};
	static const size_t nsensor_ranges = nitems(sensor_ranges);

	int *sensor_counts[] = {
	    &sc->sc_voltage_count, &sc->sc_current_count,
	    &sc->sc_power_count };
	char **sensor_arrays[] = {
	    sc->sc_voltage_sensors, sc->sc_current_sensors,
	    sc->sc_power_sensors };

	for (unsigned int r = 0; r < nsensor_ranges; r++) {
		error = asmc_key_search(dev, sensor_ranges[r].range_start,
		    &start);
		if (error == 0)
			error = asmc_key_search(dev,
			    sensor_ranges[r].range_end, &end);
		if (error == 0)
			asmc_scan_sensor_range(dev, start, end,
			    sensor_ranges[r].prefix, asmc_match_supported,
			    sensor_counts[r], sensor_arrays[r],
			    ASMC_MAX_SENSORS);
	}

	/* Ambient light sensors: AL* in A..B range */
	error = asmc_key_search(dev, "A\0\0\0", &start);
	if (error == 0)
		error = asmc_key_search(dev, "B\0\0\0", &end);
	if (error == 0)
		asmc_scan_sensor_range(dev, start, end, 'A', asmc_match_light,
		    &sc->sc_light_count, sc->sc_light_sensors,
		    ASMC_MAX_SENSORS);

	if (bootverbose)
		device_printf(dev,
		    "detected %d temp, %d voltage, %d current, "
		    "%d power, %d light sensors\n",
		    sc->sc_temp_count, sc->sc_voltage_count,
		    sc->sc_current_count,
		    sc->sc_power_count, sc->sc_light_count);

	/* Register sysctls for detected sensors */
	sysctlctx = device_get_sysctl_ctx(dev);

	static const struct {
		const char	*node_name;
		const char	*node_desc;
		char		tag;
		const char	*leaf_desc;
	} sensor_sysctl[] = {
		{ "voltage", "Voltage sensors (millivolts)",  'V',
		    "Voltage sensor (millivolts)" },
		{ "current", "Current sensors (milliamps)",   'I',
		    "Current sensor (milliamps)" },
		{ "power",   "Power sensors (milliwatts)",    'P',
		    "Power sensor (milliwatts)" },
		{ "ambient", "Ambient light sensors",         'L',
		    "Light sensor value" },
	};

	int *sysctl_counts[] = {
	    &sc->sc_voltage_count, &sc->sc_current_count,
	    &sc->sc_power_count, &sc->sc_light_count };
	char **sysctl_arrays[] = {
	    sc->sc_voltage_sensors, sc->sc_current_sensors,
	    sc->sc_power_sensors, sc->sc_light_sensors };

	for (unsigned int s = 0; s < nitems(sensor_sysctl); s++) {
		int count = *sysctl_counts[s];
		if (count <= 0)
			continue;
		tree_node = SYSCTL_ADD_NODE(sysctlctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    sensor_sysctl[s].node_name,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
		    sensor_sysctl[s].node_desc);
		for (i = 0; i < count; i++) {
			SYSCTL_ADD_PROC(sysctlctx,
			    SYSCTL_CHILDREN(tree_node),
			    OID_AUTO, sysctl_arrays[s][i],
			    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
			    dev, (sensor_sysctl[s].tag << 8) | i,
			    asmc_sensor_sysctl, "I",
			    sensor_sysctl[s].leaf_desc);
		}
	}

	return (0);
}

/*
 * Helper function to get key info by index (for sensor detection).
 */
int
asmc_key_dump_by_index(device_t dev, int index, char *key_out,
    char *type_out, uint8_t *len_out)
{
	struct asmc_softc *sc = device_get_softc(dev);
	int error;

	if (sc->sc_is_mmio) {
		error = asmc_mmio_key_getbyindex(dev, index, key_out);
		if (error != 0)
			return (error);
		return (asmc_mmio_key_getinfo(dev, key_out, len_out,
		    type_out));
	}

	return (asmc_pio_key_dump_by_index(dev, index, key_out, type_out,
	    len_out));
}

/*
 * Binary search for the first key index >= prefix.
 * SMC keys are sorted, so this finds the lower bound efficiently.
 */
static int
asmc_key_search(device_t dev, const char *prefix, unsigned int *idx)
{
	struct asmc_softc *sc = device_get_softc(dev);
	unsigned int lo, hi, mid;
	char key[ASMC_KEYLEN + 1];
	char type[ASMC_TYPELEN + 1];
	uint8_t len;
	int error;

	lo = 0;
	hi = sc->sc_nkeys;
	while (lo < hi) {
		mid = lo + (hi - lo) / 2;
		error = asmc_key_dump_by_index(dev, mid,
		    key, type, &len);
		if (error != 0)
			return (error);
		if (strncmp(key, prefix, ASMC_KEYLEN) < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	*idx = lo;
	return (0);
}

int
asmc_key_write(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	struct asmc_softc *sc = device_get_softc(dev);

	if (sc->sc_is_mmio)
		return (asmc_mmio_key_write(dev, key, buf, len));

	return (asmc_pio_key_write(dev, key, buf, len));
}

/*
 * Fan control functions.
 */
static int
asmc_fan_count(device_t dev)
{
	uint8_t buf[1];

	if (asmc_key_read(dev, ASMC_KEY_FANCOUNT, buf, sizeof(buf)) != 0)
		return (-1);

	return (buf[0]);
}

static int
asmc_fan_getvalue(device_t dev, const char *key, int fan)
{
	struct asmc_softc *sc = device_get_softc(dev);
	int speed;
	uint8_t buf[4];
	char fankey[5];
	char type[ASMC_TYPELEN + 1];

	snprintf(fankey, sizeof(fankey), key, fan);

	/*
	 * T2 Macs use IEEE 754 float ("flt ") for fan speeds,
	 * stored little-endian in the MMIO data register.
	 * Standard Macs use s14.2 fixed-point ("fpe2", 2 bytes).
	 */
	if (sc->sc_is_t2 &&
	    asmc_key_getinfo(dev, fankey, NULL, type) == 0 &&
	    strncmp(type, "flt ", 4) == 0) {
		if (asmc_key_read(dev, fankey, buf, 4) != 0)
			return (-1);
		speed = (int)asmc_float_to_u32(le32dec(buf));
	} else {
		if (asmc_key_read(dev, fankey, buf, 2) != 0)
			return (-1);
		speed = (buf[0] << 6) | (buf[1] >> 2);
	}

	return (speed);
}

static char *
asmc_fan_getstring(device_t dev, const char *key, int fan, uint8_t *buf,
    uint8_t buflen)
{
	char fankey[5];
	char *desc;

	snprintf(fankey, sizeof(fankey), key, fan);
	if (asmc_key_read(dev, fankey, buf, buflen) != 0)
		return (NULL);
	desc = buf + 4;

	return (desc);
}

static int
asmc_fan_setvalue(device_t dev, const char *key, int fan, int speed)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t buf[4];
	char fankey[5];
	char type[ASMC_TYPELEN + 1];

	snprintf(fankey, sizeof(fankey), key, fan);

	if (sc->sc_is_t2 &&
	    asmc_key_getinfo(dev, fankey, NULL, type) == 0 &&
	    strncmp(type, "flt ", 4) == 0) {
		uint32_t fval;
		speed = MAX(speed, 0);
		speed = MIN(speed, 65535);
		fval = asmc_u32_to_float((uint32_t)speed);
		le32enc(buf, fval);
		if (asmc_key_write(dev, fankey, buf, 4) != 0)
			return (-1);
	} else {
		speed *= 4;
		buf[0] = speed >> 8;
		buf[1] = speed;
		if (asmc_key_write(dev, fankey, buf, 2) != 0)
			return (-1);
	}

	return (0);
}

static int
asmc_mb_sysctl_fanspeed(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int which = ASMC_FAN_ARG2_WHICH(arg2);
	int fan = ASMC_FAN_ARG2_FAN(arg2);
	int error;
	int32_t v;

	v = asmc_fan_getvalue(dev, asmc_fan_speed_keys[which].key, fan);
	error = sysctl_handle_int(oidp, &v, 0, req);

	if (error == 0 && req->newptr != NULL &&
	    asmc_fan_speed_keys[which].writable) {
		unsigned int newspeed = v;
		asmc_fan_setvalue(dev, asmc_fan_speed_keys[which].key, fan,
		    newspeed);
	}

	return (error);
}

static int
asmc_mb_sysctl_fanid(SYSCTL_HANDLER_ARGS)
{
	uint8_t buf[16];
	device_t dev = (device_t)arg1;
	int fan = ASMC_FAN_ARG2_FAN(arg2);
	int error = true;
	char *desc;

	desc = asmc_fan_getstring(dev, ASMC_KEY_FANID, fan, buf, sizeof(buf));

	if (desc != NULL)
		error = sysctl_handle_string(oidp, desc, 0, req);

	return (error);
}

static int
asmc_mb_sysctl_fanmanual(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	struct asmc_softc *sc = device_get_softc(dev);
	int fan = ASMC_FAN_ARG2_FAN(arg2);
	int error;
	int32_t v;
	uint8_t buf[2];
	uint16_t val;
	char fmkey[5];

	/*
	 * T2 Macs use per-fan F%dMd keys (1 byte each).
	 * Standard Macs use FS! bitmask (2 bytes).
	 */
	snprintf(fmkey, sizeof(fmkey), ASMC_KEY_FANMANUAL_T2, fan);
	if (sc->sc_is_t2 &&
	    asmc_key_getinfo(dev, fmkey, NULL, NULL) == 0) {
		error = asmc_key_read(dev, fmkey, buf, 1);
		if (error != 0)
			return (error);
		v = buf[0] ? 1 : 0;

		error = sysctl_handle_int(oidp, &v, 0, req);
		if (error == 0 && req->newptr != NULL) {
			if (v != 0 && v != 1)
				return (EINVAL);
			buf[0] = (uint8_t)v;
			error = asmc_key_write(dev, fmkey, buf, 1);
		}
		return (error);
	}

	/* Read current FS! bitmask (asmc_key_read locks internally) */
	error = asmc_key_read(dev, ASMC_KEY_FANMANUAL, buf, sizeof(buf));
	if (error != 0)
		return (error);

	/* Extract manual bit for this fan (big-endian) */
	val = (buf[0] << 8) | buf[1];
	v = (val >> fan) & 0x01;

	/* Let sysctl handle the value */
	error = sysctl_handle_int(oidp, &v, 0, req);

	if (error == 0 && req->newptr != NULL) {
		/* Validate input (0 = auto, 1 = manual) */
		if (v != 0 && v != 1)
			return (EINVAL);
		/* Read-modify-write of FS! bitmask */
		error = asmc_key_read(dev, ASMC_KEY_FANMANUAL, buf,
		    sizeof(buf));
		if (error == 0) {
			val = (buf[0] << 8) | buf[1];

			/* Modify single bit */
			if (v)
				val |= (1 << fan);   /* Set to manual */
			else
				val &= ~(1 << fan);  /* Set to auto */

			/* Write back */
			buf[0] = val >> 8;
			buf[1] = val & 0xff;
			error = asmc_key_write(dev, ASMC_KEY_FANMANUAL, buf,
			    sizeof(buf));
		}
	}

	return (error);
}

/*
 * Temperature functions.
 */
static int
asmc_temp_getvalue(device_t dev, const char *key)
{
	uint8_t buf[2];

	/*
	 * Check for invalid temperatures.
	 */
	if (asmc_key_read(dev, key, buf, sizeof(buf)) != 0)
		return (-1);

	return (buf[0]);
}

static int
asmc_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	struct asmc_softc *sc = device_get_softc(dev);
	int error, val;

	if (arg2 < 0 || arg2 >= sc->sc_temp_count)
		return (EINVAL);

	val = asmc_temp_getvalue(dev, sc->sc_temp_sensors[arg2]);
	error = sysctl_handle_int(oidp, &val, 0, req);

	return (error);
}

/*
 * Sudden Motion Sensor functions.
 */
static int
asmc_sms_read(device_t dev, const char *key, int16_t *val)
{
	uint8_t buf[2];
	int error;

	/* no need to do locking here as asmc_key_read() already does it */
	switch (key[3]) {
	case 'X':
	case 'Y':
	case 'Z':
		error = asmc_key_read(dev, key, buf, sizeof(buf));
		break;
	default:
		device_printf(dev, "%s called with invalid argument %s\n",
		    __func__, key);
		error = EINVAL;
		goto out;
	}
	*val = ((int16_t)buf[0] << 8) | buf[1];
out:
	return (error);
}

static void
asmc_sms_calibrate(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);

	asmc_sms_read(dev, ASMC_KEY_SMS_X, &sc->sms_rest_x);
	asmc_sms_read(dev, ASMC_KEY_SMS_Y, &sc->sms_rest_y);
	asmc_sms_read(dev, ASMC_KEY_SMS_Z, &sc->sms_rest_z);
}

static int
asmc_sms_intrfast(void *arg)
{
	uint8_t type;
	device_t dev = (device_t)arg;
	struct asmc_softc *sc = device_get_softc(dev);
	if (!sc->sc_sms_intr_works)
		return (FILTER_HANDLED);

	mtx_lock_spin(&sc->sc_mtx);
	type = ASMC_INTPORT_READ(sc);
	mtx_unlock_spin(&sc->sc_mtx);

	sc->sc_sms_intrtype = type;
	asmc_sms_printintr(dev, type);

	/* Don't queue SMS task for ambient light interrupts */
	if (type == ASMC_ALSL_INT2A && sc->sc_has_alsl)
		return (FILTER_HANDLED);

	taskqueue_enqueue(sc->sc_sms_tq, &sc->sc_sms_task);
	return (FILTER_HANDLED);
}

static void
asmc_sms_printintr(device_t dev, uint8_t type)
{
	struct asmc_softc *sc = device_get_softc(dev);

	switch (type) {
	case ASMC_SMS_INTFF:
		device_printf(dev, "WARNING: possible free fall!\n");
		break;
	case ASMC_SMS_INTHA:
		device_printf(dev, "WARNING: high acceleration detected!\n");
		break;
	case ASMC_SMS_INTSH:
		device_printf(dev, "WARNING: possible shock!\n");
		break;
	case ASMC_ALSL_INT2A:
		/*
		 * This suppresses console and log messages for the ambient
		 * light sensor interrupt on models that have ALSL.
		 */
		if (sc->sc_has_alsl)
			break;
		/* FALLTHROUGH */
	default:
		device_printf(dev, "unknown interrupt: 0x%x\n", type);
	}
}

static void
asmc_sms_task(void *arg, int pending)
{
	struct asmc_softc *sc = (struct asmc_softc *)arg;
	char notify[16];
	int type;

	switch (sc->sc_sms_intrtype) {
	case ASMC_SMS_INTFF:
		type = 2;
		break;
	case ASMC_SMS_INTHA:
		type = 1;
		break;
	case ASMC_SMS_INTSH:
		type = 0;
		break;
	default:
		type = 255;
	}

	snprintf(notify, sizeof(notify), " notify=0x%x", type);
	devctl_notify("ACPI", "asmc", "SMS", notify);
}

static int
asmc_mb_sysctl_sms(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int error;
	int16_t val;
	int32_t v;

	asmc_sms_read(dev, asmc_sms_sysctls[arg2].key, &val);
	v = (int32_t)val;
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mbp_sysctl_light(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t buf[6];
	int error;
	int32_t v;

	asmc_key_read(dev, asmc_light_keys[arg2], buf, sizeof(buf));
	v = buf[2];
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mbp_sysctl_light_control(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t buf[2];
	int error;
	int v;

	v = sc->sc_light_control;
	error = sysctl_handle_int(oidp, &v, 0, req);

	if (error == 0 && req->newptr != NULL) {
		if (v < 0 || v > 255)
			return (EINVAL);
		sc->sc_light_control = v;
		sc->sc_kbd_bkl_level = v * 100 / 255;
		buf[0] = sc->sc_light_control;
		buf[1] = 0x00;
		asmc_key_write(dev, ASMC_KEY_LIGHTVALUE, buf, sizeof(buf));
	}
	return (error);
}

static int
asmc_mbp_sysctl_light_left_10byte(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t buf[10];
	int error;
	uint32_t v;

	asmc_key_read(dev, ASMC_KEY_LIGHTLEFT, buf, sizeof(buf));

	/*
	 * This seems to be a 32 bit big endian value from buf[6] -> buf[9].
	 *
	 * Extract it out manually here, then shift/clamp it.
	 */
	v = be32dec(&buf[6]);

	/*
	 * Shift out, clamp at 255; that way it looks like the
	 * earlier SMC firmware version responses.
	 */
	v = v >> 8;
	if (v > 255)
		v = 255;

	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

/*
 * Auto power-on after AC power loss (AUPO key).
 * When non-zero the machine boots automatically when AC is restored
 * after an unclean power loss.  Useful for always-on servers / home labs.
 */
static int
asmc_aupo_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t aupo;
	int val, error;

	if (asmc_key_read(dev, ASMC_KEY_AUPO, &aupo, 1) != 0)
		return (EIO);

	val = (aupo != 0) ? 1 : 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	aupo = (val != 0) ? 1 : 0;
	if (asmc_key_write(dev, ASMC_KEY_AUPO, &aupo, 1) != 0)
		return (EIO);

	return (0);
}

/* Sleep Indicator LED (SIL) control; see ASMC_KEY_MSLD/MSLS in asmcvar.h. */
static int
asmc_sil_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t msld;
	int val, error;

	if (asmc_key_read(dev, ASMC_KEY_MSLD, &msld, 1) != 0)
		return (EIO);

	/* MSLD 0xff means off, anything else means on */
	val = (msld != 0xff) ? 1 : 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (val != 0) {
		/* Turn on: unlatch MSLS first, then set MSLD duty */
		uint8_t msls = 0x01;
		if (asmc_key_write(dev, ASMC_KEY_MSLS, &msls, 1) != 0)
			return (EIO);
		msld = 0x01;
	} else {
		/* Turn off: just set MSLD to 0xff */
		msld = 0xff;
	}

	if (asmc_key_write(dev, ASMC_KEY_MSLD, &msld, 1) != 0)
		return (EIO);

	return (0);
}

static int
asmc_backlight_update_status(device_t dev, struct backlight_props *props)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t buf[2];

	sc->sc_kbd_bkl_level = props->brightness;
	sc->sc_light_control = props->brightness * 255 / 100;
	buf[0] = sc->sc_light_control;
	buf[1] = 0x00;
	asmc_key_write(dev, ASMC_KEY_LIGHTVALUE, buf, sizeof(buf));

	return (0);
}

static int
asmc_backlight_get_status(device_t dev, struct backlight_props *props)
{
	struct asmc_softc *sc = device_get_softc(dev);

	props->brightness = sc->sc_kbd_bkl_level;
	props->nlevels = 0;

	return (0);
}

static int
asmc_backlight_get_info(device_t dev, struct backlight_info *info)
{
	info->type = BACKLIGHT_TYPE_KEYBOARD;
	strlcpy(info->name, "Apple MacBook Keyboard", BACKLIGHTMAXNAMELENGTH);

	return (0);
}

static const char *
asmc_cause_str(int8_t cause, bool is_sleep)
{
	size_t i;

	for (i = 0; i < nitems(asmc_cause_table); i++) {
		if (asmc_cause_table[i].code != cause)
			continue;
		if (is_sleep && asmc_cause_table[i].sleep_desc != NULL)
			return (asmc_cause_table[i].sleep_desc);
		return (asmc_cause_table[i].desc);
	}
	return (NULL);
}

/* MSSD/MSSP: last shutdown/sleep cause.  arg2: 0=shutdown, 1=sleep. */
static int
asmc_cause_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	bool is_sleep = (arg2 != 0);
	const char *key = is_sleep ? ASMC_KEY_MSSP : ASMC_KEY_MSSD;
	int8_t cause;
	const char *desc;
	char buf[ASMC_CAUSE_BUFLEN];

	/* EIO: SMC I/O bus did not respond to key read. */
	if (asmc_key_read(dev, key, (uint8_t *)&cause, 1) != 0)
		return (EIO);

	desc = asmc_cause_str(cause, is_sleep);
	if (desc != NULL)
		snprintf(buf, sizeof(buf), "%d (%s)", (int)cause, desc);
	else
		snprintf(buf, sizeof(buf), "%d", (int)cause);

	return (sysctl_handle_string(oidp, buf, sizeof(buf), req));
}

static int
asmc_msal_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t msal;
	char buf[80];

	/* EIO: SMC I/O bus did not respond to key read. */
	if (asmc_key_read(dev, ASMC_KEY_MSAL, &msal, 1) != 0)
		return (EIO);

	snprintf(buf, sizeof(buf),
	    "0x%02x (tss=%d therm_valid=%d calib_valid=%d prochot=%d plimits=%d)",
	    msal,
	    (msal & ASMC_MSAL_TSS) != 0,
	    (msal & ASMC_MSAL_THERM_VALID) != 0,
	    (msal & ASMC_MSAL_CALIB_VALID) != 0,
	    (msal & ASMC_MSAL_PROCHOT) != 0,
	    (msal & ASMC_MSAL_PLIMITS) != 0);

	return (sysctl_handle_string(oidp, buf, sizeof(buf), req));
}

static int
asmc_clkt_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t buf[4];
	uint32_t secs;

	if (asmc_key_read(dev, ASMC_KEY_CLKT, buf, 4) != 0)
		return (EIO);

	secs = be32dec(buf);
	return (sysctl_handle_32(oidp, &secs, 0, req));
}

static int
asmc_msps_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t buf[2], len;
	uint32_t state;

	if (asmc_key_getinfo(dev, ASMC_KEY_MSPS, &len, NULL) != 0)
		return (EIO);
	if (len != 1 && len != 2)
		return (EIO);

	memset(buf, 0, sizeof(buf));
	if (asmc_key_read(dev, ASMC_KEY_MSPS, buf, len) != 0)
		return (EIO);

	state = (len == 1) ? buf[0] : be16dec(buf);
	return (sysctl_handle_32(oidp, &state, 0, req));
}

static int
asmc_rplt_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t buf[ASMC_RPLT_MAXLEN + 1];
	char name[ASMC_RPLT_MAXLEN + 1];

	memset(buf, 0, sizeof(buf));
	if (asmc_key_read(dev, ASMC_KEY_RPLT, buf, ASMC_RPLT_MAXLEN) != 0)
		return (EIO);

	memcpy(name, buf, ASMC_RPLT_MAXLEN);
	name[ASMC_RPLT_MAXLEN] = '\0';

	return (sysctl_handle_string(oidp, name, sizeof(name), req));
}

static int
asmc_rgen_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t gen;
	uint32_t val;

	if (asmc_key_read(dev, ASMC_KEY_RGEN, &gen, 1) != 0)
		return (EIO);

	val = gen;
	return (sysctl_handle_32(oidp, &val, 0, req));
}
