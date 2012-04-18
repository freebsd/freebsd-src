/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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
 * Texas Instruments TWL4030/TWL5030/TWL60x0/TPS659x0 Power Management and
 * Audio CODEC devices.
 *
 * This code is based on the Linux TWL multifunctional device driver, which is
 * copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * These chips are typically used as support ICs for the OMAP range of embedded
 * ARM processes/SOC from Texas Instruments.  They are typically used to control
 * on board voltages, however some variants have other features like audio
 * codecs, USB OTG transceivers, RTC, PWM, etc.
 *
 * Currently this driver focuses on the voltage regulator side of things,
 * however in future it might be wise to split up this driver so that there is
 * one base driver used for communication and other child drivers that
 * manipulate the various modules on the chip.
 *
 * Voltage Regulators
 * ------------------
 * - Voltage regulators can belong to different power groups, in this driver we
 *   put the regulators under our control in the "Application power group".
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/mutex.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/iicbus/iiconf.h>

#include "twl.h"
#include "twl_vreg.h"

/* The register sets are divided up into groups with the following base address
 * and I2C addresses.
 */

/*
 * XXX cognet: the locking is plain wrong, but we can't just keep locks while
 * calling functions that can sleep, such as malloc() or iicbus_transfer
 */

/*
 * Power Groups bits for the 4030 and 6030 devices
 */
#define TWL4030_P3_GRP		0x80	/* Peripherals, power group */
#define TWL4030_P2_GRP		0x40	/* Modem power group */
#define TWL4030_P1_GRP		0x20	/* Application power group (FreeBSD control) */

#define TWL6030_P3_GRP		0x04	/* Modem power group */
#define TWL6030_P2_GRP		0x02	/* Connectivity power group */
#define TWL6030_P1_GRP		0x01	/* Application power group (FreeBSD control) */

/*
 * Register offsets within a LDO regulator register set
 */
#define TWL_VREG_GRP		0x00	/* Regulator GRP register */
#define TWL_VREG_STATE		0x02
#define TWL_VREG_VSEL		0x03	/* Voltage select register */

#define UNDF  0xFFFF

static const uint16_t twl6030_voltages[] = {
	0000, 1000, 1100, 1200, 1300, 1400, 1500, 1600,
	1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400,
	2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200,
	3300, UNDF, UNDF, UNDF, UNDF, UNDF, UNDF, 2750
};

static const uint16_t twl4030_vaux1_voltages[] = {
	1500, 1800, 2500, 2800, 3000, 3000, 3000, 3000
};
static const uint16_t twl4030_vaux2_voltages[] = {
	1700, 1700, 1900, 1300, 1500, 1800, 2000, 2500,
	2100, 2800, 2200, 2300, 2400, 2400, 2400, 2400
};
static const uint16_t twl4030_vaux3_voltages[] = {
	1500, 1800, 2500, 2800, 3000, 3000, 3000, 3000
};
static const uint16_t twl4030_vaux4_voltages[] = {
	700,  1000, 1200, 1300, 1500, 1800, 1850, 2500,
	2600, 2800, 2850, 3000, 3150, 3150, 3150, 3150
};
static const uint16_t twl4030_vmmc1_voltages[] = {
	1850, 2850, 3000, 3150
};
static const uint16_t twl4030_vmmc2_voltages[] = {
	1000, 1000, 1200, 1300, 1500, 1800, 1850, 2500,
	2600, 2800, 2850, 3000, 3150, 3150, 3150, 3150
};
static const uint16_t twl4030_vpll1_voltages[] = {
	1000, 1200, 1300, 1800, 2800, 3000, 3000, 3000
};
static const uint16_t twl4030_vpll2_voltages[] = {
	700,  1000, 1200, 1300, 1500, 1800, 1850, 2500,
	2600, 2800, 2850, 3000, 3150, 3150, 3150, 3150
};
static const uint16_t twl4030_vsim_voltages[] = {
	1000, 1200, 1300, 1800, 2800, 3000, 3000, 3000
};
static const uint16_t twl4030_vdac_voltages[] = {
	1200, 1300, 1800, 1800
};
static const uint16_t twl4030_vdd1_voltages[] = {
	800, 1450
};
static const uint16_t twl4030_vdd2_voltages[] = {
	800, 1450, 1500
};
static const uint16_t twl4030_vio_voltages[] = {
	1800, 1850
};
static const uint16_t twl4030_vintana2_voltages[] = {
	2500, 2750
};

