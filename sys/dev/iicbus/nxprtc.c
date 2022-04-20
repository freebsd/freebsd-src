/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 Ian Lepore <ian@freebsd.org>
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
 * Driver for NXP real-time clock/calendar chips:
 *  - PCF8563 = low power, countdown timer
 *  - PCA8565 = like PCF8563, automotive temperature range
 *  - PCF8523 = low power, countdown timer, oscillator freq tuning, 2 timers
 *  - PCF2127 = like PCF8523, industrial, tcxo, tamper/ts, i2c & spi, 512B ram
 *  - PCA2129 = like PCF8523, automotive, tcxo, tamper/ts, i2c & spi, (note 1)
 *  - PCF2129 = like PCF8523, industrial, tcxo, tamper/ts, i2c & spi, (note 1)
 *
 *  Most chips have a countdown timer, ostensibly intended to generate periodic
 *  interrupt signals on an output pin.  The timer is driven from the same
 *  divider chain that clocks the time of day registers, and they start counting
 *  in sync when the STOP bit is cleared after the time and timer registers are
 *  set.  The timer register can also be read on the fly, so we use it to count
 *  fractional seconds and get a resolution of ~15ms.
 *
 *  [1] Note that the datasheets for the PCx2129 chips state that they have only
 *  a watchdog timer, not a countdown timer.  Empirical testing shows that the
 *  countdown timer is actually there and it works fine, except that it can't
 *  trigger an interrupt or toggle an output pin like it can on other chips.  We
 *  don't care about interrupts and output pins, we just read the timer register
 *  to get better resolution.
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>
#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include "clock_if.h"
#include "iicbus_if.h"

/*
 * I2C address 1010 001x : PCA2129 PCF2127 PCF2129 PCF8563 PCF8565
 * I2C address 1101 000x : PCF8523
 */
#define	PCF8563_ADDR		0xa2
#define	PCF8523_ADDR		0xd0

/*
 * Registers, bits within them, and masks that are common to all chip types.
 */
#define	PCF85xx_R_CS1		0x00	/* CS1 and CS2 control regs are in */
#define	PCF85xx_R_CS2		0x01	/* the same location on all chips. */

#define	PCF85xx_B_CS1_STOP	0x20	/* Stop time incrementing bit */
#define	PCF85xx_B_SECOND_OS	0x80	/* Oscillator Stopped bit */

#define	PCF85xx_M_SECOND	0x7f	/* Masks for all BCD time regs... */
#define	PCF85xx_M_MINUTE	0x7f
#define	PCF85xx_M_12HOUR	0x1f
#define	PCF85xx_M_24HOUR	0x3f
#define	PCF85xx_M_DAY		0x3f
#define	PCF85xx_M_MONTH		0x1f
#define	PCF85xx_M_YEAR		0xff

/*
 * PCF2127-specific registers, bits, and masks.
 */
#define	PCF2127_R_TMR_CTL	0x10	/* Timer/watchdog control */

#define	PCF2127_M_TMR_CTRL	0xe3	/* Mask off undef bits */

#define	PCF2127_B_TMR_CD	0x40	/* Run in countdown mode */
#define	PCF2127_B_TMR_64HZ	0x01	/* Timer frequency 64Hz */

#define	PCF2127_R_TS_CTL	0x12	/* Timestamp control */
#define	PCF2127_B_TSOFF		0x40	/* Turn off timestamp function */

#define	PCF2127_R_AGING_OFFSET	0x19	/* Frequency aging offset in PPM */

/*
 * PCA/PCF2129-specific registers, bits, and masks.
 */
#define	PCF2129_B_CS1_12HR	0x04	/* Use 12-hour (AM/PM) mode bit */
#define	PCF2129_B_CLKOUT_OTPR	0x20	/* OTP refresh command */
#define	PCF2129_B_CLKOUT_HIGHZ	0x07	/* Clock Out Freq = disable */

/*
 * PCF8523-specific registers, bits, and masks.
 */
#define	PCF8523_R_CS3		0x02	/* Control and status reg 3 */
#define	PCF8523_R_SECOND	0x03	/* Seconds */
#define	PCF8523_R_TMR_CLKOUT	0x0F	/* Timer and clockout control */
#define	PCF8523_R_TMR_A_FREQ	0x10	/* Timer A frequency control */
#define	PCF8523_R_TMR_A_COUNT	0x11	/* Timer A count */

#define	PCF8523_M_TMR_A_FREQ	0x07	/* Mask off undef bits */

