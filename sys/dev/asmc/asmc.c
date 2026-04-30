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
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/rman.h>

#include <machine/resource.h>
#include <netinet/in.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>
#include <dev/asmc/asmcvar.h>

#include <dev/backlight/backlight.h>
#include "backlight_if.h"

/*
 * Device interface.
 */
static int 	asmc_probe(device_t dev);
static int 	asmc_attach(device_t dev);
static int 	asmc_detach(device_t dev);
static int 	asmc_resume(device_t dev);

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
static int 	asmc_command(device_t dev, uint8_t command);
static int 	asmc_wait(device_t dev, uint8_t val);
static int 	asmc_wait_ack(device_t dev, uint8_t val, int amount);
static int 	asmc_key_write(device_t dev, const char *key, uint8_t *buf,
    uint8_t len);
static int 	asmc_key_read(device_t dev, const char *key, uint8_t *buf,
    uint8_t);
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
#ifdef ASMC_DEBUG
void		asmc_dumpall(device_t);
static int	asmc_key_dump(device_t, int);
#endif

/*
 * Sysctl handlers.
 */
static int 	asmc_mb_sysctl_fanid(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fanspeed(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fansafespeed(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fanminspeed(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fanmaxspeed(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fantargetspeed(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fanmanual(SYSCTL_HANDLER_ARGS);
static int 	asmc_temp_sysctl(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_sms_x(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_sms_y(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_sms_z(SYSCTL_HANDLER_ARGS);
static int 	asmc_mbp_sysctl_light_left(SYSCTL_HANDLER_ARGS);
static int 	asmc_mbp_sysctl_light_right(SYSCTL_HANDLER_ARGS);
static int 	asmc_mbp_sysctl_light_control(SYSCTL_HANDLER_ARGS);
static int 	asmc_mbp_sysctl_light_left_10byte(SYSCTL_HANDLER_ARGS);
static int	asmc_wol_sysctl(SYSCTL_HANDLER_ARGS);

static int	asmc_key_getinfo(device_t, const char *, uint8_t *, char *);

#ifdef ASMC_DEBUG
/* Raw key access */
static int	asmc_raw_key_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_raw_value_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_raw_len_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_raw_type_sysctl(SYSCTL_HANDLER_ARGS);
#endif

/* Voltage/Current/Power/Light sensor support */
static int	asmc_sensor_read(device_t, const char *, int *);
static int	asmc_sensor_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_detect_sensors(device_t);
static int	asmc_key_dump_by_index(device_t, int, char *, char *, uint8_t *);
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
#ifdef ASMC_DEBUG
#define ASMC_DPRINTF(str, ...)	device_printf(dev, str, ##__VA_ARGS__)
#else
#define ASMC_DPRINTF(str, ...)
#endif

/* NB: can't be const */
static char *asmc_ids[] = { "APP0001", NULL };

static unsigned int light_control = 0;

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

static int
asmc_attach(device_t dev)
{
	int i, j;
	int ret;
	char name[2];
	struct asmc_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	struct sysctl_oid *sysctlnode;

	sc->sc_ioport = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
	    &sc->sc_rid_port, RF_ACTIVE);
	if (sc->sc_ioport == NULL) {
		device_printf(dev, "unable to allocate IO port\n");
		return (ENOMEM);
	}

	sysctlctx = device_get_sysctl_ctx(dev);
	sysctlnode = device_get_sysctl_tree(dev);

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

	/*
	 * dev.asmc.n.fan.* tree.
	 */
	sc->sc_fan_tree[0] = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode), OID_AUTO, "fan",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Fan Root Tree");

	for (i = 1; i <= sc->sc_nfan; i++) {
		j = i - 1;
		name[0] = '0' + j;
		name[1] = 0;
		sc->sc_fan_tree[i] = SYSCTL_ADD_NODE(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[0]), OID_AUTO, name,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Fan Subtree");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
		    OID_AUTO, "id",
		    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, j,
		    asmc_mb_sysctl_fanid, "I", "Fan ID");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
		    OID_AUTO, "speed",
		    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, j,
		    asmc_mb_sysctl_fanspeed, "I", "Fan speed in RPM");

		if (sc->sc_has_safespeed) {
			SYSCTL_ADD_PROC(sysctlctx,
			    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
			    OID_AUTO, "safespeed",
			    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, j,
			    asmc_mb_sysctl_fansafespeed, "I",
			    "Fan safe speed in RPM");
		}

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
		    OID_AUTO, "minspeed",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, dev, j,
		    asmc_mb_sysctl_fanminspeed, "I",
		    "Fan minimum speed in RPM");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
		    OID_AUTO, "maxspeed",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, dev, j,
		    asmc_mb_sysctl_fanmaxspeed, "I",
		    "Fan maximum speed in RPM");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
		    OID_AUTO, "targetspeed",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, dev, j,
		    asmc_mb_sysctl_fantargetspeed, "I",
		    "Fan target speed in RPM");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
		    OID_AUTO, "manual",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, dev, j,
		    asmc_mb_sysctl_fanmanual, "I",
		    "Fan manual mode (0=auto, 1=manual)");
	}

	/*
	 * dev.asmc.n.temp tree.
	 */
	sc->sc_temp_tree = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode), OID_AUTO, "temp",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Temperature sensors");

	for (i = 0; i < sc->sc_temp_count; i++) {
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
		    dev, 0,
		    sc->sc_light_len == ASMC_LIGHT_LONGLEN ?
		        asmc_mbp_sysctl_light_left_10byte :
		        asmc_mbp_sysctl_light_left,
		    "I", "Keyboard backlight left sensor");

		if (sc->sc_light_len != ASMC_LIGHT_LONGLEN &&
		    asmc_key_getinfo(dev, ASMC_KEY_LIGHTRIGHT,
		    NULL, NULL) == 0) {
			SYSCTL_ADD_PROC(sysctlctx,
			    SYSCTL_CHILDREN(sc->sc_light_tree),
			    OID_AUTO, "right",
			    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
			    dev, 0,
			    asmc_mbp_sysctl_light_right, "I",
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

#ifdef ASMC_DEBUG
	/*
	 * Raw SMC key access for debugging.
	 */
	sc->sc_raw_tree = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "raw", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Raw SMC key access");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sc->sc_raw_tree),
	    OID_AUTO, "key",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    dev, 0, asmc_raw_key_sysctl, "A",
	    "SMC key name (4 chars)");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sc->sc_raw_tree),
	    OID_AUTO, "value",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    dev, 0, asmc_raw_value_sysctl, "A",
	    "SMC key value (hex string)");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sc->sc_raw_tree),
	    OID_AUTO, "len",
	    CTLTYPE_U8 | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, 0, asmc_raw_len_sysctl, "CU",
	    "SMC key value length");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sc->sc_raw_tree),
	    OID_AUTO, "type",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, 0, asmc_raw_type_sysctl, "A",
	    "SMC key type (4 chars)");