/**
 *  Support voltage regulators for the different IC's
 */
struct twl_regulator {
	const char	*name;
	uint8_t		subdev;
	uint8_t		regbase;

	uint16_t	fixedvoltage;

	const uint16_t	*voltages;
	uint32_t	num_voltages;
};

#define TWL_REGULATOR_ADJUSTABLE(name, subdev, reg, voltages) \
	{ name, subdev, reg, 0, voltages, (sizeof(voltages)/sizeof(voltages[0])) }
#define TWL_REGULATOR_FIXED(name, subdev, reg, voltage) \
	{ name, subdev, reg, voltage, NULL, 0 }

static const struct twl_regulator twl4030_regulators[] = {
	TWL_REGULATOR_ADJUSTABLE("vaux1",    0, 0x17, twl4030_vaux1_voltages),
	TWL_REGULATOR_ADJUSTABLE("vaux2",    0, 0x1B, twl4030_vaux2_voltages),
	TWL_REGULATOR_ADJUSTABLE("vaux3",    0, 0x1F, twl4030_vaux3_voltages),
	TWL_REGULATOR_ADJUSTABLE("vaux4",    0, 0x23, twl4030_vaux4_voltages),
	TWL_REGULATOR_ADJUSTABLE("vmmc1",    0, 0x27, twl4030_vmmc1_voltages),
	TWL_REGULATOR_ADJUSTABLE("vmmc2",    0, 0x2B, twl4030_vmmc2_voltages),
	TWL_REGULATOR_ADJUSTABLE("vpll1",    0, 0x2F, twl4030_vpll1_voltages),
	TWL_REGULATOR_ADJUSTABLE("vpll2",    0, 0x33, twl4030_vpll2_voltages),
	TWL_REGULATOR_ADJUSTABLE("vsim",     0, 0x37, twl4030_vsim_voltages),
	TWL_REGULATOR_ADJUSTABLE("vdac",     0, 0x3B, twl4030_vdac_voltages),
	TWL_REGULATOR_ADJUSTABLE("vintana2", 0, 0x43, twl4030_vintana2_voltages),
	TWL_REGULATOR_FIXED("vintana1", 0, 0x3F, 1500),
	TWL_REGULATOR_FIXED("vintdig",  0, 0x47, 1500),
	TWL_REGULATOR_FIXED("vusb1v5",  0, 0x71, 1500),
	TWL_REGULATOR_FIXED("vusb1v8",  0, 0x74, 1800),
	TWL_REGULATOR_FIXED("vusb3v1",  0, 0x77, 3100),
	{ NULL, 0, 0x00, 0, NULL, 0 }
};

static const struct twl_regulator twl6030_regulators[] = {
	TWL_REGULATOR_ADJUSTABLE("vaux1", 0, 0x84, twl6030_voltages),
	TWL_REGULATOR_ADJUSTABLE("vaux2", 0, 0x89, twl6030_voltages),
	TWL_REGULATOR_ADJUSTABLE("vaux3", 0, 0x8C, twl6030_voltages),
	TWL_REGULATOR_ADJUSTABLE("vmmc",  0, 0x98, twl6030_voltages),
	TWL_REGULATOR_ADJUSTABLE("vpp",   0, 0x9C, twl6030_voltages),
	TWL_REGULATOR_ADJUSTABLE("vusim", 0, 0xA4, twl6030_voltages),
	TWL_REGULATOR_FIXED("vmem",  0, 0x64, 1800),
	TWL_REGULATOR_FIXED("vusb",  0, 0xA0, 3300),
	TWL_REGULATOR_FIXED("v1v8",  0, 0x46, 1800),
	TWL_REGULATOR_FIXED("v2v1",  0, 0x4C, 2100),
	TWL_REGULATOR_FIXED("v1v29", 0, 0x40, 1290),
	TWL_REGULATOR_FIXED("vcxio", 0, 0x90, 1800),
	TWL_REGULATOR_FIXED("vdac",  0, 0x94, 1800),
	TWL_REGULATOR_FIXED("vana",  0, 0x80, 2100),
	{ NULL, 0, 0x00, 0, NULL, 0 } 
};

