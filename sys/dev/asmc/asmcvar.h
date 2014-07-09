/*-
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
 * $FreeBSD$
 *
 */

#define ASMC_MAXFANS	2

struct asmc_softc {
	device_t 		sc_dev;
	struct mtx 		sc_mtx;
	int 			sc_nfan;
	int16_t			sms_rest_x;
	int16_t			sms_rest_y;
	int16_t			sms_rest_z;
	struct sysctl_oid 	*sc_fan_tree[ASMC_MAXFANS+1];
	struct sysctl_oid 	*sc_temp_tree;
	struct sysctl_oid 	*sc_sms_tree;
	struct sysctl_oid 	*sc_light_tree;
	struct asmc_model 	*sc_model;
	int 			sc_rid_port;
	int 			sc_rid_irq;
	struct resource 	*sc_ioport;
	struct resource 	*sc_irq;
	void 			*sc_cookie;
	int 			sc_sms_intrtype;
	struct taskqueue 	*sc_sms_tq;
	struct task 		sc_sms_task;
	uint8_t			sc_sms_intr_works;
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

/*
 * Interrupt port.
 */
#define ASMC_INTPORT_READ(sc)	bus_read_1(sc->sc_ioport, 0x1f)


/* Number of keys */
#define ASMC_NKEYS		"#KEY"	/* RO; 4 bytes */ 

/*
 * Fan control via SMC.
 */
#define ASMC_KEY_FANCOUNT	"FNum"	/* RO; 1 byte */
#define ASMC_KEY_FANMANUAL	"FS! "	/* RW; 2 bytes */
#define ASMC_KEY_FANSPEED	"F%dAc"	/* RO; 2 bytes */
#define ASMC_KEY_FANMINSPEED	"F%dMn"	/* RO; 2 bytes */
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
 * Keyboard backlight.
 */
#define ASMC_KEY_LIGHTLEFT	"ALV0"	/* RO; 6 bytes */
#define ASMC_KEY_LIGHTRIGHT	"ALV1"	/* RO; 6 bytes */
#define ASMC_KEY_LIGHTVALUE	"LKSB"	/* WO; 2 bytes */

/*
 * Clamshell.
 */
#define ASMC_KEY_CLAMSHELL	"MSLD"	/* RO; 1 byte */

/*
 * Interrupt keys.
 */
#define ASMC_KEY_INTOK		"NTOK"	/* WO; 1 byte */

/*
 * Temperatures.
 *
 * First for MacBook, second for MacBook Pro, third for Intel Mac Mini,
 * fourth the Mac Pro 8-core and finally the MacBook Air.
 *
 */
/* maximum array size for temperatures including the last NULL */
#define ASMC_TEMP_MAX		36
#define ASMC_MB_TEMPS		{ "TB0T", "TN0P", "TN1P", "Th0H", "Th1H", \
				  "TM0P", NULL }
#define ASMC_MB_TEMPNAMES	{ "enclosure", "northbridge1", \
				  "northbridge2", "heatsink1", \
				  "heatsink2", "memory", }
#define ASMC_MB_TEMPDESCS	{ "Enclosure Bottomside", \
				  "Northbridge Point 1", \
				  "Northbridge Point 2", "Heatsink 1", \
				  "Heatsink 2", "Memory Bank A", }

#define ASMC_MBP_TEMPS		{ "TB0T", "Th0H", "Th1H", "Tm0P", \
				  "TG0H", "TG0P", "TG0T", NULL }

#define ASMC_MBP_TEMPNAMES	{ "enclosure", "heatsink1", \
				  "heatsink2", "memory", "graphics", \
				  "graphicssink", "unknown", }

#define ASMC_MBP_TEMPDESCS	{ "Enclosure Bottomside", \
				  "Heatsink 1", "Heatsink 2", \
				  "Memory Controller", \
				  "Graphics Chip", "Graphics Heatsink", \
				  "Unknown", } 

#define ASMC_MBP4_TEMPS		{ "TB0T", "Th0H", "Th1H", "Th2H", "Tm0P", \
				  "TG0H", "TG0D", "TC0D", "TC0P", "Ts0P", \
				  "TTF0", "TW0P", NULL }