#define	PCF8523_B_HOUR_PM	0x20	/* PM bit */
#define	PCF8523_B_CS1_SOFTRESET	0x58	/* Initiate Soft Reset bits */
#define	PCF8523_B_CS1_12HR	0x08	/* Use 12-hour (AM/PM) mode bit */
#define	PCF8523_B_CLKOUT_TACD	0x02	/* TimerA runs in CountDown mode */
#define	PCF8523_B_CLKOUT_HIGHZ	0x38	/* Clock Out Freq = disable */
#define	PCF8523_B_TMR_A_64HZ	0x01	/* Timer A freq 64Hz */

#define	PCF8523_M_CS3_PM	0xE0	/* Power mode mask */
#define	PCF8523_B_CS3_PM_NOBAT	0xE0	/* PM bits: no battery usage */
#define	PCF8523_B_CS3_PM_STD	0x00	/* PM bits: standard */
#define	PCF8523_B_CS3_PM_DSNBM	0xa0	/* PM bits: direct switch, no bat mon */
#define	PCF8523_B_CS3_BLF	0x04	/* Battery Low Flag bit */

/*
 * PCF8563-specific registers, bits, and masks.
 */
#define	PCF8563_R_SECOND	0x02	/* Seconds */

#define	PCF8563_R_CLKOUT	0x0d	/* Clock output control */

#define	PCF8563_R_TMR_CTRL	0x0e	/* Timer control */
#define	PCF8563_R_TMR_COUNT	0x0f	/* Timer count */

#define	PCF8563_M_TMR_CTRL	0x93	/* Mask off undef bits */

#define	PCF8563_B_TMR_ENABLE	0x80	/* Enable countdown timer */
#define	PCF8563_B_TMR_64HZ	0x01	/* Timer frequency 64Hz */

#define	PCF8563_B_MONTH_C	0x80	/* Century bit */

/*
 * We use the countdown timer for fractional seconds.  We program it for 64 Hz,
 * the fastest available rate that doesn't roll over in less than a second.
 */
#define	TMR_TICKS_SEC		64
#define	TMR_TICKS_HALFSEC	32

/*
 * The chip types we support.
 */
enum {
	TYPE_NONE,
	TYPE_PCA2129,
	TYPE_PCA8565,
	TYPE_PCF2127,
	TYPE_PCF2129,
	TYPE_PCF8523,
	TYPE_PCF8563,

	TYPE_COUNT
};
static const char *desc_strings[] = {
	"",
	"NXP PCA2129 RTC",
	"NXP PCA8565 RTC",
	"NXP PCF2127 RTC",
	"NXP PCF2129 RTC",
	"NXP PCF8523 RTC",
	"NXP PCF8563 RTC",
};
CTASSERT(nitems(desc_strings) == TYPE_COUNT);

/*
 * The time registers in the order they are laid out in hardware.
 */
struct time_regs {
	uint8_t sec, min, hour, day, wday, month, year;
};

struct nxprtc_softc {
	device_t	dev;
	device_t	busdev;
	struct intr_config_hook
			config_hook;
	u_int		flags;		/* SC_F_* flags */
	u_int		chiptype;	/* Type of PCF85xx chip */
	time_t		bat_time;	/* Next time to check battery */
	int		freqadj;	/* Current freq adj in PPM */
	uint8_t		secaddr;	/* Address of seconds register */
	uint8_t		tmcaddr;	/* Address of timer count register */
	bool		use_timer;	/* Use timer for fractional sec */
	bool		use_ampm;	/* Chip is set to use am/pm mode */
	bool		is212x;		/* Chip type is 2127 or 2129 */
};

#define	SC_F_CPOL	(1 << 0)	/* Century bit means 19xx */

/*
 * When doing i2c IO, indicate that we need to wait for exclusive bus ownership,
 * but that we should not wait if we already own the bus.  This lets us put
 * iicbus_acquire_bus() calls with a non-recursive wait at the entry of our API
 * functions to ensure that only one client at a time accesses the hardware for
 * the entire series of operations it takes to read or write the clock.
 */
#define	WAITFLAGS	(IIC_WAIT | IIC_RECURSIVE)

/*
 * We use the compat_data table to look up hint strings in the non-FDT case, so
 * define the struct locally when we don't get it from ofw_bus_subr.h.
 */
#ifdef FDT
typedef struct ofw_compat_data nxprtc_compat_data;
#else
typedef struct {
	const char *ocd_str;
	uintptr_t  ocd_data;
} nxprtc_compat_data;
#endif