#define TWL_VREG_MAX_NAMELEN  32

/**
 *  Linked list of voltage regulators support by the device.
 */
struct twl_regulator_entry {
	LIST_ENTRY(twl_regulator_entry) entries;

	char			name[TWL_VREG_MAX_NAMELEN];
	struct sysctl_oid	*oid;

	uint8_t			sub_dev;
	uint8_t			reg_off;

	uint16_t		fixed_voltage;

	const uint16_t		*supp_voltages;
	uint32_t		num_supp_voltages;
};

/**
 *	Structure that stores the driver context.
 *
 *	This structure is allocated during driver attach.
 */
struct twl_vreg_softc {
	device_t			sc_dev;
	device_t			sc_pdev;

	struct mtx			sc_mtx;

	struct intr_config_hook sc_init_hook;

	LIST_HEAD(twl_regulator_list, twl_regulator_entry) sc_vreg_list;
};

/**
 *	Macros for driver mutex locking
 */
#define TWL_VREG_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	TWL_VREG_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define TWL_VREG_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	         "twl_vreg", MTX_DEF)
#define TWL_VREG_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define TWL_VREG_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define TWL_VREG_ASSERT_UNLOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static int debug;

/**
 *	twl_is_4030 - returns true if the device is TWL4030
 *	twl_is_6025 - returns true if the device is TWL6025
 *	twl_is_6030 - returns true if the device is TWL6030
 *	@sc: device soft context
 *
 *	Returns a non-zero value if the device matches.
 *
 *	LOCKING:
 *	None.
 *
 *	RETURNS:
 *	Returns a non-zero value if the device matches, otherwise zero.
 */
static int
twl_is_4030(device_t dev)
{

	return (0);
}

static int
twl_is_6025(device_t dev)
{

	return (0);
}

static int
twl_is_6030(device_t dev)
{

	return (1);
}

/**
 *	twl_vreg_read_1 - read one or more registers from the TWL device
 *	@sc: device soft context
 *	@nsub: the sub-module to read from
 *	@reg: the register offset within the module to read
 *	@buf: buffer to store the bytes in
 *	@cnt: the number of bytes to read
 *
 *	Reads one or registers and stores the result in the suppled buffer.
 *
 *	LOCKING:
 *	Expects the TWL lock to be held.
 *
 *	RETURNS:
 *	Zero on success or a negative error code on failure.
 */
static inline int
twl_vreg_read_1(struct twl_vreg_softc *sc, struct twl_regulator_entry *regulator,
    uint8_t off, uint8_t *val)
{
	return (twl_read(sc->sc_pdev, regulator->sub_dev, 
	    regulator->reg_off + off, val, 1));
}

/**
 *	twl_write - writes one or more registers to the TWL device
 *	@sc: device soft context
 *	@nsub: the sub-module to read from
 *	@reg: the register offset within the module to read
 *	@buf: data to write
 *	@cnt: the number of bytes to write
 *
 *	Writes one or more registers.
 *
 *	LOCKING:
 *	Expects the TWL lock to be held.
 *
 *	RETURNS:
 *	Zero on success or a negative error code on failure.
 */
static inline int
twl_vreg_write_1(struct twl_vreg_softc *sc, struct twl_regulator_entry *regulator,
    uint8_t off, uint8_t val)
{
	return (twl_write(sc->sc_pdev, regulator->sub_dev,
	    regulator->reg_off + off, &val, 1));
}

/**
 *	twl_millivolt_to_vsel - gets the vsel bit value to write into the register
 *	                        for a desired voltage
 *	@sc: the device soft context
 *	@regulator: pointer to the regulator device
 *	@millivolts: the millivolts to find the bit value for
 *	@bitval: upond return will contain the value to write
 *	@diff: upon return will contain the difference between the requested
 *	       voltage value and the actual voltage (in millivolts)
 *
 *  Accepts a voltage value and tries to find the closest match to the actual
 *	supported voltages.  If a match is found within 100mv of the target, the
 *	function returns the VSEL value.  If no voltage match is found the function
 *	returns UNDEF.
 *
 *	LOCKING:
 *	It's expected the TWL lock is held while this function is called.
 *
 *	RETURNS:
 *	A value to load into the VSEL register if matching voltage found, if not
 *	EINVAL is returned.
 */
