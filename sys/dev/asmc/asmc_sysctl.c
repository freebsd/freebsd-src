/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Sysctl layer for Apple SMC.
 *
 * The dev.asmc.N tree: the leaf tables, the registration helpers asmc_attach()
 * calls to build each subtree, and the handlers behind them.  Everything here
 * reaches the hardware through the key transport in asmc.c.
 */

#include "opt_asmc.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <dev/asmc/asmcvar.h>
#include <dev/asmc/asmcmmio.h>
#include <dev/asmc/asmc_sysctl.h>

/*
 * Shutdown/sleep cause codes (MSSD/MSSP values).
 *
 * Positive values indicate normal operation; negative values indicate
 * fault conditions.
 * Sources: Apple SMC firmware, VirtualSMC docs, macOS
 * pmset/powermetrics output.
 */
struct asmc_cause {
	int8_t		code;
	const char	*desc;		/* shutdown description */
	const char	*sleep_desc;	/* sleep description, or NULL if same */
};

#define ASMC_CAUSE_BUFLEN	48	/* "-128 (temperature-overlimit-timeout)\0" */

static const struct asmc_cause asmc_cause_table[] = {
	{    5,	"good-shutdown",		"good-sleep" },
	{    3,	"power-button",			NULL },
	{    2,	"low-battery-sleep",		NULL },
	{    1,	"overtemp-sleep",		NULL },
	{    0,	"initial",			NULL },
	{   -1,	"health-check",			NULL },
	{   -2,	"power-supply",			NULL },
	{   -3,	"temperature-multisleep",	NULL },
	{   -4,	"sensor-fan",			NULL },
	{  -30,	"temperature-overlimit-timeout", NULL },
	{  -40,	"pswr-smrst",			NULL },	/* power supply watchdog reset */
	{  -50,	"unmapped",			NULL },
	{  -60,	"low-battery",			NULL },
	{  -61,	"ninja-shutdown",		NULL },	/* sudden unexpected power loss */
	{  -62,	"ninja-restart",		NULL },
	{  -70,	"palm-rest-temperature",	NULL },
	{  -71,	"sodimm-temperature",		NULL },
	{  -72,	"heatpipe-temperature",		NULL },
	{  -74,	"battery-temperature",		NULL },
	{  -75,	"adapter-timeout",		NULL },
	{  -77,	"manual-temperature",		NULL },
	{  -78,	"adapter-current",		NULL },
	{  -79,	"battery-current",		NULL },
	{  -82,	"skin-temperature",		NULL },
	{  -83,	"skin-temperature-sensors",	NULL },
	{  -84,	"backup-temperature",		NULL },
	{  -86,	"cpu-proximity-temperature",	NULL },
	{  -95,	"cpu-temperature",		NULL },
	{ -100,	"power-supply-temperature",	NULL },
	{ -101,	"lcd-temperature",		NULL },
	{ -102,	"rsm-power-fail",		NULL },
	{ -103,	"battery-cuv",			NULL },	/* cell under-voltage */
	{ -127,	"pmu-forced-shutdown",		NULL },
	{ -128,	"unknown",			NULL },
};

/*
 * Sysctl handlers.
 */
static int	asmc_mb_sysctl_fanid(SYSCTL_HANDLER_ARGS);
static int	asmc_mb_sysctl_fanspeed(SYSCTL_HANDLER_ARGS);
static int	asmc_mb_sysctl_fanmanual(SYSCTL_HANDLER_ARGS);
static int	asmc_temp_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_mb_sysctl_sms(SYSCTL_HANDLER_ARGS);
static int	asmc_mbp_sysctl_light(SYSCTL_HANDLER_ARGS);
static int	asmc_mbp_sysctl_light_control(SYSCTL_HANDLER_ARGS);
static int	asmc_mbp_sysctl_light_left_10byte(SYSCTL_HANDLER_ARGS);
static int	asmc_aupo_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_sil_sysctl(SYSCTL_HANDLER_ARGS);

/* System state / board identity sysctls */
static int	asmc_cause_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_msal_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_clkt_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_msps_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_rplt_sysctl(SYSCTL_HANDLER_ARGS);
static int	asmc_rgen_sysctl(SYSCTL_HANDLER_ARGS);

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
	for (unsigned int i = 0; i < nitems(asmc_temp_descs); i++) {
		if (strcmp(asmc_temp_descs[i].key, key) == 0)
			return (asmc_temp_descs[i].desc);
	}
	return ("Temperature");
}

void
asmc_sysctl_add_temp(device_t dev, struct sysctl_ctx_list *ctx)
{
	struct asmc_softc *sc = device_get_softc(dev);

	sc->sc_temp_tree = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "temp",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Temperature sensors");

	for (int i = 0; i < sc->sc_temp_count; i++) {
		SYSCTL_ADD_PROC(ctx,
		    SYSCTL_CHILDREN(sc->sc_temp_tree),
		    OID_AUTO, sc->sc_temp_sensors[i],
		    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, i,
		    asmc_temp_sysctl, "I",
		    asmc_temp_desc(sc->sc_temp_sensors[i]));
	}
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

void
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

void
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

/* The ambient light sensors, selected by arg2. */
#define	ASMC_LIGHT_LEFT		0
#define	ASMC_LIGHT_RIGHT	1

static const char * const asmc_light_keys[] = {
	[ASMC_LIGHT_LEFT]	= ASMC_KEY_LIGHTLEFT,
	[ASMC_LIGHT_RIGHT]	= ASMC_KEY_LIGHTRIGHT,
};

void
asmc_sysctl_add_light(device_t dev, struct sysctl_ctx_list *ctx)
{
	struct asmc_softc *sc = device_get_softc(dev);

	sc->sc_light_tree = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "light",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
	    "Keyboard backlight sensors");

	SYSCTL_ADD_PROC(ctx,
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
		SYSCTL_ADD_PROC(ctx,
		    SYSCTL_CHILDREN(sc->sc_light_tree),
		    OID_AUTO, "right",
		    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
		    dev, ASMC_LIGHT_RIGHT,
		    asmc_mbp_sysctl_light, "I",
		    "Keyboard backlight right sensor");
	}

	SYSCTL_ADD_PROC(ctx,
	    SYSCTL_CHILDREN(sc->sc_light_tree),
	    OID_AUTO, "control",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY | CTLFLAG_MPSAFE,
	    dev, 0, asmc_mbp_sysctl_light_control, "I",
	    "Keyboard backlight brightness control");
}

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

int
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

void
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

/*
 * Generic sensor sysctl handler for voltage/current/power/light sensors.
 * arg2 encodes: sensor_type (high byte) | sensor_index (low byte)
 * Sensor types: 'V'=voltage, 'I'=current, 'P'=power, 'L'=light
 */
int
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
		v = buf[0] != 0 ? 1 : 0;

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
			if (v != 0) {
				val |= (1 << fan);   /* Set to manual */
			} else {
				val &= ~(1 << fan);  /* Set to auto */
			}

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

static const char *
asmc_cause_str(int8_t cause, bool is_sleep)
{
	for (size_t i = 0; i < nitems(asmc_cause_table); i++) {
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