static nxprtc_compat_data compat_data[] = {
	{"nxp,pca2129",     TYPE_PCA2129},
	{"nxp,pca8565",     TYPE_PCA8565},
	{"nxp,pcf2127",     TYPE_PCF2127},
	{"nxp,pcf2129",     TYPE_PCF2129},
	{"nxp,pcf8523",     TYPE_PCF8523},
	{"nxp,pcf8563",     TYPE_PCF8563},

	/* Undocumented compat strings known to exist in the wild... */
	{"pcf8563",         TYPE_PCF8563},
	{"phg,pcf8563",     TYPE_PCF8563},
	{"philips,pcf8563", TYPE_PCF8563},

	{NULL,              TYPE_NONE},
};

static int
nxprtc_readfrom(device_t slavedev, uint8_t regaddr, void *buffer,
    uint16_t buflen, int waithow)
{
	struct iic_msg msg;
	int err;
	uint8_t slaveaddr;

	/*
	 * Two transfers back to back with a stop and start between them; first we
	 * write the address-within-device, then we read from the device.  This
	 * is used instead of the standard iicdev_readfrom() because some of the
	 * chips we service don't support i2c repeat-start operations (grrrrr)
	 * so we do two completely separate transfers with a full stop between.
	 */
	slaveaddr = iicbus_get_addr(slavedev);

	msg.slave = slaveaddr;
	msg.flags = IIC_M_WR;
	msg.len   = 1;
	msg.buf   = &regaddr;

	if ((err = iicbus_transfer_excl(slavedev, &msg, 1, waithow)) != 0)
		return (err);

	msg.slave = slaveaddr;
	msg.flags = IIC_M_RD;
	msg.len   = buflen;
	msg.buf   = buffer;

	return (iicbus_transfer_excl(slavedev, &msg, 1, waithow));
}

static int
read_reg(struct nxprtc_softc *sc, uint8_t reg, uint8_t *val)
{

	return (nxprtc_readfrom(sc->dev, reg, val, sizeof(*val), WAITFLAGS));
}

static int
write_reg(struct nxprtc_softc *sc, uint8_t reg, uint8_t val)
{

	return (iicdev_writeto(sc->dev, reg, &val, sizeof(val), WAITFLAGS));
}

static int
read_timeregs(struct nxprtc_softc *sc, struct time_regs *tregs, uint8_t *tmr)
{
	int err;
	uint8_t sec, tmr1, tmr2;

	/*
	 * The datasheet says loop to read the same timer value twice because it
	 * does not freeze while reading.  To that we add our own logic that
	 * the seconds register must be the same before and after reading the
	 * timer, ensuring the fractional part is from the same second as tregs.
	 */
	do {
		if (sc->use_timer) {
			if ((err = read_reg(sc, sc->secaddr, &sec)) != 0)
				break;
			if ((err = read_reg(sc, sc->tmcaddr, &tmr1)) != 0)
				break;
			if ((err = read_reg(sc, sc->tmcaddr, &tmr2)) != 0)
				break;
			if (tmr1 != tmr2)
				continue;
		}
		if ((err = nxprtc_readfrom(sc->dev, sc->secaddr, tregs,
		    sizeof(*tregs), WAITFLAGS)) != 0)
			break;
	} while (sc->use_timer && tregs->sec != sec);

	/*
	 * If the timer value is greater than our hz rate (or is zero),
	 * something is wrong.  Maybe some other OS used the timer differently?
	 * Just set it to zero.  Likewise if we're not using the timer.  After
	 * the offset calc below, the zero turns into 32, the mid-second point,
	 * which in effect performs 4/5 rounding, which is just the right thing
	 * to do if we don't have fine-grained time.
	 */
	if (!sc->use_timer || tmr1 > TMR_TICKS_SEC)
		tmr1 = 0;

	/*
	 * Turn the downcounter into an upcounter.  The timer starts counting at
	 * and rolls over at mid-second, so add half a second worth of ticks to
	 * get its zero point back in sync with the tregs.sec rollover.
	 */
	*tmr = (TMR_TICKS_SEC - tmr1 + TMR_TICKS_HALFSEC) % TMR_TICKS_SEC;

	return (err);
}

static int
write_timeregs(struct nxprtc_softc *sc, struct time_regs *tregs)
{

	return (iicdev_writeto(sc->dev, sc->secaddr, tregs,
	    sizeof(*tregs), WAITFLAGS));
}

static int
freqadj_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct nxprtc_softc *sc;
	int err, freqppm, newppm;

	sc = arg1;

	/* PPM range [-7,8] maps to reg value range [0,15] */
	freqppm = newppm = 8 - sc->freqadj;

	err = sysctl_handle_int(oidp, &newppm, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	if (freqppm != newppm) {
		if (newppm < -7 || newppm > 8)
			return (EINVAL);
		sc->freqadj = 8 - newppm;
		err = write_reg(sc, PCF2127_R_AGING_OFFSET, sc->freqadj);
	}

	return (err);
}