static int
twl_vreg_millivolt_to_vsel(struct twl_vreg_softc *sc,
    struct twl_regulator_entry *regulator, int millivolts)
{
	int delta, smallest_delta;
	unsigned i, closest_idx;

	TWL_VREG_ASSERT_LOCKED(sc);

	/* First check that variable voltage is supported */
	if (regulator->supp_voltages == NULL)
		return (EINVAL);

	/* Loop over the support voltages and try and find the closest match */
	closest_idx = 0;
	smallest_delta = 0x7fffffff;
	for (i = 0; i < regulator->num_supp_voltages; i++) {

		/* Ignore undefined values */
		if (regulator->supp_voltages[i] == UNDF)
			continue;

		/* Calculate the difference */
		delta = millivolts - (int)regulator->supp_voltages[i];
		if (abs(delta) < smallest_delta) {
			smallest_delta = abs(delta);
			closest_idx = i;
		}
	}

	/* Check we got a voltage that was within 150mv of the actual target, this
	 * is just a value I picked out of thin air.
	 */
	if (smallest_delta > 100)
		return (EINVAL);

	return (closest_idx);
}

/**
 *	twl_regulator_is_enabled - returns the enabled status of the regulator
 *	@sc: the device soft context
 *	@regulator: pointer to the regulator device
 *
 *
 *
 *	LOCKING:
 *	It's expected the TWL lock is held while this function is called.
 *
 *	RETURNS:
 *	Zero if disabled, positive non-zero value if enabled, on failure a -1
 */
static int
twl_vreg_is_regulator_enabled(struct twl_vreg_softc *sc,
    struct twl_regulator_entry *regulator)
{
	int err;
	uint8_t grp;
	uint8_t state;

	/* The status reading is different for the different devices */
	if (twl_is_4030(sc->sc_dev)) {

		err = twl_vreg_read_1(sc, regulator, TWL_VREG_GRP, &state);
		if (err)
			return (-1);

		return (state & TWL4030_P1_GRP);

	} else if (twl_is_6030(sc->sc_dev) || twl_is_6025(sc->sc_dev)) {

		/* Check the regulator is in the application group */
		if (twl_is_6030(sc->sc_dev)) {
			err = twl_vreg_read_1(sc, regulator, TWL_VREG_GRP, &grp);
			if (err)
				return (-1);

			if (!(grp & TWL6030_P1_GRP))
				return (0);
		}

		/* Read the application mode state and verify it's ON */
		err = twl_vreg_read_1(sc, regulator, TWL_VREG_STATE, &state);
		if (err)
			return (-1);

		return ((state & 0x0C) == 0x04);
	}

	return (-1);
}

/**
 *	twl_disable_regulator - disables the voltage regulator
 *	@sc: the device soft context
 *	@regulator: pointer to the regulator device
 *
 *  Disables the regulator which will stop the out drivers.
 *
 *	LOCKING:
 *	It's expected the TWL lock is held while this function is called.
 *
 *	RETURNS:
 *	Zero on success, or otherwise a negative error code.
 */
static int
twl_vreg_disable_regulator(struct twl_vreg_softc *sc,
    struct twl_regulator_entry *regulator)
{
	int err = 0;
	uint8_t grp;

	if (twl_is_4030(sc->sc_dev)) {

		/* Read the regulator CFG_GRP register */
		err = twl_vreg_read_1(sc, regulator, TWL_VREG_GRP, &grp);
		if (err)
			return (err);

		/* On the TWL4030 we just need to remove the regulator from all the
		 * power groups.
		 */
		grp &= ~(TWL4030_P1_GRP | TWL4030_P2_GRP | TWL4030_P3_GRP);
		err = twl_vreg_write_1(sc, regulator, TWL_VREG_GRP, grp);

	} else if (twl_is_6030(sc->sc_dev) || twl_is_6025(sc->sc_dev)) {

		/* On TWL6030 we need to make sure we disable power for all groups */
		if (twl_is_6030(sc->sc_dev))
			grp = TWL6030_P1_GRP | TWL6030_P2_GRP | TWL6030_P3_GRP;
		else
			grp = 0x00;

		/* Write the resource state to "OFF" */
		err = twl_vreg_write_1(sc, regulator, TWL_VREG_STATE, (grp << 5));
	}

	return (err);
}