#endif

	if (!sc->sc_has_sms)
		goto nosms;

	/*
	 * Initialize SMS hardware.
	 */
	asmc_sms_init(dev);

	/*
	 * dev.asmc.n.sms tree.
	 */
	sc->sc_sms_tree = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode), OID_AUTO, "sms",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Sudden Motion Sensor");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sc->sc_sms_tree),
	    OID_AUTO, "x",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, 0, asmc_mb_sysctl_sms_x, "I",
	    "Sudden Motion Sensor X value");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sc->sc_sms_tree),
	    OID_AUTO, "y",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, 0, asmc_mb_sysctl_sms_y, "I",
	    "Sudden Motion Sensor Y value");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sc->sc_sms_tree),
	    OID_AUTO, "z",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, 0, asmc_mb_sysctl_sms_z, "I",
	    "Sudden Motion Sensor Z value");

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
	if (mtx_initialized(&sc->sc_mtx)) {
		mtx_destroy(&sc->sc_mtx);
	}

	return (0);
}

static int
asmc_resume(device_t dev)
{
	uint8_t buf[2];

	buf[0] = light_control;
	buf[1] = 0x00;
	asmc_key_write(dev, ASMC_KEY_LIGHTVALUE, buf, sizeof(buf));

	return (0);
}

#ifdef ASMC_DEBUG
void
asmc_dumpall(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	int i;

	if (sc->sc_nkeys == 0) {
		device_printf(dev, "asmc_dumpall: key count not available\n");
		return;
	}

	device_printf(dev, "asmc_dumpall: dumping %d keys\n", sc->sc_nkeys);
	for (i = 0; i < sc->sc_nkeys; i++)
		asmc_key_dump(dev, i);
}
#endif

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
	if (error != 0)
		goto out;
	device_printf(dev, "SMC revision: %x.%x%x%x\n", buf[0], buf[1], buf[2],
	    ntohs(*(uint16_t *)buf + 4));

	/* Wake-on-LAN convenience sysctl */
	if (asmc_key_read(dev, ASMC_KEY_AUPO, buf, 1) == 0) {
		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, "wol",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
		    dev, 0, asmc_wol_sysctl, "I",
		    "Wake-on-LAN enable (0=off, 1=on)");
	}

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
#ifdef ASMC_DEBUG
	asmc_dumpall(dev);
#endif
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
	sc->sc_has_sms =
	    (asmc_key_getinfo(dev, ASMC_KEY_SMS,
	    &len, type) == 0 &&
	    asmc_key_getinfo(dev, ASMC_KEY_SMS_X,
	    &len, type) == 0 &&
	    asmc_key_getinfo(dev, ASMC_KEY_SMS_Y,
	    &len, type) == 0 &&
	    asmc_key_getinfo(dev, ASMC_KEY_SMS_Z,
	    &len, type) == 0 &&
	    asmc_key_getinfo(dev, ASMC_KEY_SMS_LOW,
	    &len, type) == 0 &&
	    asmc_key_getinfo(dev, ASMC_KEY_SMS_HIGH,
	    &len, type) == 0 &&
	    asmc_key_getinfo(dev, ASMC_KEY_SMS_LOW_INT,
	    &len, type) == 0 &&
	    asmc_key_getinfo(dev, ASMC_KEY_SMS_HIGH_INT,
	    &len, type) == 0 &&
	    asmc_key_getinfo(dev, ASMC_KEY_SMS_FLAG,
	    &len, type) == 0 &&
	    asmc_key_getinfo(dev, ASMC_KEY_INTOK,
	    &len, type) == 0);

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