static int
pcf8523_battery_check(struct nxprtc_softc *sc)
{
	struct timespec ts;
	int err;
	uint8_t cs3;

	/* We check the battery when starting up, and then only once a day. */
	getnanouptime(&ts);
	if (ts.tv_sec < sc->bat_time)
		return (0);
	sc->bat_time = ts.tv_sec + (60 * 60 * 24);

	/*
	 * The 8523, 2127, and 2129 chips have a "power manager" which includes
	 * an optional battery voltage monitor and several choices for power
	 * switching modes.  The battery monitor uses a lot of current and it
	 * remains active when running from battery, making it the "drain my
	 * battery twice as fast" mode.  So, we run the chip in direct-switching
	 * mode with the battery monitor disabled, reducing the current draw
	 * when running on battery from 1930nA to 880nA.  While it's not clear
	 * from the datasheets, empirical testing shows that both disabling the
	 * battery monitor and using direct-switch mode are required to get the
	 * full power savings.
	 *
	 * There isn't any need to continuously monitor the battery voltage, so
	 * this function is used to periodically enable the monitor, check the
	 * voltage, then return to the low-power monitor-disabled mode.
	 */
	err = write_reg(sc, PCF8523_R_CS3, PCF8523_B_CS3_PM_STD);
	if (err != 0) {
		device_printf(sc->dev, "cannot write CS3 reg\n");
		return (err);
	}
	pause_sbt("nxpbat", mstosbt(10), 0, 0);
	if ((err = read_reg(sc, PCF8523_R_CS3, &cs3)) != 0) {
		device_printf(sc->dev, "cannot read CS3 reg\n");
		return (err);
	}
	err = write_reg(sc, PCF8523_R_CS3, PCF8523_B_CS3_PM_DSNBM);
	if (err != 0) {
		device_printf(sc->dev, "cannot write CS3 reg\n");
		return (err);
	}

	if (cs3 & PCF8523_B_CS3_BLF)
		device_printf(sc->dev, "WARNING: RTC battery is low\n");

	return (0);
}