/**
 *	twl_enable_regulator - enables the voltage regulator
 *	@sc: the device soft context
 *	@regulator: pointer to the regulator device
 *
 *  Enables the regulator which will enable the voltage out.
 *
 *	LOCKING:
 *	It's expected the TWL lock is held while this function is called.
 *
 *	RETURNS:
 *	Zero on success, or otherwise a negative error code.
 */
static int
twl_vreg_enable_regulator(struct twl_vreg_softc *sc,
    struct twl_regulator_entry *regulator)
{
	int err;
	uint8_t grp;

	/* Read the regulator CFG_GRP register */
	err = twl_vreg_read_1(sc, regulator, TWL_VREG_GRP, &grp);
	if (err)
		return (err);

	/* Enable the regulator by ensuring it's in the application power group
	 * and is in the "on" state.
	 */
	if (twl_is_4030(sc->sc_dev)) {

		/* On the TWL4030 we just need to ensure the regulator is in the right
		 * power domain, don't need to turn on explicitly like TWL6030.
		 */
		grp |= TWL4030_P1_GRP;
		err = twl_vreg_write_1(sc, regulator, TWL_VREG_GRP, grp);

	} else if (twl_is_6030(sc->sc_dev) || twl_is_6025(sc->sc_dev)) {

		if (twl_is_6030(sc->sc_dev) && !(grp & TWL6030_P1_GRP)) {
			grp |= TWL6030_P1_GRP;
			err = twl_vreg_write_1(sc, regulator, TWL_VREG_GRP, grp);
			if (err)
				return (err);
		}

		/* Write the resource state to "ON" */
		err = twl_vreg_write_1(sc, regulator, TWL_VREG_STATE, (grp << 5) | 0x01);
	}

	return (err);
}

/**
 *	twl_vreg_write_regulator_voltage - sets the voltage on a regulator with given name
 *	@sc: the device soft context
 *	@regulator: pointer to the regulator structure
 *	@millivolts: the voltage to set
 *
 *  Sets the voltage output on a given regulator.
 *
 *	LOCKING:
 *	It's expected the TWL lock is held while this function is called.
 *
 *	RETURNS:
 *	EIO if device is not present, otherwise 0 is returned.
 */
static int
twl_vreg_write_regulator_voltage(struct twl_vreg_softc *sc,
    struct twl_regulator_entry *regulator, int millivolts)
{
	int err;
	int vsel;

	/* If millivolts is zero then we simply disable the output */
	if (millivolts == 0)
		return (twl_vreg_disable_regulator(sc, regulator));

	/* If the regulator has a fixed voltage then check the setting matches
	 * and simply enable.
	 */
	if (regulator->supp_voltages == NULL || regulator->num_supp_voltages == 0) {
		if (millivolts != regulator->fixed_voltage)
			return (EINVAL);

		return (twl_vreg_enable_regulator(sc, regulator));
	}

	/* Get the VSEL value for the given voltage */
	vsel = twl_vreg_millivolt_to_vsel(sc, regulator, millivolts);
	if (vsel < 0)
		return (vsel);

	/* Next set the voltage on the variable voltage regulators */
	err = twl_vreg_write_1(sc, regulator, TWL_VREG_VSEL, (vsel & 0x1f));
	if (err != 0)
		return (err);

	if (debug)
		device_printf(sc->sc_dev, "%s : setting voltage to %dmV (vsel: 0x%x)\n",
		    regulator->name, millivolts, vsel);

	return (twl_vreg_enable_regulator(sc, regulator));
}

/**
 *	twl_read_regulator_voltage - reads the voltage on a given regulator
 *	@sc: the device soft context
 *	@regulator: pointer to the regulator structure
 *	@millivolts: upon return will contain the voltage on the regulator
 *
 *  Reads the voltage on a regulator.
 *
 *	LOCKING:
 *	It's expected the TWL lock is held while this function is called.
 *
 *	RETURNS:
 *	Zero on success, or otherwise a negative error code.
 */
static int
twl_vreg_read_regulator_voltage(struct twl_vreg_softc *sc,
    struct twl_regulator_entry *regulator, int *millivolts)
{
	int err;
	int ret;
	uint8_t vsel;

	/* Check if the regulator is currently enabled */
	if ((ret = twl_vreg_is_regulator_enabled(sc, regulator)) < 0)
		return (EINVAL);