/*
 * We need to make sure that the SMC acks the byte sent.
 * Just wait up to (amount * 10)  ms.
 */
static int
asmc_wait_ack(device_t dev, uint8_t val, int amount)
{
	struct asmc_softc *sc = device_get_softc(dev);
	u_int i;

	val = val & ASMC_STATUS_MASK;

	for (i = 0; i < amount; i++) {
		if ((ASMC_CMDPORT_READ(sc) & ASMC_STATUS_MASK) == val)
			return (0);
		DELAY(10);
	}

	return (1);
}

/*
 * We need to make sure that the SMC acks the byte sent.
 * Just wait up to 100 ms.
 */
static int
asmc_wait(device_t dev, uint8_t val)
{
#ifdef ASMC_DEBUG
	struct asmc_softc *sc;
#endif

	if (asmc_wait_ack(dev, val, 1000) == 0)
		return (0);

#ifdef ASMC_DEBUG
	sc = device_get_softc(dev);

	device_printf(dev, "%s failed: 0x%x, 0x%x\n", __func__,
	    val & ASMC_STATUS_MASK, ASMC_CMDPORT_READ(sc));
#endif
	return (1);
}

/*
 * Send the given command, retrying up to 10 times if
 * the acknowledgement fails.
 */
static int
asmc_command(device_t dev, uint8_t command)
{
	int i;
	struct asmc_softc *sc = device_get_softc(dev);

	for (i = 0; i < 10; i++) {
		ASMC_CMDPORT_WRITE(sc, command);
		if (asmc_wait_ack(dev, 0x0c, 100) == 0) {
			return (0);
		}
	}

#ifdef ASMC_DEBUG
	device_printf(dev, "%s failed: 0x%x, 0x%x\n", __func__, command,
	    ASMC_CMDPORT_READ(sc));
#endif
	return (1);
}

static int
asmc_key_read(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	int i, error = 1, try = 0;
	struct asmc_softc *sc = device_get_softc(dev);

	mtx_lock_spin(&sc->sc_mtx);

begin:
	if (asmc_command(dev, ASMC_CMDREAD))
		goto out;

	for (i = 0; i < 4; i++) {
		ASMC_DATAPORT_WRITE(sc, key[i]);
		if (asmc_wait(dev, 0x04))
			goto out;
	}

	ASMC_DATAPORT_WRITE(sc, len);

	for (i = 0; i < len; i++) {
		if (asmc_wait(dev, 0x05))
			goto out;
		buf[i] = ASMC_DATAPORT_READ(sc);
	}

	error = 0;
out:
	if (error) {
		if (++try < 10)
			goto begin;
		device_printf(dev, "%s for key %s failed %d times, giving up\n",
		    __func__, key, try);
	}

	mtx_unlock_spin(&sc->sc_mtx);

	return (error);
}

#ifdef ASMC_DEBUG
static int
asmc_key_dump(device_t dev, int number)
{
	struct asmc_softc *sc = device_get_softc(dev);
	char key[ASMC_KEYLEN + 1] = { 0 };
	char type[ASMC_KEYINFO_RESPLEN + 1] = { 0 };
	uint8_t index[4];
	uint8_t v[ASMC_MAXVAL];
	uint8_t maxlen;
	int i, error = 1, try = 0;

	mtx_lock_spin(&sc->sc_mtx);

	index[0] = (number >> 24) & 0xff;
	index[1] = (number >> 16) & 0xff;
	index[2] = (number >> 8) & 0xff;
	index[3] = number & 0xff;

begin:
	if (asmc_command(dev, ASMC_CMDGETBYINDEX))
		goto out;

	for (i = 0; i < ASMC_KEYLEN; i++) {
		ASMC_DATAPORT_WRITE(sc, index[i]);
		if (asmc_wait(dev, ASMC_STATUS_AWAIT_DATA))
			goto out;
	}

	ASMC_DATAPORT_WRITE(sc, ASMC_KEYLEN);

	for (i = 0; i < ASMC_KEYLEN; i++) {
		if (asmc_wait(dev, ASMC_STATUS_DATA_READY))
			goto out;
		key[i] = ASMC_DATAPORT_READ(sc);
	}

	/* Get key info (length + type). */
	if (asmc_command(dev, ASMC_CMDGETINFO))
		goto out;

	for (i = 0; i < ASMC_KEYLEN; i++) {
		ASMC_DATAPORT_WRITE(sc, key[i]);
		if (asmc_wait(dev, ASMC_STATUS_AWAIT_DATA))
			goto out;
	}

	ASMC_DATAPORT_WRITE(sc, ASMC_KEYINFO_RESPLEN);

	for (i = 0; i < ASMC_KEYINFO_RESPLEN; i++) {
		if (asmc_wait(dev, ASMC_STATUS_DATA_READY))
			goto out;
		type[i] = ASMC_DATAPORT_READ(sc);
	}

	error = 0;
out:
	if (error) {
		if (++try < ASMC_MAXRETRIES)
			goto begin;
		device_printf(dev,
		    "%s for key %d failed %d times, giving up\n",
		    __func__, number, try);
	}
	mtx_unlock_spin(&sc->sc_mtx);

	if (error)
		return (error);

	maxlen = type[0];
	type[0] = ' ';
	type[5] = '\0';
	if (maxlen > sizeof(v))
		maxlen = sizeof(v);

	memset(v, 0, sizeof(v));
	error = asmc_key_read(dev, key, v, maxlen);
	if (error)
		return (error);

	device_printf(dev, "key %d: %s, type%s (len %d), data",
	    number, key, type, maxlen);
	for (i = 0; i < maxlen; i++)
		printf(" %02x", v[i]);
	printf("\n");

	return (0);
}
#endif /* ASMC_DEBUG */