static int
pcf8523_start(struct nxprtc_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *tree;
	struct csr {
		uint8_t	cs1;
		uint8_t	cs2;
		uint8_t cs3;
		uint8_t sec;
	} csr;
	int err;
	uint8_t clkout, freqadj;

	/* Read the control and status registers. */
	if ((err = nxprtc_readfrom(sc->dev, PCF85xx_R_CS1, &csr,
	    sizeof(csr), WAITFLAGS)) != 0){
		device_printf(sc->dev, "cannot read RTC control regs\n");
		return (err);
	}

	/*
	 * Do a full init if...
	 *  - The chip power manager is in battery-disable mode.
	 *  - The OS (oscillator stopped) bit is set (all power was lost).
	 *  - The clock-increment STOP flag is set (this is just insane).
	 */
	if ((csr.cs3 & PCF8523_M_CS3_PM) == PCF8523_B_CS3_PM_NOBAT ||
	    (csr.cs1 & PCF85xx_B_CS1_STOP) || (csr.sec & PCF85xx_B_SECOND_OS)) {
		device_printf(sc->dev, 
		    "WARNING: RTC battery failed; time is invalid\n");

		/*
		 * For 212x series...
		 * - Turn off the POR-Override bit (used for mfg test only), 
		 *   by writing zero to cs 1 (all other bits power on as zero). 
		 * - Turn off the timestamp option to save the power used to
		 *   monitor that input pin.
		 * - Trigger OTP refresh by forcing the OTPR bit to zero then
		 *   back to 1, then wait 100ms for the refresh.
		 */
		if (sc->is212x) {
			err = write_reg(sc, PCF85xx_R_CS1, 0);
			if (err != 0) {
				device_printf(sc->dev,
				    "cannot write CS1 reg\n");
				return (err);
			}

			err = write_reg(sc, PCF2127_R_TS_CTL, PCF2127_B_TSOFF);
			if (err != 0) {
				device_printf(sc->dev,
				    "cannot write timestamp control\n");
				return (err);
			}

			clkout = PCF2129_B_CLKOUT_HIGHZ;
			err = write_reg(sc, PCF8523_R_TMR_CLKOUT, clkout);
			if (err == 0)
				err = write_reg(sc, PCF8523_R_TMR_CLKOUT,
				    clkout | PCF2129_B_CLKOUT_OTPR);
			if (err != 0) {
				device_printf(sc->dev,
				    "cannot write CLKOUT control\n");
				return (err);
			}
			pause_sbt("nxpotp", mstosbt(100), mstosbt(10), 0);
		} else
			clkout = PCF8523_B_CLKOUT_HIGHZ;

		/* All chips: set clock output pin to high-z to save power */
		if ((err = write_reg(sc, PCF8523_R_TMR_CLKOUT, clkout)) != 0) {
			device_printf(sc->dev, "cannot write CLKOUT control\n");
			return (err);
		}
	}

	/*
	 * Check the battery voltage and report if it's low.  This also has the
	 * side effect of (re-)initializing the power manager to low-power mode
	 * when we come up after a power fail.
	 */
	pcf8523_battery_check(sc);

	/*
	 * Remember whether we're running in AM/PM mode.  The chip default is
	 * 24-hour mode, but if we're co-existing with some other OS that
	 * prefers AM/PM we can run that way too.
	 *
	 * Also, for 212x chips, retrieve the current frequency aging offset,
	 * and set up the sysctl handler for reading/setting it.
	 */
	if (sc->is212x) {
		if (csr.cs1 & PCF2129_B_CS1_12HR)
			sc->use_ampm = true;

		err = read_reg(sc, PCF2127_R_AGING_OFFSET, &freqadj);
		if (err != 0) {
			device_printf(sc->dev,
			    "cannot read AGINGOFFSET register\n");
			return (err);
		}
		sc->freqadj = (int8_t)freqadj;

		ctx = device_get_sysctl_ctx(sc->dev);
		tree = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));

		SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "freqadj",
		    CTLFLAG_RWTUN | CTLTYPE_INT | CTLFLAG_MPSAFE, sc, 0,
		    freqadj_sysctl, "I", "Frequency adjust in PPM, range [-7,+8]");
	} else {
		if (csr.cs1 & PCF8523_B_CS1_12HR)
			sc->use_ampm = true;
	}

	return (0);
}
static int
pcf8523_start_timer(struct nxprtc_softc *sc)
{
	int err;
	uint8_t clkout, stdclk, stdfreq, tmrfreq;

	/*
	 * Read the timer control and frequency regs.  If they don't have the
	 * values we normally program into them then the timer count doesn't
	 * contain a valid fractional second, so zero it to prevent using a bad
	 * value.  Then program the normal timer values so that on the first
	 * settime call we'll begin to use fractional time.
	 */
	if ((err = read_reg(sc, PCF8523_R_TMR_A_FREQ, &tmrfreq)) != 0)
		return (err);
	if ((err = read_reg(sc, PCF8523_R_TMR_CLKOUT, &clkout)) != 0)
		return (err);

	stdfreq = PCF8523_B_TMR_A_64HZ;
	stdclk = PCF8523_B_CLKOUT_TACD | PCF8523_B_CLKOUT_HIGHZ;

	if (clkout != stdclk || (tmrfreq & PCF8523_M_TMR_A_FREQ) != stdfreq) {
		if ((err = write_reg(sc, sc->tmcaddr, 0)) != 0)
			return (err);
		if ((err = write_reg(sc, PCF8523_R_TMR_A_FREQ, stdfreq)) != 0)
			return (err);
		if ((err = write_reg(sc, PCF8523_R_TMR_CLKOUT, stdclk)) != 0)
			return (err);
	}
	return (0);
}

static int
pcf2127_start_timer(struct nxprtc_softc *sc)
{
	int err;
	uint8_t stdctl, tmrctl;

	/*
	 * Set up timer if it's not already in the mode we normally run it.  See
	 * the comment in pcf8523_start_timer() for more details.
	 *
	 * Note that the PCF2129 datasheet says it has no countdown timer, but
	 * empirical testing shows that it works just fine for our purposes.
	 */
	if ((err = read_reg(sc, PCF2127_R_TMR_CTL, &tmrctl)) != 0)
		return (err);

	stdctl = PCF2127_B_TMR_CD | PCF8523_B_TMR_A_64HZ;

	if ((tmrctl & PCF2127_M_TMR_CTRL) != stdctl) {
		if ((err = write_reg(sc, sc->tmcaddr, 0)) != 0)
			return (err);
		if ((err = write_reg(sc, PCF2127_R_TMR_CTL, stdctl)) != 0)
			return (err);
	}
	return (0);
}