	if (ret == 0) {
		*millivolts = 0;
		return (0);
	}

	/* Not all voltages are adjustable */
	if (regulator->supp_voltages == NULL || !regulator->num_supp_voltages) {
		*millivolts = regulator->fixed_voltage;
		return (0);
	}

	/* For variable voltages read the voltage register */
	err = twl_vreg_read_1(sc, regulator, TWL_VREG_VSEL, &vsel);
	if (err)
		return (err);

	/* Convert the voltage */
	vsel &= (regulator->num_supp_voltages - 1);
	if (regulator->supp_voltages[vsel] == UNDF) {
		*millivolts = 0;
		return (EINVAL);
	}

	*millivolts = regulator->supp_voltages[vsel];

	if (debug)
		device_printf(sc->sc_dev, "%s : reading voltage is %dmV (vsel: 0x%x)\n",
		    regulator->name, *millivolts, vsel);

	return (err);
}

/**
 *	twl_vreg_get_voltage - public interface to read the voltage on a regulator
 *	@dev: TWL PMIC device
 *	@name: the name of the regulator to read the voltage of
 *	@millivolts: pointer to an integer that upon return will contain the mV
 *
 *
 *
 *	LOCKING:
 *	Internally the function takes and releases the TWL lock.
 *
 *	RETURNS:
 *	Zero on success or a negative error code on failure.
 */
int
twl_vreg_get_voltage(device_t dev, const char *name, int *millivolts)
{
	struct twl_vreg_softc *sc;
	struct twl_regulator_entry *regulator;
	int found = 0;
	int err = EINVAL;

	if (millivolts == NULL)
		return (EINVAL);

	/* Get the device context, take the lock and find the matching regulator */
	sc = device_get_softc(dev);


	/* Find the regulator with the matching name */
	LIST_FOREACH(regulator, &sc->sc_vreg_list, entries) {
		if (strcmp(regulator->name, name) == 0) {
			found = 1;
			break;
		}
	}

	/* Sanity check that we found the regulator */
	if (found)
		err = twl_vreg_read_regulator_voltage(sc, regulator, millivolts);

	return (err);
}

/**
 *	twl_vreg_set_voltage - public interface to write the voltage on a regulator
 *	@dev: TWL PMIC device
 *	@name: the name of the regulator to read the voltage of
 *	@millivolts: the voltage to set in millivolts
 *
 *
 *
 *	LOCKING:
 *	Internally the function takes and releases the TWL lock.
 *
 *	RETURNS:
 *	Zero on success or a negative error code on failure.
 */
int
twl_vreg_set_voltage(device_t dev, const char *name, int millivolts)
{
	struct twl_vreg_softc *sc;
	struct twl_regulator_entry *regulator;
	int found = 0;
	int err = EINVAL;

	/* Get the device context, take the lock and find the matching regulator */
	sc = device_get_softc(dev);

	/* Find the regulator with the matching name */
	LIST_FOREACH(regulator, &sc->sc_vreg_list, entries) {
		if (strcmp(regulator->name, name) == 0) {
			found = 1;
			break;
		}
	}

	/* Sanity check that we found the regulator */
	if (found)
		err = twl_vreg_write_regulator_voltage(sc, regulator, millivolts);

	return (err);
}

/**
 *	twl_sysctl_voltage - reads or writes the voltage for a regulator
 *	@SYSCTL_HANDLER_ARGS: arguments for the callback
 *
 *	Callback for the sysctl entry on the
 *
 *	LOCKING:
 *	It's expected the TWL lock is held while this function is called.
 *
 *	RETURNS:
 *	EIO if device is not present, otherwise 0 is returned.
 */
static int
twl_vreg_sysctl_voltage(SYSCTL_HANDLER_ARGS)
{
	struct twl_vreg_softc *sc = (struct twl_vreg_softc*)arg1;
	struct twl_regulator_entry *regulator;
	int voltage;
	int found = 0;

	TWL_VREG_LOCK(sc);

	/* Find the regulator with the matching name */
	LIST_FOREACH(regulator, &sc->sc_vreg_list, entries) {
		if (strcmp(regulator->name, oidp->oid_name) == 0) {
			found = 1;
			break;
		}
	}

	/* Sanity check that we found the regulator */
	if (!found) {
		TWL_VREG_UNLOCK(sc);
		return (EINVAL);
	}
	TWL_VREG_UNLOCK(sc);

	twl_vreg_read_regulator_voltage(sc, regulator, &voltage);

	return sysctl_handle_int(oidp, &voltage, 0, req);
}