/*
 * Get key info (length and type) from SMC using command 0x13.
 * If len is non-NULL, stores the key's value length.
 * If type is non-NULL, stores the 4-char type string (must be at least 5 bytes).
 */
static int
asmc_key_getinfo(device_t dev, const char *key, uint8_t *len, char *type)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t info[ASMC_KEYINFO_RESPLEN];
	int i, error = -1, try = 0;

	mtx_lock_spin(&sc->sc_mtx);

begin:
	if (asmc_command(dev, ASMC_CMDGETINFO))
		goto out;

	for (i = 0; i < ASMC_KEYLEN; i++) {
		ASMC_DATAPORT_WRITE(sc, key[i]);
		if (asmc_wait(dev, ASMC_STATUS_AWAIT_DATA))
			goto out;
	}

	ASMC_DATAPORT_WRITE(sc, ASMC_KEYINFO_RESPLEN);

	for (i = 0; i < ASMC_KEYINFO_RESPLEN; i++) {
		if (asmc_wait(dev, ASMC_STATUS_DATA_READY))
			goto out;
		info[i] = ASMC_DATAPORT_READ(sc);
	}

	error = 0;
out:
	if (error && ++try < ASMC_MAXRETRIES)
		goto begin;
	mtx_unlock_spin(&sc->sc_mtx);

	if (error == 0) {
		if (len != NULL)
			*len = info[0];
		if (type != NULL) {
			for (i = 0; i < ASMC_TYPELEN; i++)
				type[i] = info[i + 1];
			type[ASMC_TYPELEN] = '\0';
		}
	}
	return (error);
}

#ifdef ASMC_DEBUG
/*
 * Raw SMC key access sysctls - enables reading/writing any SMC key by name
 * Usage:
 *   sysctl dev.asmc.0.raw.key=AUPO   # Set key, auto-detects length
 *   sysctl dev.asmc.0.raw.value      # Read current value (hex bytes)
 *   sysctl dev.asmc.0.raw.value=01   # Write new value
 */
static int
asmc_raw_key_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	struct asmc_softc *sc = device_get_softc(dev);
	char newkey[ASMC_KEYLEN + 1];
	uint8_t keylen;
	int error;

	strlcpy(newkey, sc->sc_rawkey, sizeof(newkey));
	error = sysctl_handle_string(oidp, newkey, sizeof(newkey), req);
	if (error || req->newptr == NULL)
		return (error);

	if (strlen(newkey) != ASMC_KEYLEN)
		return (EINVAL);

	/* Get key info to auto-detect length and type */
	if (asmc_key_getinfo(dev, newkey, &keylen, sc->sc_rawtype) != 0)
		return (ENOENT);

	if (keylen > ASMC_MAXVAL)
		keylen = ASMC_MAXVAL;

	strlcpy(sc->sc_rawkey, newkey, sizeof(sc->sc_rawkey));
	sc->sc_rawlen = keylen;
	memset(sc->sc_rawval, 0, sizeof(sc->sc_rawval));

	/* Read the key value */
	asmc_key_read(dev, sc->sc_rawkey, sc->sc_rawval, sc->sc_rawlen);

	return (0);
}