static int
pcf8563_start(struct nxprtc_softc *sc)
{
	struct csr {
		uint8_t	cs1;
		uint8_t	cs2;
		uint8_t sec;
	} csr;
	int err;

	/* Read the control and status registers. */
	if ((err = nxprtc_readfrom(sc->dev, PCF85xx_R_CS1, &csr,
	    sizeof(csr), WAITFLAGS)) != 0){
		device_printf(sc->dev, "cannot read RTC control regs\n");
		return (err);
	}

	/*
	 * Do a full init if...
	 *  - The OS (oscillator stopped) bit is set (all power was lost).  This
	 *    bit is called VL (Voltage Low) in the 8563 datasheet.
	 *  - The clock-increment STOP flag is set (this is just insane).
	 */
	if ((csr.cs1 & PCF85xx_B_CS1_STOP) || (csr.sec & PCF85xx_B_SECOND_OS)) {
		device_printf(sc->dev, 
		    "WARNING: RTC battery failed; time is invalid\n");
		/*
		 * - Turn off the POR-Override bit (used for mfg test only), by
		 *   writing zero to cs 1 (all other bits power on as zero).
		 * - Turn off the clock output pin (to save battery power), by
		 *   writing zero to the clkout control reg.
		 */
		if ((err = write_reg(sc, PCF85xx_R_CS1, 0)) != 0) {
			device_printf(sc->dev, "cannot write CS1 reg\n");
			return (err);
		}

		if ((err = write_reg(sc, PCF8563_R_CLKOUT, 0)) != 0) {
			device_printf(sc->dev, "cannot write CS1 reg\n");
			return (err);
		}
	}

	return (0);
}

static int
pcf8563_start_timer(struct nxprtc_softc *sc)
{
	int err;
	uint8_t stdctl, tmrctl;

	/* See comment in pcf8523_start_timer().  */
	if ((err = read_reg(sc, PCF8563_R_TMR_CTRL, &tmrctl)) != 0)
		return (err);

	stdctl = PCF8563_B_TMR_ENABLE | PCF8563_B_TMR_64HZ;

	if ((tmrctl & PCF8563_M_TMR_CTRL) != stdctl) {
		if ((err = write_reg(sc, sc->tmcaddr, 0)) != 0)
			return (err);
		if ((err = write_reg(sc, PCF8563_R_TMR_CTRL, stdctl)) != 0)
			return (err);
	}
	return (0);
}

static void
nxprtc_start(void *dev)
{
	struct nxprtc_softc *sc;
	int clockflags, resolution;

	sc = device_get_softc((device_t)dev);
	config_intrhook_disestablish(&sc->config_hook);

	/* First do chip-specific inits. */
	switch (sc->chiptype) {
	case TYPE_PCA2129:
	case TYPE_PCF2129:
	case TYPE_PCF2127:
		sc->is212x = true;
		if (pcf8523_start(sc) != 0)
			return;
		if (pcf2127_start_timer(sc) != 0) {
			device_printf(sc->dev, "cannot set up timer\n");
			return;
		}
		break;
	case TYPE_PCF8523:
		if (pcf8523_start(sc) != 0)
			return;
		if (pcf8523_start_timer(sc) != 0) {
			device_printf(sc->dev, "cannot set up timer\n");
			return;
		}
		break;
	case TYPE_PCA8565:
	case TYPE_PCF8563:
		if (pcf8563_start(sc) != 0)
			return;
		if (pcf8563_start_timer(sc) != 0) {
			device_printf(sc->dev, "cannot set up timer\n");
			return;
		}
		break;
	default:
		device_printf(sc->dev, "missing init code for this chiptype\n");
		return;
	}

	/*
	 * Everything looks good if we make it to here; register as an RTC.  If
	 * we're using the timer to count fractional seconds, our resolution is
	 * 1e6/64, about 15.6ms.  Without the timer we still align the RTC clock
	 * when setting it so our error is an average .5s when reading it.
	 * Schedule our clock_settime() method to be called at a .495ms offset
	 * into the second, because the clock hardware resets the divider chain
	 * to the mid-second point when you set the time and it takes about 5ms
	 * of i2c bus activity to set the clock.
	 */
	resolution = sc->use_timer ? 1000000 / TMR_TICKS_SEC : 1000000 / 2;
	clockflags = CLOCKF_GETTIME_NO_ADJ | CLOCKF_SETTIME_NO_TS;
	clock_register_flags(sc->dev, resolution, clockflags);
	clock_schedule(sc->dev, 495000000);
}