/**
 *	twl_add_regulator - adds single voltage regulator sysctls for the device
 *	@sc: device soft context
 *	@name: the name of the regulator
 *	@nsub: the number of the subdevice
 *	@regbase: the base address of the voltage regulator registers
 *
 *	Adds a voltage regulator to the device and also a sysctl interface for the
 *	regulator.
 *
 *	LOCKING:
 *	It's expected the TWL lock is held while this function is called.
 *
 *	RETURNS:
 *	Pointer to the new regulator entry on success, otherwise on failure NULL.
 */
static struct twl_regulator_entry*
twl_vreg_add_regulator(struct twl_vreg_softc *sc, const char *name,
    uint8_t nsub, uint8_t regbase, uint16_t fixed_voltage,
    const uint16_t *voltages, uint32_t num_voltages)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);
	struct twl_regulator_entry *new, *tmp;

	TWL_VREG_ASSERT_LOCKED(sc);
	TWL_VREG_UNLOCK(sc);

	new = malloc(sizeof(struct twl_regulator_entry), M_DEVBUF, M_NOWAIT | M_ZERO);
	TWL_VREG_LOCK(sc);
	if (new == NULL)
		return (NULL);

	/*
	 * We had to drop the lock while calling malloc(), maybe 
	 * the regulator got added in the meanwhile.
	 */
	   
	LIST_FOREACH(tmp, &sc->sc_vreg_list, entries) {
		if (!strncmp(new->name, name, strlen(new->name)) &&
		    new->sub_dev == nsub && new->reg_off == regbase) {
			free(new, M_DEVBUF);
			return (NULL);
		}
	}
	/* Copy over the name and register details */
	strncpy(new->name, name, TWL_VREG_MAX_NAMELEN);
	new->name[TWL_VREG_MAX_NAMELEN - 1] = '\0';

	new->sub_dev = nsub;
	new->reg_off = regbase;

	new->fixed_voltage = fixed_voltage;

	new->supp_voltages = voltages;
	new->num_supp_voltages = num_voltages;

	/* 
	 * We're in the list now, so we should be protected against double
	 * inclusion.
	 */
	   
	TWL_VREG_UNLOCK(sc);
	/* Add a sysctl entry for the voltage */
	new->oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, name,
	    CTLTYPE_INT | CTLFLAG_RD, sc, 0,
	    twl_vreg_sysctl_voltage, "I", "voltage regulator");
	TWL_VREG_LOCK(sc);

	/* Finally add the regulator to list of supported regulators */
	LIST_INSERT_HEAD(&sc->sc_vreg_list, new, entries);

	return (new);
}

/**
 *	twl_vreg_add_regulators - adds hint'ed voltage regulators to the device
 *	@sc: device soft context
 *	@chip: the name of the chip used in the hints
 *	@regulators: the list of possible voltage regulators
 *
 *  Loops over the list of regulators and matches up with the hint'ed values,
 *	adjusting the actual voltage based on the supplied values.
 *
 *	LOCKING:
 *	It's expected the TWL lock is held while this function is called.
 *
 *	RETURNS:
 *	Always returns 0.
 */
static int
twl_vreg_add_regulators(struct twl_vreg_softc *sc,
    const struct twl_regulator *regulators)
{
	int millivolts;
	const struct twl_regulator *walker;
	struct twl_regulator_entry *entry;

	TWL_VREG_ASSERT_LOCKED(sc);

	/* Uses hints to determine which regulators are needed to be supported,
	 * then if they are a new sysctl is added.
	 */
	walker = &regulators[0];
	while (walker->name != NULL) {

		/* Add the regulator to the list */
		entry = twl_vreg_add_regulator(sc, walker->name, walker->subdev,
		    walker->regbase, walker->fixedvoltage,
		    walker->voltages, walker->num_voltages);
		if (entry == NULL)
			continue;

		/* TODO: set voltage from FDT if required */

		/* Read the current voltage and print the info */
		TWL_VREG_UNLOCK(sc);
		twl_vreg_read_regulator_voltage(sc, entry, &millivolts);
		TWL_VREG_LOCK(sc);
		if (debug)
			device_printf(sc->sc_dev, "%s : %d mV\n", walker->name, millivolts);

		walker++;
	}

	return (0);
}

