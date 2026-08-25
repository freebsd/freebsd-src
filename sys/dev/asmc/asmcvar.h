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

#define ASMC_MAXFANS	6
#define ASMC_MAXVAL	32	/* Maximum SMC value size */
#define ASMC_KEYLEN	4	/* SMC key name length */
#define ASMC_TYPELEN	4	/* SMC type string length */
#define ASMC_MAX_SENSORS	64	/* Max sensors per type */

/* Maximum number of auto-detected temperature sensors */
#define ASMC_TEMP_MAX		80

struct asmc_softc {
	device_t 		sc_dev;
	struct mtx 		sc_mtx;
	int 			sc_nfan;
	int 			sc_nkeys;
	int16_t			sms_rest_x;
	int16_t			sms_rest_y;
	int16_t			sms_rest_z;
	struct sysctl_oid 	*sc_fan_tree[ASMC_MAXFANS+1];
	struct sysctl_oid 	*sc_temp_tree;
	struct sysctl_oid 	*sc_sms_tree;
	struct sysctl_oid 	*sc_light_tree;
	int 			sc_rid_port;
	int 			sc_rid_irq;
	struct resource 	*sc_ioport;
	struct resource 	*sc_irq;
	void 			*sc_cookie;
	/* MMIO backend (T2 Macs) */
	int			sc_rid_mem;
	struct resource		*sc_iomem;
	bool			sc_is_mmio;
	int			sc_is_t2;	/* T2 fan float + per-fan manual */
	int 			sc_sms_intrtype;
	struct taskqueue 	*sc_sms_tq;
	struct task 		sc_sms_task;
	uint8_t			sc_sms_intr_works;
	struct cdev		*sc_kbd_bkl;
	uint32_t		sc_kbd_bkl_level;
#ifdef ASMC_DEBUG
	/* Raw key access */
	struct sysctl_oid	*sc_raw_tree;
	char			sc_rawkey[ASMC_KEYLEN + 1];
	uint8_t			sc_rawval[ASMC_MAXVAL];
	uint8_t			sc_rawlen;
	char			sc_rawtype[ASMC_TYPELEN + 1];
#endif
	/* Voltage/Current/Power/Light sensors */
	char			*sc_voltage_sensors[ASMC_MAX_SENSORS];
	int			sc_voltage_count;
	char			*sc_current_sensors[ASMC_MAX_SENSORS];
	int			sc_current_count;
	char			*sc_power_sensors[ASMC_MAX_SENSORS];
	int			sc_power_count;
	char			*sc_light_sensors[ASMC_MAX_SENSORS];
	int			sc_light_count;
	/* Auto-detected temperature sensors */
	char			*sc_temp_sensors[ASMC_TEMP_MAX];
	int			sc_temp_count;
	/* Auto-detected capabilities */
	int			sc_has_sms;
	int			sc_has_light;
	int			sc_light_len;	/* ASMC_LIGHT_{SHORT,LONG}LEN */
	int			sc_has_safespeed;
	int			sc_has_alsl;	/* ALSL interrupt source */
};

/*
 * Data port.
 */
#define ASMC_DATAPORT_READ(sc)	bus_read_1(sc->sc_ioport, 0x00)
#define ASMC_DATAPORT_WRITE(sc, val) \
	bus_write_1(sc->sc_ioport, 0x00, val)
#define ASMC_STATUS_MASK 	0x0f

/*
 * Command port.
 */
#define ASMC_CMDPORT_READ(sc)	bus_read_1(sc->sc_ioport, 0x04)
#define ASMC_CMDPORT_WRITE(sc, val) \
	bus_write_1(sc->sc_ioport, 0x04, val)
#define ASMC_CMDREAD		0x10
#define ASMC_CMDWRITE		0x11
#define ASMC_CMDGETBYINDEX	0x12
#define ASMC_CMDGETINFO		0x13

#define ASMC_STATUS_AWAIT_DATA	0x04
#define ASMC_STATUS_DATA_READY	0x05

#define ASMC_KEYINFO_RESPLEN	6	/* getinfo: 1 len + 4 type + 1 attr */
#define ASMC_MAXRETRIES		10

/*
 * Interrupt port.
 */
#define ASMC_INTPORT_READ(sc)	bus_read_1(sc->sc_ioport, 0x1f)

/* Number of keys */
#define ASMC_NKEYS		"#KEY"	/* RO; 4 bytes */

/* Query the ASMC revision */
#define ASMC_KEY_REV		"REV "  /* RO: 6 bytes */

/*
 * Fan control via SMC.
 */
#define ASMC_KEY_FANCOUNT	"FNum"	/* RO; 1 byte */
#define ASMC_KEY_FANMANUAL	"FS! "	/* RW; 2 bytes */
#define ASMC_KEY_FANID		"F%dID"	/* RO; 16 bytes */
#define ASMC_KEY_FANSPEED	"F%dAc"	/* RO; 2 bytes */
#define ASMC_KEY_FANMINSPEED	"F%dMn"	/* RW; 2 bytes */
#define ASMC_KEY_FANMAXSPEED	"F%dMx"	/* RO; 2 bytes */
#define ASMC_KEY_FANSAFESPEED	"F%dSf"	/* RO; 2 bytes */
#define ASMC_KEY_FANTARGETSPEED	"F%dTg"	/* RW; 2 bytes */