static int
nxprtc_gettime(device_t dev, struct timespec *ts)
{
	struct bcd_clocktime bct;
	struct time_regs tregs;
	struct nxprtc_softc *sc;
	int err;
	uint8_t cs1, hourmask, tmrcount;

	sc = device_get_softc(dev);

	/*
	 * Read the time, but before using it, validate that the oscillator-
	 * stopped/power-fail bit is not set, and that the time-increment STOP
	 * bit is not set in the control reg.  The latter can happen if there
	 * was an error when setting the time.
	 */
	if ((err = iicbus_request_bus(sc->busdev, sc->dev, IIC_WAIT)) == 0) {
		if ((err = read_timeregs(sc, &tregs, &tmrcount)) == 0) {
			err = read_reg(sc, PCF85xx_R_CS1, &cs1);
		}
		iicbus_release_bus(sc->busdev, sc->dev);
	}
	if (err != 0)
		return (err);

	if ((tregs.sec & PCF85xx_B_SECOND_OS) || (cs1 & PCF85xx_B_CS1_STOP)) {
		device_printf(dev, "RTC clock not running\n");
		return (EINVAL); /* hardware is good, time is not. */
	}

	if (sc->use_ampm)
		hourmask = PCF85xx_M_12HOUR;
	else
		hourmask = PCF85xx_M_24HOUR;

	bct.nsec = ((uint64_t)tmrcount * 1000000000) / TMR_TICKS_SEC;
	bct.ispm = (tregs.hour & PCF8523_B_HOUR_PM) != 0;
	bct.sec  = tregs.sec   & PCF85xx_M_SECOND;
	bct.min  = tregs.min   & PCF85xx_M_MINUTE;
	bct.hour = tregs.hour  & hourmask;
	bct.day  = tregs.day   & PCF85xx_M_DAY;
	bct.mon  = tregs.month & PCF85xx_M_MONTH;
	bct.year = tregs.year  & PCF85xx_M_YEAR;

	/*
	 * Old PCF8563 datasheets recommended that the C bit be 1 for 19xx and 0
	 * for 20xx; newer datasheets don't recommend that.  We don't care,
	 * but we may co-exist with other OSes sharing the hardware. Determine
	 * existing polarity on a read so that we can preserve it on a write.
	 */
	if (sc->chiptype == TYPE_PCF8563) {
		if (tregs.month & PCF8563_B_MONTH_C) {
			if (bct.year < 0x70)
				sc->flags |= SC_F_CPOL;
		} else if (bct.year >= 0x70)
				sc->flags |= SC_F_CPOL;
	}

	clock_dbgprint_bcd(sc->dev, CLOCK_DBG_READ, &bct); 
	err = clock_bcd_to_ts(&bct, ts, sc->use_ampm);
	ts->tv_sec += utc_offset();

	return (err);
}

static int
nxprtc_settime(device_t dev, struct timespec *ts)
{
	struct bcd_clocktime bct;
	struct time_regs tregs;
	struct nxprtc_softc *sc;
	int err;
	uint8_t cflag, cs1;

	sc = device_get_softc(dev);

	/*
	 * We stop the clock, set the time, then restart the clock.  Half a
	 * second after restarting the clock it ticks over to the next second.
	 * So to align the RTC, we schedule this function to be called when
	 * system time is roughly halfway (.495) through the current second.
	 *
	 * Reserve use of the i2c bus and stop the RTC clock.  Note that if
	 * anything goes wrong from this point on, we leave the clock stopped,
	 * because we don't really know what state it's in.
	 */
	if ((err = iicbus_request_bus(sc->busdev, sc->dev, IIC_WAIT)) != 0)
		return (err);
	if ((err = read_reg(sc, PCF85xx_R_CS1, &cs1)) != 0)
		goto errout;
	cs1 |= PCF85xx_B_CS1_STOP;
	if ((err = write_reg(sc, PCF85xx_R_CS1, cs1)) != 0)
		goto errout;

	/* Grab a fresh post-sleep idea of what time it is. */
	getnanotime(ts);
	ts->tv_sec -= utc_offset();
	ts->tv_nsec = 0;
	clock_ts_to_bcd(ts, &bct, sc->use_ampm);
	clock_dbgprint_bcd(sc->dev, CLOCK_DBG_WRITE, &bct);

	/* On 8563 set the century based on the polarity seen when reading. */
	cflag = 0;
	if (sc->chiptype == TYPE_PCF8563) {
		if ((sc->flags & SC_F_CPOL) != 0) {
			if (bct.year >= 0x2000)
				cflag = PCF8563_B_MONTH_C;
		} else if (bct.year < 0x2000)
				cflag = PCF8563_B_MONTH_C;
	}

	tregs.sec   = bct.sec;
	tregs.min   = bct.min;
	tregs.hour  = bct.hour | (bct.ispm ? PCF8523_B_HOUR_PM : 0);
	tregs.day   = bct.day;
	tregs.month = bct.mon;
	tregs.year  = (bct.year & 0xff) | cflag;
	tregs.wday  = bct.dow;

	/*
	 * Set the time, reset the timer count register, then start the clocks.
	 */
	if ((err = write_timeregs(sc, &tregs)) != 0)
		goto errout;

	if ((err = write_reg(sc, sc->tmcaddr, TMR_TICKS_SEC)) != 0)
		return (err);

	cs1 &= ~PCF85xx_B_CS1_STOP;
	err = write_reg(sc, PCF85xx_R_CS1, cs1);

	/*
	 * Check for battery-low.  The actual check is throttled to only occur
	 * once a day, mostly to avoid spamming the user with frequent warnings.
	 */
	pcf8523_battery_check(sc);

errout:

	iicbus_release_bus(sc->busdev, sc->dev);

	if (err != 0)
		device_printf(dev, "cannot write RTC time\n");

	return (err);
}