/**
 *	twl_scan - disables IRQ's on the given channel
 *	@ch: the channel to disable IRQ's on
 *
 *	Disable interupt generation for the given channel.
 *
 *	RETURNS:
 *	BUS_PROBE_NOWILDCARD
 */
static void
twl_vreg_init(void *dev)
{
	struct twl_vreg_softc *sc;

	sc = device_get_softc((device_t)dev);

	TWL_VREG_LOCK(sc);

	/* Scan the hints and add any regulators specified */
	if (twl_is_4030(sc->sc_dev))
		twl_vreg_add_regulators(sc, twl4030_regulators);
	else if (twl_is_6030(sc->sc_dev) || twl_is_6025(sc->sc_dev))
		twl_vreg_add_regulators(sc, twl6030_regulators);

	TWL_VREG_UNLOCK(sc);

	config_intrhook_disestablish(&sc->sc_init_hook);
}

/**
 *	twl_probe - disables IRQ's on the given channel
 *	@ch: the channel to disable IRQ's on
 *
 *	Disable interupt generation for the given channel.
 *
 *	RETURNS:
 *	BUS_PROBE_NOWILDCARD
 */
static int
twl_vreg_probe(device_t dev)
{

	device_set_desc(dev, "TI TWL4030/TWL5030/TWL60x0/TPS659x0 Voltage Regulators");
	return (0);
}

/**
 *	twl_attach - disables IRQ's on the given channel
 *	@ch: the channel to disable IRQ's on
 *
 *	Disable interupt generation for the given channel.
 *
 *	RETURNS:
 *	EH_HANDLED or EH_NOT_HANDLED
 */
static int
twl_vreg_attach(device_t dev)
{
	struct twl_vreg_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_pdev = device_get_parent(dev);

	TWL_VREG_LOCK_INIT(sc);

	/* Initialise the head of the list of voltage regulators supported by the
	 * TWL device.
	 */
	LIST_INIT(&sc->sc_vreg_list);

	/* We have to wait until interrupts are enabled. I2C read and write
	 * only works if the interrupts are available.
	 */
	sc->sc_init_hook.ich_func = twl_vreg_init;
	sc->sc_init_hook.ich_arg = dev;

	if (config_intrhook_establish(&sc->sc_init_hook) != 0)
		return (ENOMEM);

	return (0);
}

/**
 *	twl_vreg_detach - disables IRQ's on the given channel
 *	@ch: the channel to disable IRQ's on
 *
 *	Disable interupt generation for the given channel.
 *
 *	RETURNS:
 *	EH_HANDLED or EH_NOT_HANDLED
 */
static int
twl_vreg_detach(device_t dev)
{
	struct twl_vreg_softc *sc;
	struct twl_regulator_entry *regulator;
	struct twl_regulator_entry *tmp;

	sc = device_get_softc(dev);

	/* Take the lock and free all the added regulators */
	TWL_VREG_LOCK(sc);

	/* Find the regulator with the matching name */
	LIST_FOREACH_SAFE(regulator, &sc->sc_vreg_list, entries, tmp) {

		/* Remove from the list and clean up */
		LIST_REMOVE(regulator, entries);
		sysctl_remove_oid(regulator->oid, 1, 0);
		free(regulator, M_DEVBUF);
	}

	TWL_VREG_UNLOCK(sc);

	TWL_VREG_LOCK_DESTROY(sc);

	return (0);
}

static device_method_t twl_vreg_methods[] = {
	DEVMETHOD(device_probe,		twl_vreg_probe),
	DEVMETHOD(device_attach,	twl_vreg_attach),
	DEVMETHOD(device_detach,	twl_vreg_detach),

	{0, 0},
};

static driver_t twl_vreg_driver = {
	"twl_vreg",
	twl_vreg_methods,
	sizeof(struct twl_vreg_softc),
};

static devclass_t twl_vreg_devclass;

DRIVER_MODULE(twl_vreg, twl, twl_vreg_driver, twl_vreg_devclass, 0, 0);
MODULE_VERSION(twl_vreg, 1);