#define ASMC_MBP4_TEMPNAMES	{ "enclosure", "heatsink1", "heatsink2", \
				  "heatsink3", "memory", "graphicssink", \
				  "graphics", "cpu", "cpu2", "unknown1", \
				  "unknown2", "wireless", }

#define ASMC_MBP4_TEMPDESCS	{ "Enclosure Bottomside", \
				  "Main Heatsink 1", "Main Heatsink 2", \
				  "Main Heatsink 3", \
				  "Memory Controller", \
				  "Graphics Chip Heatsink", \
				  "Graphics Chip Diode", \
				  "CPU Temperature Diode", "CPU Point 2", \
				  "Unknown", "Unknown", \
				  "Wireless Module", } 

#define ASMC_MM_TEMPS		{ "TN0P", "TN1P", NULL }
#define ASMC_MM_TEMPNAMES	{ "northbridge1", "northbridge2" }
#define ASMC_MM_TEMPDESCS	{ "Northbridge Point 1", \
				  "Northbridge Point 2" }

#define ASMC_MM31_TEMPS		{ "TC0D", "TC0H", \
				  "TC0P", "TH0P", \
				  "TN0D", "TN0P", \
				  "TW0P", NULL }

#define ASMC_MM31_TEMPNAMES	{ "cpu0_die", "cpu0_heatsink", \
				  "cpu0_proximity", "hdd_bay", \
				  "northbridge_die", \
				  "northbridge_proximity", \
				  "wireless_module", }

#define ASMC_MM31_TEMPDESCS	{ "CPU0 Die Core Temperature", \
				  "CPU0 Heatsink Temperature", \
				  "CPU0 Proximity Temperature", \
				  "HDD Bay Temperature", \
				  "Northbridge Die Core Temperature", \
				  "Northbridge Proximity Temperature", \
				  "Wireless Module Temperature", }

#define ASMC_MP_TEMPS		{ "TA0P", "TCAG", "TCAH", "TCBG", "TCBH", \
				  "TC0C", "TC0D", "TC0P", "TC1C", "TC1D", \
				  "TC2C", "TC2D", "TC3C", "TC3D", "THTG", \
				  "TH0P", "TH1P", "TH2P", "TH3P", "TMAP", \
				  "TMAS", "TMBS", "TM0P", "TM0S", "TM1P", \
				  "TM1S", "TM2P", "TM2S", "TM3S", "TM8P", \
				  "TM8S", "TM9P", "TM9S", "TN0H", "TS0C", \
				  NULL }

#define ASMC_MP_TEMPNAMES	{ "TA0P", "TCAG", "TCAH", "TCBG", "TCBH", \
				  "TC0C", "TC0D", "TC0P", "TC1C", "TC1D", \
				  "TC2C", "TC2D", "TC3C", "TC3D", "THTG", \
				  "TH0P", "TH1P", "TH2P", "TH3P", "TMAP", \
				  "TMAS", "TMBS", "TM0P", "TM0S", "TM1P", \
				  "TM1S", "TM2P", "TM2S", "TM3S", "TM8P", \
				  "TM8S", "TM9P", "TM9S", "TN0H", "TS0C", \
				  NULL }

#define ASMC_MP_TEMPDESCS	{ "TA0P", "TCAG", "TCAH", "TCBG", "TCBH", \
				  "TC0C", "TC0D", "TC0P", "TC1C", "TC1D", \
				  "TC2C", "TC2D", "TC3C", "TC3D", "THTG", \
				  "TH0P", "TH1P", "TH2P", "TH3P", "TMAP", \
				  "TMAS", "TMBS", "TM0P", "TM0S", "TM1P", \
				  "TM1S", "TM2P", "TM2S", "TM3S", "TM8P", \
				  "TM8S", "TM9P", "TM9S", "TN0H", "TS0C", \
				  NULL }

#define	ASMC_MBA_TEMPS		{ "TB0T", NULL }
#define	ASMC_MBA_TEMPNAMES	{ "enclosure" }
#define	ASMC_MBA_TEMPDESCS	{ "Enclosure Bottom" }