static int
asmc_raw_value_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	struct asmc_softc *sc = device_get_softc(dev);
	char hexbuf[ASMC_MAXVAL * 2 + 1];
	int error, i;

	/* Refresh from SMC if a key has been selected. */
	if (sc->sc_rawkey[0] != '\0') {
		asmc_key_read(dev, sc->sc_rawkey, sc->sc_rawval,
		    sc->sc_rawlen > 0 ? sc->sc_rawlen : ASMC_MAXVAL);
	}

	/* Format as hex string */
	for (i = 0; i < sc->sc_rawlen && i < ASMC_MAXVAL; i++)
		snprintf(hexbuf + i * 2, 3, "%02x", sc->sc_rawval[i]);
	hexbuf[i * 2] = '\0';

	error = sysctl_handle_string(oidp, hexbuf, sizeof(hexbuf), req);
	if (error || req->newptr == NULL)
		return (error);

	/* Reject writes until a key is selected via raw.key. */
	if (sc->sc_rawkey[0] == '\0')
		return (EINVAL);

	memset(sc->sc_rawval, 0, sizeof(sc->sc_rawval));
	for (i = 0; i < sc->sc_rawlen && hexbuf[i*2] && hexbuf[i*2+1]; i++) {
		unsigned int val;
		char tmp[3] = { hexbuf[i*2], hexbuf[i*2+1], 0 };
		if (sscanf(tmp, "%02x", &val) == 1)
			sc->sc_rawval[i] = (uint8_t)val;
	}

	if (asmc_key_write(dev, sc->sc_rawkey, sc->sc_rawval, sc->sc_rawlen) != 0)
		return (EIO);

	return (0);
}

static int
asmc_raw_len_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	struct asmc_softc *sc = device_get_softc(dev);

	return (sysctl_handle_8(oidp, &sc->sc_rawlen, 0, req));
}

static int
asmc_raw_type_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	struct asmc_softc *sc = device_get_softc(dev);

	return (sysctl_handle_string(oidp, sc->sc_rawtype,
	    sizeof(sc->sc_rawtype), req));
}
#endif

/*
 * Convert signed fixed-point SMC values to milli-units.
 * Format "spXY" means signed with X integer bits and Y fraction bits.
 */
static int
asmc_sp78_to_milli(const uint8_t *buf)
{
	int16_t val = (int16_t)be16dec(buf);

	return ((int)val * 1000) / 256;
}

static int
asmc_sp87_to_milli(const uint8_t *buf)
{
	int16_t val = (int16_t)be16dec(buf);

	return ((int)val * 1000) / 128;
}

static int
asmc_sp4b_to_milli(const uint8_t *buf)
{
	int16_t val = (int16_t)be16dec(buf);

	return ((int)val * 1000) / 2048;
}

static int
asmc_sp5a_to_milli(const uint8_t *buf)
{
	int16_t val = (int16_t)be16dec(buf);

	return ((int)val * 1000) / 1024;
}

static int
asmc_sp69_to_milli(const uint8_t *buf)
{
	int16_t val = (int16_t)be16dec(buf);

	return ((int)val * 1000) / 512;
}

static int
asmc_sp96_to_milli(const uint8_t *buf)
{
	int16_t val = (int16_t)be16dec(buf);

	return ((int)val * 1000) / 64;
}

static int
asmc_sp2d_to_milli(const uint8_t *buf)
{
	int16_t val = (int16_t)be16dec(buf);

	return ((int)val * 1000) / 8192;
}