/*
 * Sudden Motion Sensor (SMS).
 */
#define ASMC_SMS_INIT1		0xe0
#define ASMC_SMS_INIT2		0xf8
#define ASMC_KEY_SMS		"MOCN"	/* RW; 2 bytes */
#define ASMC_KEY_SMS_X		"MO_X"	/* RO; 2 bytes */
#define ASMC_KEY_SMS_Y		"MO_Y"	/* RO; 2 bytes */
#define ASMC_KEY_SMS_Z		"MO_Z"	/* RO; 2 bytes */
#define ASMC_KEY_SMS_LOW	"MOLT"	/* RW; 2 bytes */
#define ASMC_KEY_SMS_HIGH	"MOHT"	/* RW; 2 bytes */
#define ASMC_KEY_SMS_LOW_INT	"MOLD"	/* RW; 1 byte */
#define ASMC_KEY_SMS_HIGH_INT	"MOHD"	/* RW; 1 byte */
#define ASMC_KEY_SMS_FLAG	"MSDW"	/* RW; 1 byte */
#define ASMC_SMS_INTFF		0x60	/* Free fall Interrupt */
#define ASMC_SMS_INTHA		0x6f	/* High Acceleration Interrupt */
#define ASMC_SMS_INTSH		0x80	/* Shock Interrupt */

/*
 * Light Sensor.
 */
#define ASMC_ALSL_INT2A		0x2a	/* Ambient Light related Interrupt */

/*
 * Keyboard backlight.
 */
#define ASMC_LIGHT_SHORTLEN	6	/* ALV0 short format */
#define ASMC_LIGHT_LONGLEN	10	/* ALV0 long format (10-byte) */
#define ASMC_KEY_LIGHTLEFT	"ALV0"	/* RO; 6 or 10 bytes */
#define ASMC_KEY_LIGHTRIGHT	"ALV1"	/* RO; 6 bytes */
#define ASMC_KEY_LIGHTVALUE	"LKSB"	/* WO; 2 bytes */
#define ASMC_KEY_LIGHTSRC	"ALSL"	/* RO; ambient light source */
#define ASMC_KEY_FANSAFESPEED0	"F0Sf"	/* RO; 2 bytes */

/*
 * Clamshell.
 */
#define ASMC_KEY_CLAMSHELL	"MSLD"	/* RO; 1 byte */

/*
 * Auto power-on after AC power loss (AUPO).
 * When set, the machine boots automatically when AC power is restored
 * after an unclean power loss.  This is NOT Wake-on-LAN.
 */
#define ASMC_KEY_AUPO		"AUPO"	/* RW; 1 byte */

/*
 * Interrupt keys.
 */
#define ASMC_KEY_INTOK		"NTOK"	/* WO; 1 byte */

/*
 * System State Machine / diagnostics keys.
 *
 * MSSD - Mac System Shutdown cause (last shutdown reason code)
 * MSSP - Mac System SleeP cause (last sleep reason code)
 * MSAL - Mac System ALarm (thermal subsystem status bitmap)
 * MSPS - Mac System Power State (current power state index)
 * CLKT - CLocK Time (seconds since midnight, SMC RTC)
 * MSTS - Mac System sTate/Status (SSM state; 0 = S0 awake)
 */
#define ASMC_KEY_CLKT		"CLKT"	/* RO; 4 bytes ui32 BE */
#define ASMC_KEY_MSSD		"MSSD"	/* RO; 1 byte si8 */
#define ASMC_KEY_MSSP		"MSSP"	/* RO; 1 byte si8 */
#define ASMC_KEY_MSAL		"MSAL"	/* RO; 1 byte hex_ */
#define ASMC_KEY_MSPS		"MSPS"	/* RO; 1 or 2 bytes hex_ */
#define ASMC_KEY_MSTS		"MSTS"	/* RO; 1 byte ui8 */

#define ASMC_MSAL_TSS		0x01	/* TSS running */
#define ASMC_MSAL_THERM_VALID	0x02	/* thermal calibration valid */
#define ASMC_MSAL_CALIB_VALID	0x08	/* I/P calibration valid */
#define ASMC_MSAL_PROCHOT	0x40	/* Prochot enabled */
#define ASMC_MSAL_PLIMITS	0x80	/* plimits enabled */

/*
 * Board identity keys.
 *
 * RPlt - board platform identifier (e.g. "Mac-XXXX")
 * RGEN - security chip generation (3 = T2)
 */
#define ASMC_KEY_RPLT		"RPlt"	/* RO; 8 bytes ch8* */
#define ASMC_KEY_RGEN		"RGEN"	/* RO; 1 byte ui8 */
#define ASMC_RPLT_MAXLEN	8	/* RPlt key data length */

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
	{  -30,	"temperature-overlimit-timeout",NULL },
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