static int
nxprtc_get_chiptype(device_t dev)
{
#ifdef FDT

	return (ofw_bus_search_compatible(dev, compat_data)->ocd_data);
#else
	nxprtc_compat_data *cdata;
	const char *htype;
	int chiptype;

	/*
	 * If given a chiptype hint string, loop through the ofw compat data
	 * comparing the hinted chip type to the compat strings.  The table end
	 * marker ocd_data is TYPE_NONE.
	 */
	if (resource_string_value(device_get_name(dev), 
	    device_get_unit(dev), "compatible", &htype) == 0) {
		for (cdata = compat_data; cdata->ocd_str != NULL; ++cdata) {
			if (strcmp(htype, cdata->ocd_str) == 0)
				break;
		}
		chiptype = cdata->ocd_data;
	} else
		chiptype = TYPE_NONE;

	/*
	 * On non-FDT systems the historical behavior of this driver was to
	 * assume a PCF8563; keep doing that for compatibility.
	 */
	if (chiptype == TYPE_NONE)
		return (TYPE_PCF8563);
	else
		return (chiptype);
#endif
}

static int
nxprtc_probe(device_t dev)
{
	int chiptype, rv;

#ifdef FDT
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	rv = BUS_PROBE_GENERIC;
#else
	rv = BUS_PROBE_NOWILDCARD;
#endif
	if ((chiptype = nxprtc_get_chiptype(dev)) == TYPE_NONE)
		return (ENXIO);

	device_set_desc(dev, desc_strings[chiptype]);
	return (rv);
}

static int
nxprtc_attach(device_t dev)
{
	struct nxprtc_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->busdev = device_get_parent(dev);

	/* We need to know what kind of chip we're driving. */
	sc->chiptype = nxprtc_get_chiptype(dev);

	/* The features and some register addresses vary by chip type. */
	switch (sc->chiptype) {
	case TYPE_PCA2129:
	case TYPE_PCF2129:
	case TYPE_PCF2127:
	case TYPE_PCF8523:
		sc->secaddr = PCF8523_R_SECOND;
		sc->tmcaddr = PCF8523_R_TMR_A_COUNT;
		sc->use_timer = true;
		break;
	case TYPE_PCA8565:
	case TYPE_PCF8563:
		sc->secaddr = PCF8563_R_SECOND;
		sc->tmcaddr = PCF8563_R_TMR_COUNT;
		sc->use_timer = true;
		break;
	default:
		device_printf(dev, "impossible: cannot determine chip type\n");
		return (ENXIO);
	}

	/*
	 * We have to wait until interrupts are enabled.  Sometimes I2C read
	 * and write only works when the interrupts are available.
	 */
	sc->config_hook.ich_func = nxprtc_start;
	sc->config_hook.ich_arg = dev;
	if (config_intrhook_establish(&sc->config_hook) != 0)
		return (ENOMEM);

	return (0);
}

static int
nxprtc_detach(device_t dev)
{

	clock_unregister(dev);
	return (0);
}

static device_method_t nxprtc_methods[] = {
	DEVMETHOD(device_probe,		nxprtc_probe),
	DEVMETHOD(device_attach,	nxprtc_attach),
	DEVMETHOD(device_detach,	nxprtc_detach),

	DEVMETHOD(clock_gettime,	nxprtc_gettime),
	DEVMETHOD(clock_settime,	nxprtc_settime),

	DEVMETHOD_END
};

static driver_t nxprtc_driver = {
	"nxprtc",
	nxprtc_methods,
	sizeof(struct nxprtc_softc),
};

DRIVER_MODULE(nxprtc, iicbus, nxprtc_driver, NULL, NULL);
MODULE_VERSION(nxprtc, 1);
MODULE_DEPEND(nxprtc, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
IICBUS_FDT_PNP_INFO(compat_data);