static bool
asmc_sensor_type_supported(const char *type)
{

	return (strncmp(type, "sp78", 4) == 0 ||
	    strncmp(type, "sp87", 4) == 0 ||
	    strncmp(type, "sp4b", 4) == 0 ||
	    strncmp(type, "sp5a", 4) == 0 ||
	    strncmp(type, "sp69", 4) == 0 ||
	    strncmp(type, "sp96", 4) == 0 ||
	    strncmp(type, "sp2d", 4) == 0 ||
	    strncmp(type, "ui16", 4) == 0);
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

	if (strncmp(type, "sp78", 4) == 0) {
		*millivalue = asmc_sp78_to_milli(buf);
	} else if (strncmp(type, "sp87", 4) == 0) {
		*millivalue = asmc_sp87_to_milli(buf);
	} else if (strncmp(type, "sp4b", 4) == 0) {
		*millivalue = asmc_sp4b_to_milli(buf);
	} else if (strncmp(type, "sp5a", 4) == 0) {
		*millivalue = asmc_sp5a_to_milli(buf);
	} else if (strncmp(type, "sp69", 4) == 0) {
		*millivalue = asmc_sp69_to_milli(buf);
	} else if (strncmp(type, "sp96", 4) == 0) {
		*millivalue = asmc_sp96_to_milli(buf);
	} else if (strncmp(type, "sp2d", 4) == 0) {
		*millivalue = asmc_sp2d_to_milli(buf);
	} else if (strncmp(type, "ui16", 4) == 0) {
		*millivalue = be16dec(buf);
	} else {
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

/*
 * Scan a range of SMC key indices, adding matching sensors.
 * Only considers 2-byte keys with a supported type.
 */
static void
asmc_scan_sensor_range(device_t dev, unsigned int start,
    unsigned int end, char prefix, int *countp, char **sensors,
    int maxcount)
{
	char key[ASMC_KEYLEN + 1];
	char type[ASMC_TYPELEN + 1];
	uint8_t len;
	unsigned int i;
	char *sensor_key;

	for (i = start; i < end; i++) {
		if (asmc_key_dump_by_index(dev, i, key, type, &len))
			continue;
		if (key[0] != prefix || len != 2)
			continue;
		if (!asmc_sensor_type_supported(type))
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
	char key[ASMC_KEYLEN + 1];
	char type[ASMC_TYPELEN + 1];
	uint8_t len;
	unsigned int start, end, i;
	int error;
	char *sensor_key;

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
	if (error == 0) {
		for (i = start; i < end; i++) {
			if (asmc_key_dump_by_index(dev, i,
			    key, type, &len))
				continue;
			if (len != 2 ||
			    strncmp(type, "sp78", 4) != 0)
				continue;
			if (sc->sc_temp_count >= ASMC_TEMP_MAX)
				break;
			sensor_key = malloc(ASMC_KEYLEN + 1,
			    M_DEVBUF, M_WAITOK);
			memcpy(sensor_key, key, ASMC_KEYLEN + 1);
			sc->sc_temp_sensors[sc->sc_temp_count++] =
			    sensor_key;
		}
	}

	/* Voltage sensors: V..W range */
	error = asmc_key_search(dev, "V\0\0\0", &start);
	if (error == 0)
		error = asmc_key_search(dev, "W\0\0\0", &end);
	if (error == 0)
		asmc_scan_sensor_range(dev, start, end, 'V',
		    &sc->sc_voltage_count, sc->sc_voltage_sensors,
		    ASMC_MAX_SENSORS);

	/* Current sensors: I..J range */
	error = asmc_key_search(dev, "I\0\0\0", &start);
	if (error == 0)
		error = asmc_key_search(dev, "J\0\0\0", &end);
	if (error == 0)
		asmc_scan_sensor_range(dev, start, end, 'I',
		    &sc->sc_current_count, sc->sc_current_sensors,
		    ASMC_MAX_SENSORS);

	/* Power sensors: P..Q range */
	error = asmc_key_search(dev, "P\0\0\0", &start);
	if (error == 0)
		error = asmc_key_search(dev, "Q\0\0\0", &end);
	if (error == 0)
		asmc_scan_sensor_range(dev, start, end, 'P',
		    &sc->sc_power_count, sc->sc_power_sensors,
		    ASMC_MAX_SENSORS);

	/* Ambient light sensors: AL* in A..B range */
	error = asmc_key_search(dev, "A\0\0\0", &start);
	if (error == 0)
		error = asmc_key_search(dev, "B\0\0\0", &end);
	if (error == 0) {
		for (i = start; i < end; i++) {
			if (asmc_key_dump_by_index(dev, i,
			    key, type, &len))
				continue;
			if (key[0] != 'A' || key[1] != 'L' ||
			    (key[2] != 'V' && key[2] != 'S') ||
			    len != 2)
				continue;
			if (!asmc_sensor_type_supported(type))
				continue;
			if (sc->sc_light_count >= ASMC_MAX_SENSORS)
				break;
			sensor_key = malloc(ASMC_KEYLEN + 1,
			    M_DEVBUF, M_WAITOK);
			memcpy(sensor_key, key, ASMC_KEYLEN + 1);
			sc->sc_light_sensors[sc->sc_light_count++] =
			    sensor_key;
		}
	}

	if (bootverbose)
		device_printf(dev,
		    "detected %d temp, %d voltage, %d current, "
		    "%d power, %d light sensors\n",
		    sc->sc_temp_count, sc->sc_voltage_count,
		    sc->sc_current_count,
		    sc->sc_power_count, sc->sc_light_count);

	/* Register sysctls for detected sensors */
	sysctlctx = device_get_sysctl_ctx(dev);

	/* Voltage sensors */
	if (sc->sc_voltage_count > 0) {
		tree_node = SYSCTL_ADD_NODE(sysctlctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    "voltage", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Voltage sensors (millivolts)");

		for (i = 0; i < sc->sc_voltage_count; i++) {
			SYSCTL_ADD_PROC(sysctlctx, SYSCTL_CHILDREN(tree_node),
			    OID_AUTO, sc->sc_voltage_sensors[i],
			    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
			    dev, ('V' << 8) | i, asmc_sensor_sysctl, "I",
			    "Voltage sensor (millivolts)");
		}
	}

	/* Current sensors */
	if (sc->sc_current_count > 0) {
		tree_node = SYSCTL_ADD_NODE(sysctlctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    "current", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Current sensors (milliamps)");

		for (i = 0; i < sc->sc_current_count; i++) {
			SYSCTL_ADD_PROC(sysctlctx, SYSCTL_CHILDREN(tree_node),
			    OID_AUTO, sc->sc_current_sensors[i],
			    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
			    dev, ('I' << 8) | i, asmc_sensor_sysctl, "I",
			    "Current sensor (milliamps)");
		}
	}

	/* Power sensors */
	if (sc->sc_power_count > 0) {
		tree_node = SYSCTL_ADD_NODE(sysctlctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    "power", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Power sensors (milliwatts)");

		for (i = 0; i < sc->sc_power_count; i++) {
			SYSCTL_ADD_PROC(sysctlctx, SYSCTL_CHILDREN(tree_node),
			    OID_AUTO, sc->sc_power_sensors[i],
			    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
			    dev, ('P' << 8) | i, asmc_sensor_sysctl, "I",
			    "Power sensor (milliwatts)");
		}
	}

	/* Ambient light sensors */
	if (sc->sc_light_count > 0) {
		tree_node = SYSCTL_ADD_NODE(sysctlctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    "ambient", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Ambient light sensors");

		for (i = 0; i < sc->sc_light_count; i++) {
			SYSCTL_ADD_PROC(sysctlctx, SYSCTL_CHILDREN(tree_node),
			    OID_AUTO, sc->sc_light_sensors[i],
			    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
			    dev, ('L' << 8) | i, asmc_sensor_sysctl, "I",
			    "Light sensor value");
		}
	}

	return (0);
}

/*
 * Helper function to get key info by index (for sensor detection).
 */
static int
asmc_key_dump_by_index(device_t dev, int index, char *key_out,
    char *type_out, uint8_t *len_out)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t index_buf[ASMC_KEYLEN];
	uint8_t key_buf[ASMC_KEYLEN];
	uint8_t info_buf[ASMC_KEYINFO_RESPLEN];
	int error = ENXIO, try = 0;
	int i;

	mtx_lock_spin(&sc->sc_mtx);

	index_buf[0] = (index >> 24) & 0xff;
	index_buf[1] = (index >> 16) & 0xff;
	index_buf[2] = (index >> 8) & 0xff;
	index_buf[3] = index & 0xff;

begin:
	if (asmc_command(dev, ASMC_CMDGETBYINDEX))
		goto out;

	for (i = 0; i < ASMC_KEYLEN; i++) {
		ASMC_DATAPORT_WRITE(sc, index_buf[i]);
		if (asmc_wait(dev, ASMC_STATUS_AWAIT_DATA))
			goto out;
	}

	ASMC_DATAPORT_WRITE(sc, ASMC_KEYLEN);

	for (i = 0; i < ASMC_KEYLEN; i++) {
		if (asmc_wait(dev, ASMC_STATUS_DATA_READY))
			goto out;
		key_buf[i] = ASMC_DATAPORT_READ(sc);
	}

	if (asmc_command(dev, ASMC_CMDGETINFO))
		goto out;

	for (i = 0; i < ASMC_KEYLEN; i++) {
		ASMC_DATAPORT_WRITE(sc, key_buf[i]);
		if (asmc_wait(dev, ASMC_STATUS_AWAIT_DATA))
			goto out;
	}

	ASMC_DATAPORT_WRITE(sc, ASMC_KEYINFO_RESPLEN);

	for (i = 0; i < ASMC_KEYINFO_RESPLEN; i++) {
		if (asmc_wait(dev, ASMC_STATUS_DATA_READY))
			goto out;
		info_buf[i] = ASMC_DATAPORT_READ(sc);
	}

	memcpy(key_out, key_buf, ASMC_KEYLEN);
	key_out[ASMC_KEYLEN] = '\0';
	*len_out = info_buf[0];
	memcpy(type_out, &info_buf[1], ASMC_TYPELEN);
	type_out[ASMC_TYPELEN] = '\0';
	error = 0;

out:
	if (error) {
		if (++try < ASMC_MAXRETRIES)
			goto begin;
	}

	mtx_unlock_spin(&sc->sc_mtx);
	return (error);
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

static int
asmc_key_write(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	int i, error = -1, try = 0;
	struct asmc_softc *sc = device_get_softc(dev);

	mtx_lock_spin(&sc->sc_mtx);

begin:
	ASMC_DPRINTF(("cmd port: cmd write\n"));
	if (asmc_command(dev, ASMC_CMDWRITE))
		goto out;

	ASMC_DPRINTF(("data port: key\n"));
	for (i = 0; i < 4; i++) {
		ASMC_DATAPORT_WRITE(sc, key[i]);
		if (asmc_wait(dev, 0x04))
			goto out;
	}
	ASMC_DPRINTF(("data port: length\n"));
	ASMC_DATAPORT_WRITE(sc, len);

	ASMC_DPRINTF(("data port: buffer\n"));
	for (i = 0; i < len; i++) {
		if (asmc_wait(dev, 0x04))
			goto out;
		ASMC_DATAPORT_WRITE(sc, buf[i]);
	}

	error = 0;
out:
	if (error) {
		if (++try < 10)
			goto begin;
		device_printf(dev, "%s for key %s failed %d times, giving up\n",
		    __func__, key, try);
	}

	mtx_unlock_spin(&sc->sc_mtx);

	return (error);
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
	int speed;
	uint8_t buf[2];
	char fankey[5];

	snprintf(fankey, sizeof(fankey), key, fan);
	if (asmc_key_read(dev, fankey, buf, sizeof(buf)) != 0)
		return (-1);
	speed = (buf[0] << 6) | (buf[1] >> 2);

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
	uint8_t buf[2];
	char fankey[5];

	speed *= 4;

	buf[0] = speed >> 8;
	buf[1] = speed;

	snprintf(fankey, sizeof(fankey), key, fan);
	if (asmc_key_write(dev, fankey, buf, sizeof(buf)) < 0)
		return (-1);

	return (0);
}

static int
asmc_mb_sysctl_fanspeed(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int fan = arg2;
	int error;
	int32_t v;

	v = asmc_fan_getvalue(dev, ASMC_KEY_FANSPEED, fan);
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mb_sysctl_fanid(SYSCTL_HANDLER_ARGS)
{
	uint8_t buf[16];
	device_t dev = (device_t)arg1;
	int fan = arg2;
	int error = true;
	char *desc;

	desc = asmc_fan_getstring(dev, ASMC_KEY_FANID, fan, buf, sizeof(buf));

	if (desc != NULL)
		error = sysctl_handle_string(oidp, desc, 0, req);

	return (error);
}

static int
asmc_mb_sysctl_fansafespeed(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int fan = arg2;
	int error;
	int32_t v;

	v = asmc_fan_getvalue(dev, ASMC_KEY_FANSAFESPEED, fan);
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mb_sysctl_fanminspeed(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int fan = arg2;
	int error;
	int32_t v;

	v = asmc_fan_getvalue(dev, ASMC_KEY_FANMINSPEED, fan);
	error = sysctl_handle_int(oidp, &v, 0, req);

	if (error == 0 && req->newptr != NULL) {
		unsigned int newspeed = v;
		asmc_fan_setvalue(dev, ASMC_KEY_FANMINSPEED, fan, newspeed);
	}

	return (error);
}

static int
asmc_mb_sysctl_fanmaxspeed(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int fan = arg2;
	int error;
	int32_t v;

	v = asmc_fan_getvalue(dev, ASMC_KEY_FANMAXSPEED, fan);
	error = sysctl_handle_int(oidp, &v, 0, req);

	if (error == 0 && req->newptr != NULL) {
		unsigned int newspeed = v;
		asmc_fan_setvalue(dev, ASMC_KEY_FANMAXSPEED, fan, newspeed);
	}

	return (error);
}

static int
asmc_mb_sysctl_fantargetspeed(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int fan = arg2;
	int error;
	int32_t v;

	v = asmc_fan_getvalue(dev, ASMC_KEY_FANTARGETSPEED, fan);
	error = sysctl_handle_int(oidp, &v, 0, req);

	if (error == 0 && req->newptr != NULL) {
		unsigned int newspeed = v;
		asmc_fan_setvalue(dev, ASMC_KEY_FANTARGETSPEED, fan, newspeed);
	}

	return (error);
}

static int
asmc_mb_sysctl_fanmanual(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int fan = arg2;
	int error;
	int32_t v;
	uint8_t buf[2];
	uint16_t val;

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
asmc_mb_sysctl_sms_x(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int error;
	int16_t val;
	int32_t v;

	asmc_sms_read(dev, ASMC_KEY_SMS_X, &val);
	v = (int32_t)val;
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mb_sysctl_sms_y(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int error;
	int16_t val;
	int32_t v;

	asmc_sms_read(dev, ASMC_KEY_SMS_Y, &val);
	v = (int32_t)val;
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mb_sysctl_sms_z(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int error;
	int16_t val;
	int32_t v;

	asmc_sms_read(dev, ASMC_KEY_SMS_Z, &val);
	v = (int32_t)val;
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mbp_sysctl_light_left(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t buf[6];
	int error;
	int32_t v;

	asmc_key_read(dev, ASMC_KEY_LIGHTLEFT, buf, sizeof(buf));
	v = buf[2];
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mbp_sysctl_light_right(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t buf[6];
	int error;
	int32_t v;

	asmc_key_read(dev, ASMC_KEY_LIGHTRIGHT, buf, sizeof(buf));
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

	v = light_control;
	error = sysctl_handle_int(oidp, &v, 0, req);

	if (error == 0 && req->newptr != NULL) {
		if (v < 0 || v > 255)
			return (EINVAL);
		light_control = v;
		sc->sc_kbd_bkl_level = v * 100 / 255;
		buf[0] = light_control;
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
 * Wake-on-LAN convenience sysctl.
 * Reading returns 1 if WoL is enabled, 0 if disabled.
 * Writing 1 enables WoL, 0 disables it.
 */
static int
asmc_wol_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t aupo;
	int val, error;

	/* Read current AUPO value */
	if (asmc_key_read(dev, ASMC_KEY_AUPO, &aupo, 1) != 0)
		return (EIO);

	val = (aupo != 0) ? 1 : 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	/* Clamp to 0 or 1 */
	aupo = (val != 0) ? 1 : 0;

	/* Write AUPO */
	if (asmc_key_write(dev, ASMC_KEY_AUPO, &aupo, 1) != 0)
		return (EIO);

	return (0);
}

static int
asmc_backlight_update_status(device_t dev, struct backlight_props *props)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t buf[2];

	sc->sc_kbd_bkl_level = props->brightness;
	light_control = props->brightness * 255 / 100;
	buf[0] = light_control;
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
