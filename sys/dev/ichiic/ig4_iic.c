/*
 * Copyright (c) 2014 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com> and was subsequently ported
 * to FreeBSD by Michael Gmelin <freebsd@grem.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Intel fourth generation mobile cpus integrated I2C device.
 *
 * See ig4_reg.h for datasheet reference and notes.
 * See ig4_var.h for locking semantics.
 */

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/kdb.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sx.h>
#include <sys/syslog.h>
#include <sys/bus.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <sys/rman.h>

#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>
#endif

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/ichiic/ig4_reg.h>
#include <dev/ichiic/ig4_var.h>

#define DO_POLL(sc)	(cold || kdb_active || SCHEDULER_STOPPED() || sc->poll)

/*
 * tLOW, tHIGH periods of the SCL clock and maximal falling time of both
 * lines are taken from I2C specifications.
 */
#define	IG4_SPEED_STD_THIGH	4000	/* nsec */
#define	IG4_SPEED_STD_TLOW	4700	/* nsec */
#define	IG4_SPEED_STD_TF_MAX	300	/* nsec */
#define	IG4_SPEED_FAST_THIGH	600	/* nsec */
#define	IG4_SPEED_FAST_TLOW	1300	/* nsec */
#define	IG4_SPEED_FAST_TF_MAX	300	/* nsec */

/*
 * Ig4 hardware parameters except Haswell are taken from intel_lpss driver
 */
static const struct ig4_hw ig4iic_hw[] = {
	[IG4_HASWELL] = {
		.ic_clock_rate = 100,	/* MHz */
		.sda_hold_time = 90,	/* nsec */
		.txfifo_depth = 32,
		.rxfifo_depth = 32,
	},
	[IG4_ATOM] = {
		.ic_clock_rate = 100,
		.sda_fall_time = 280,
		.scl_fall_time = 240,
		.sda_hold_time = 60,
		.txfifo_depth = 32,
		.rxfifo_depth = 32,
	},
	[IG4_SKYLAKE] = {
		.ic_clock_rate = 120,
		.sda_hold_time = 230,
	},
	[IG4_APL] = {
		.ic_clock_rate = 133,
		.sda_fall_time = 171,
		.scl_fall_time = 208,
		.sda_hold_time = 207,
	},
};

static void ig4iic_intr(void *cookie);
static void ig4iic_dump(ig4iic_softc_t *sc);

static int ig4_dump;
SYSCTL_INT(_debug, OID_AUTO, ig4_dump, CTLFLAG_RW,
	   &ig4_dump, 0, "Dump controller registers");

/*
 * Clock registers initialization control
 * 0 - Try read clock registers from ACPI and fallback to p.1.
 * 1 - Calculate values based on controller type (IC clock rate).
 * 2 - Use values inherited from DragonflyBSD driver (old behavior).
 * 3 - Keep clock registers intact.
 */
static int ig4_timings;
SYSCTL_INT(_debug, OID_AUTO, ig4_timings, CTLFLAG_RDTUN, &ig4_timings, 0,
    "Controller timings 0=ACPI, 1=predefined, 2=legacy, 3=do not change");

/*
 * Low-level inline support functions
 */
static __inline void
reg_write(ig4iic_softc_t *sc, uint32_t reg, uint32_t value)
{
	bus_write_4(sc->regs_res, reg, value);
	bus_barrier(sc->regs_res, reg, 4, BUS_SPACE_BARRIER_WRITE);
}

static __inline uint32_t
reg_read(ig4iic_softc_t *sc, uint32_t reg)
{
	uint32_t value;

	bus_barrier(sc->regs_res, reg, 4, BUS_SPACE_BARRIER_READ);
	value = bus_read_4(sc->regs_res, reg);
	return (value);
}

static void
set_intr_mask(ig4iic_softc_t *sc, uint32_t val)
{
	if (sc->intr_mask != val) {
		reg_write(sc, IG4_REG_INTR_MASK, val);
		sc->intr_mask = val;
	}
}

static int
intrstat2iic(ig4iic_softc_t *sc, uint32_t val)
{
	uint32_t src;

	if (val & IG4_INTR_RX_UNDER)
		reg_read(sc, IG4_REG_CLR_RX_UNDER);
	if (val & IG4_INTR_RX_OVER)
		reg_read(sc, IG4_REG_CLR_RX_OVER);
	if (val & IG4_INTR_TX_OVER)
		reg_read(sc, IG4_REG_CLR_TX_OVER);

	if (val & IG4_INTR_TX_ABRT) {
		src = reg_read(sc, IG4_REG_TX_ABRT_SOURCE);
		reg_read(sc, IG4_REG_CLR_TX_ABORT);
		/* User-requested abort. Not really a error */
		if (src & IG4_ABRTSRC_TRANSFER)
			return (IIC_ESTATUS);
		/* Master has lost arbitration */
		if (src & IG4_ABRTSRC_ARBLOST)
			return (IIC_EBUSBSY);
		/* Did not receive an acknowledge from the remote slave */
		if (src & (IG4_ABRTSRC_TXNOACK_ADDR7 |
			   IG4_ABRTSRC_TXNOACK_ADDR10_1 |
			   IG4_ABRTSRC_TXNOACK_ADDR10_2 |
			   IG4_ABRTSRC_TXNOACK_DATA |
			   IG4_ABRTSRC_GENCALL_NOACK))
			return (IIC_ENOACK);
		/* Programming errors */
		if (src & (IG4_ABRTSRC_GENCALL_READ |
			   IG4_ABRTSRC_NORESTART_START |
			   IG4_ABRTSRC_NORESTART_10))
			return (IIC_ENOTSUPP);
		/* Other errors */
		if (src & IG4_ABRTSRC_ACKED_START)
			return (IIC_EBUSERR);
	}
	/*
	 * TX_OVER, RX_OVER and RX_UNDER are caused by wrong RX/TX FIFO depth
	 * detection or driver's read/write pipelining errors.
	 */
	if (val & (IG4_INTR_TX_OVER | IG4_INTR_RX_OVER))
		return (IIC_EOVERFLOW);
	if (val & IG4_INTR_RX_UNDER)
		return (IIC_EUNDERFLOW);

	return (IIC_NOERR);
}

/*
 * Enable or disable the controller and wait for the controller to acknowledge
 * the state change.
 */
static int
set_controller(ig4iic_softc_t *sc, uint32_t ctl)
{
	int retry;
	int error;
	uint32_t v;

	/*
	 * When the controller is enabled, interrupt on STOP detect
	 * or receive character ready and clear pending interrupts.
	 */
	set_intr_mask(sc, 0);
	if (ctl & IG4_I2C_ENABLE)
		reg_read(sc, IG4_REG_CLR_INTR);

	reg_write(sc, IG4_REG_I2C_EN, ctl);
	error = IIC_ETIMEOUT;

	for (retry = 100; retry > 0; --retry) {
		v = reg_read(sc, IG4_REG_ENABLE_STATUS);
		if (((v ^ ctl) & IG4_I2C_ENABLE) == 0) {
			error = 0;
			break;
		}
		pause("i2cslv", 1);
	}
	return (error);
}

/*
 * Wait up to 25ms for the requested interrupt using a 25uS polling loop.
 */
static int
wait_intr(ig4iic_softc_t *sc, uint32_t intr)
{
	uint32_t v;
	int error;
	int txlvl = -1;
	u_int count_us = 0;
	u_int limit_us = 25000; /* 25ms */

	for (;;) {
		/*
		 * Check requested status
		 */
		v = reg_read(sc, IG4_REG_RAW_INTR_STAT);
		error = intrstat2iic(sc, v & IG4_INTR_ERR_MASK);
		if (error || (v & intr))
			break;

		/*
		 * When waiting for the transmit FIFO to become empty,
		 * reset the timeout if we see a change in the transmit
		 * FIFO level as progress is being made.
		 */
		if (intr & IG4_INTR_TX_EMPTY) {
			v = reg_read(sc, IG4_REG_TXFLR) & IG4_FIFOLVL_MASK;
			if (txlvl != v) {
				txlvl = v;
				count_us = 0;
			}
		}

		/*
		 * Stop if we've run out of time.
		 */
		if (count_us >= limit_us) {
			error = IIC_ETIMEOUT;
			break;
		}

		/*
		 * When polling is not requested let the interrupt do its work.
		 */
		if (!DO_POLL(sc)) {
			mtx_lock(&sc->io_lock);
			set_intr_mask(sc, intr | IG4_INTR_ERR_MASK);
			mtx_sleep(sc, &sc->io_lock, 0, "i2cwait",
				  (hz + 99) / 100); /* sleep up to 10ms */
			set_intr_mask(sc, 0);
			mtx_unlock(&sc->io_lock);
			count_us += 10000;
		} else {
			DELAY(25);
			count_us += 25;
		}
	}

	return (error);
}

/*
 * Set the slave address.  The controller must be disabled when
 * changing the address.
 *
 * This operation does not issue anything to the I2C bus but sets
 * the target address for when the controller later issues a START.
 */
static void
set_slave_addr(ig4iic_softc_t *sc, uint8_t slave)
{
	uint32_t tar;
	uint32_t ctl;
	int use_10bit;

	use_10bit = 0;
	if (sc->slave_valid && sc->last_slave == slave &&
	    sc->use_10bit == use_10bit) {
		return;
	}
	sc->use_10bit = use_10bit;

	/*
	 * Wait for TXFIFO to drain before disabling the controller.
	 */
	wait_intr(sc, IG4_INTR_TX_EMPTY);

	set_controller(sc, 0);
	ctl = reg_read(sc, IG4_REG_CTL);
	ctl &= ~IG4_CTL_10BIT;
	ctl |= IG4_CTL_RESTARTEN;

	tar = slave;
	if (sc->use_10bit) {
		tar |= IG4_TAR_10BIT;
		ctl |= IG4_CTL_10BIT;
	}
	reg_write(sc, IG4_REG_CTL, ctl);
	reg_write(sc, IG4_REG_TAR_ADD, tar);
	set_controller(sc, IG4_I2C_ENABLE);
	sc->slave_valid = 1;
	sc->last_slave = slave;
}

/*
 *				IICBUS API FUNCTIONS
 */
static int
ig4iic_xfer_start(ig4iic_softc_t *sc, uint16_t slave, bool repeated_start)
{
	set_slave_addr(sc, slave >> 1);

	if (!repeated_start) {
		/*
		 * Clear any previous TX/RX FIFOs overflow/underflow bits.
		 */
		reg_read(sc, IG4_REG_CLR_INTR);
	}

	return (0);
}

/*
 * Amount of unread data before next burst to get better I2C bus utilization.
 * 2 bytes is enough in FAST mode. 8 bytes is better in FAST+ and HIGH modes.
 * Intel-recommended value is 16 for DMA transfers with 64-byte depth FIFOs.
 */
#define	IG4_FIFO_LOWAT	2

static int
ig4iic_read(ig4iic_softc_t *sc, uint8_t *buf, uint16_t len,
    bool repeated_start, bool stop)
{
	uint32_t cmd;
	int requested = 0;
	int received = 0;
	int burst, target, lowat = 0;
	int error;

	if (len == 0)
		return (0);

	while (received < len) {
		burst = sc->cfg.txfifo_depth -
		    (reg_read(sc, IG4_REG_TXFLR) & IG4_FIFOLVL_MASK);
		if (burst <= 0) {
			error = wait_intr(sc, IG4_INTR_TX_EMPTY);
			if (error)
				break;
			burst = sc->cfg.txfifo_depth;
		}
		/* Ensure we have enough free space in RXFIFO */
		burst = MIN(burst, sc->cfg.rxfifo_depth - lowat);
		target = MIN(requested + burst, (int)len);
		while (requested < target) {
			cmd = IG4_DATA_COMMAND_RD;
			if (repeated_start && requested == 0)
				cmd |= IG4_DATA_RESTART;
			if (stop && requested == len - 1)
				cmd |= IG4_DATA_STOP;
			reg_write(sc, IG4_REG_DATA_CMD, cmd);
			requested++;
		}
		/* Leave some data queued to maintain the hardware pipeline */
		lowat = 0;
		if (requested != len && requested - received > IG4_FIFO_LOWAT)
			lowat = IG4_FIFO_LOWAT;
		/* After TXFLR fills up, clear it by reading available data */
		while (received < requested - lowat) {
			burst = MIN((int)len - received,
			    reg_read(sc, IG4_REG_RXFLR) & IG4_FIFOLVL_MASK);
			if (burst > 0) {
				while (burst--)
					buf[received++] = 0xFF &
					    reg_read(sc, IG4_REG_DATA_CMD);
			} else {
				error = wait_intr(sc, IG4_INTR_RX_FULL);
				if (error)
					goto out;
			}
		}
	}
out:
	return (error);
}

static int
ig4iic_write(ig4iic_softc_t *sc, uint8_t *buf, uint16_t len,
    bool repeated_start, bool stop)
{
	uint32_t cmd;
	int sent = 0;
	int burst, target;
	int error;

	if (len == 0)
		return (0);

	while (sent < len) {
		burst = sc->cfg.txfifo_depth -
		    (reg_read(sc, IG4_REG_TXFLR) & IG4_FIFOLVL_MASK);
		target = MIN(sent + burst, (int)len);
		while(sent < target) {
			cmd = buf[sent];
			if (repeated_start && sent == 0)
				cmd |= IG4_DATA_RESTART;
			if (stop && sent == len - 1)
				cmd |= IG4_DATA_STOP;
			reg_write(sc, IG4_REG_DATA_CMD, cmd);
			sent++;
		}
		if (sent < len) {
			error = wait_intr(sc, IG4_INTR_TX_EMPTY);
			if (error)
				break;
		}
	}

	return (error);
}

int
ig4iic_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	const char *reason = NULL;
	uint32_t i;
	int error;
	int unit;
	bool rpstart;
	bool stop;
	bool allocated;

	/*
	 * The hardware interface imposes limits on allowed I2C messages.
	 * It is not possible to explicitly send a start or stop.
	 * They are automatically sent (or not sent, depending on the
	 * configuration) when a data byte is transferred.
	 * For this reason it's impossible to send a message with no data
	 * at all (like an SMBus quick message).
	 * The start condition is automatically generated after the stop
	 * condition, so it's impossible to not have a start after a stop.
	 * The repeated start condition is automatically sent if a change
	 * of the transfer direction happens, so it's impossible to have
	 * a change of direction without a (repeated) start.
	 * The repeated start can be forced even without the change of
	 * direction.
	 * Changing the target slave address requires resetting the hardware
	 * state, so it's impossible to do that without the stop followed
	 * by the start.
	 */
	for (i = 0; i < nmsgs; i++) {
#if 0
		if (i == 0 && (msgs[i].flags & IIC_M_NOSTART) != 0) {
			reason = "first message without start";
			break;
		}
		if (i == nmsgs - 1 && (msgs[i].flags & IIC_M_NOSTOP) != 0) {
			reason = "last message without stop";
			break;
		}
#endif
		if (msgs[i].len == 0) {
			reason = "message with no data";
			break;
		}
		if (i > 0) {
			if ((msgs[i].flags & IIC_M_NOSTART) != 0 &&
			    (msgs[i - 1].flags & IIC_M_NOSTOP) == 0) {
				reason = "stop not followed by start";
				break;
			}
			if ((msgs[i - 1].flags & IIC_M_NOSTOP) != 0 &&
			    msgs[i].slave != msgs[i - 1].slave) {
				reason = "change of slave without stop";
				break;
			}
			if ((msgs[i].flags & IIC_M_NOSTART) != 0 &&
			    (msgs[i].flags & IIC_M_RD) !=
			    (msgs[i - 1].flags & IIC_M_RD)) {
				reason = "change of direction without repeated"
				    " start";
				break;
			}
		}
	}
	if (reason != NULL) {
		if (bootverbose)
			device_printf(dev, "%s\n", reason);
		return (IIC_ENOTSUPP);
	}

	/* Check if device is already allocated with iicbus_request_bus() */
	allocated = sx_xlocked(&sc->call_lock) != 0;
	if (!allocated)
		sx_xlock(&sc->call_lock);

	/* Debugging - dump registers. */
	if (ig4_dump) {
		unit = device_get_unit(dev);
		if (ig4_dump & (1 << unit)) {
			ig4_dump &= ~(1 << unit);
			ig4iic_dump(sc);
		}
	}

	/*
	 * Clear any previous abort condition that may have been holding
	 * the txfifo in reset.
	 */
	reg_read(sc, IG4_REG_CLR_TX_ABORT);

	rpstart = false;
	error = 0;
	for (i = 0; i < nmsgs; i++) {
		if ((msgs[i].flags & IIC_M_NOSTART) == 0) {
			error = ig4iic_xfer_start(sc, msgs[i].slave, rpstart);
		} else {
			if (!sc->slave_valid ||
			    (msgs[i].slave >> 1) != sc->last_slave) {
				device_printf(dev, "start condition suppressed"
				    "but slave address is not set up");
				error = EINVAL;
				break;
			}
			rpstart = false;
		}
		if (error != 0)
			break;

		stop = (msgs[i].flags & IIC_M_NOSTOP) == 0;
		if (msgs[i].flags & IIC_M_RD)
			error = ig4iic_read(sc, msgs[i].buf, msgs[i].len,
			    rpstart, stop);
		else
			error = ig4iic_write(sc, msgs[i].buf, msgs[i].len,
			    rpstart, stop);
		if (error != 0)
			break;

		rpstart = !stop;
	}

	if (!allocated)
		sx_unlock(&sc->call_lock);
	return (error);
}

int
ig4iic_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	bool allocated;

	allocated = sx_xlocked(&sc->call_lock) != 0;
	if (!allocated)
		sx_xlock(&sc->call_lock);

	/* TODO handle speed configuration? */
	if (oldaddr != NULL)
		*oldaddr = sc->last_slave << 1;
	set_slave_addr(sc, addr >> 1);
	if (addr == IIC_UNKNOWN)
		sc->slave_valid = false;

	if (!allocated)
		sx_unlock(&sc->call_lock);
	return (0);
}

int
ig4iic_callback(device_t dev, int index, caddr_t data)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int error = 0;
	int how;

	switch (index) {
	case IIC_REQUEST_BUS:
		/* force polling if ig4iic is requested with IIC_DONTWAIT */
		how = *(int *)data;
		if ((how & IIC_WAIT) == 0) {
			if (sx_try_xlock(&sc->call_lock) == 0)
				error = IIC_EBUSBSY;
			else
				sc->poll = true;
		} else
			sx_xlock(&sc->call_lock);
		break;

	case IIC_RELEASE_BUS:
		sc->poll = false;
		sx_unlock(&sc->call_lock);
		break;

	default:
		error = errno2iic(EINVAL);
	}

	return (error);
}

/*
 * Clock register values can be calculated with following rough equations:
 * SCL_HCNT = ceil(IC clock rate * tHIGH)
 * SCL_LCNT = ceil(IC clock rate * tLOW)
 * SDA_HOLD = ceil(IC clock rate * SDA hold time)
 * Precise equations take signal's falling, rising and spike suppression
 * times in to account. They can be found in Synopsys or Intel documentation.
 *
 * Here we snarf formulas and defaults from Linux driver to be able to use
 * timing values provided by Intel LPSS driver "as is".
 */
static int
ig4iic_clk_params(const struct ig4_hw *hw, int speed,
    uint16_t *scl_hcnt, uint16_t *scl_lcnt, uint16_t *sda_hold)
{
	uint32_t thigh, tlow, tf_max;	/* nsec */
	uint32_t sda_fall_time;		/* nsec */
        uint32_t scl_fall_time;		/* nsec */

	switch (speed) {
	case IG4_CTL_SPEED_STD:
		thigh = IG4_SPEED_STD_THIGH;
		tlow = IG4_SPEED_STD_TLOW;
		tf_max = IG4_SPEED_STD_TF_MAX;
		break;

	case IG4_CTL_SPEED_FAST:
		thigh = IG4_SPEED_FAST_THIGH;
		tlow = IG4_SPEED_FAST_TLOW;
		tf_max = IG4_SPEED_FAST_TF_MAX;
		break;

	default:
		return (EINVAL);
	}

	/* Use slowest falling time defaults to be on the safe side */
	sda_fall_time = hw->sda_fall_time == 0 ? tf_max : hw->sda_fall_time;
	*scl_hcnt = (uint16_t)
	    ((hw->ic_clock_rate * (thigh + sda_fall_time) + 500) / 1000 - 3);

	scl_fall_time = hw->scl_fall_time == 0 ? tf_max : hw->scl_fall_time;
	*scl_lcnt = (uint16_t)
	    ((hw->ic_clock_rate * (tlow + scl_fall_time) + 500) / 1000 - 1);

	/*
	 * There is no "known good" default value for tHD;DAT so keep SDA_HOLD
	 * intact if sda_hold_time value is not provided.
	 */
	if (hw->sda_hold_time != 0)
		*sda_hold = (uint16_t)
		    ((hw->ic_clock_rate * hw->sda_hold_time + 500) / 1000);

	return (0);
}

#ifdef DEV_ACPI
static ACPI_STATUS
ig4iic_acpi_params(ACPI_HANDLE handle, char *method,
    uint16_t *scl_hcnt, uint16_t *scl_lcnt, uint16_t *sda_hold)
{
	ACPI_BUFFER buf;
	ACPI_OBJECT *obj, *elems;
	ACPI_STATUS status;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;

	status = AcpiEvaluateObject(handle, method, NULL, &buf);
	if (ACPI_FAILURE(status))
		return (status);

	status = AE_TYPE;
	obj = (ACPI_OBJECT *)buf.Pointer;
	if (obj->Type == ACPI_TYPE_PACKAGE && obj->Package.Count == 3) {
		elems = obj->Package.Elements;
		*scl_hcnt = elems[0].Integer.Value & IG4_SCL_CLOCK_MASK;
		*scl_lcnt = elems[1].Integer.Value & IG4_SCL_CLOCK_MASK;
		*sda_hold = elems[2].Integer.Value & IG4_SDA_TX_HOLD_MASK;
		status = AE_OK;
	}

	AcpiOsFree(obj);

	return (status);
}
#endif /* DEV_ACPI */

static void
ig4iic_get_config(ig4iic_softc_t *sc)
{
	const struct ig4_hw *hw;
	uint32_t v;
#ifdef DEV_ACPI
	ACPI_HANDLE handle;
#endif
	/* Fetch default hardware config from controller */
	sc->cfg.version = reg_read(sc, IG4_REG_COMP_VER);
	sc->cfg.bus_speed = reg_read(sc, IG4_REG_CTL) & IG4_CTL_SPEED_MASK;
	sc->cfg.ss_scl_hcnt =
	    reg_read(sc, IG4_REG_SS_SCL_HCNT) & IG4_SCL_CLOCK_MASK;
	sc->cfg.ss_scl_lcnt =
	    reg_read(sc, IG4_REG_SS_SCL_LCNT) & IG4_SCL_CLOCK_MASK;
	sc->cfg.fs_scl_hcnt =
	    reg_read(sc, IG4_REG_FS_SCL_HCNT) & IG4_SCL_CLOCK_MASK;
	sc->cfg.fs_scl_lcnt =
	    reg_read(sc, IG4_REG_FS_SCL_LCNT) & IG4_SCL_CLOCK_MASK;
	sc->cfg.ss_sda_hold = sc->cfg.fs_sda_hold =
	    reg_read(sc, IG4_REG_SDA_HOLD) & IG4_SDA_TX_HOLD_MASK;

	if (sc->cfg.bus_speed != IG4_CTL_SPEED_STD)
		sc->cfg.bus_speed = IG4_CTL_SPEED_FAST;

	/* REG_COMP_PARAM1 is not documented in latest Intel specs */
	if (sc->version == IG4_HASWELL || sc->version == IG4_ATOM) {
		v = reg_read(sc, IG4_REG_COMP_PARAM1);
		if (IG4_PARAM1_TXFIFO_DEPTH(v) != 0)
			sc->cfg.txfifo_depth = IG4_PARAM1_TXFIFO_DEPTH(v);
		if (IG4_PARAM1_RXFIFO_DEPTH(v) != 0)
			sc->cfg.rxfifo_depth = IG4_PARAM1_RXFIFO_DEPTH(v);
	} else {
		/*
		 * Hardware does not allow FIFO Threshold Levels value to be
		 * set larger than the depth of the buffer. If an attempt is
		 * made to do that, the actual value set will be the maximum
		 * depth of the buffer.
		 */
		v = reg_read(sc, IG4_REG_TX_TL);
		reg_write(sc, IG4_REG_TX_TL, v | IG4_FIFO_MASK);
		sc->cfg.txfifo_depth =
		    (reg_read(sc, IG4_REG_TX_TL) & IG4_FIFO_MASK) + 1;
		reg_write(sc, IG4_REG_TX_TL, v);
		v = reg_read(sc, IG4_REG_RX_TL);
		reg_write(sc, IG4_REG_RX_TL, v | IG4_FIFO_MASK);
		sc->cfg.rxfifo_depth =
		    (reg_read(sc, IG4_REG_RX_TL) & IG4_FIFO_MASK) + 1;
		reg_write(sc, IG4_REG_RX_TL, v);
	}

	/* Override hardware config with IC_clock-based counter values */
	if (ig4_timings < 2 && sc->version < nitems(ig4iic_hw)) {
		hw = &ig4iic_hw[sc->version];
		sc->cfg.bus_speed = IG4_CTL_SPEED_FAST;
		ig4iic_clk_params(hw, IG4_CTL_SPEED_STD, &sc->cfg.ss_scl_hcnt,
		    &sc->cfg.ss_scl_lcnt, &sc->cfg.ss_sda_hold);
		ig4iic_clk_params(hw, IG4_CTL_SPEED_FAST, &sc->cfg.fs_scl_hcnt,
		    &sc->cfg.fs_scl_lcnt, &sc->cfg.fs_sda_hold);
		if (hw->txfifo_depth != 0)
			sc->cfg.txfifo_depth = hw->txfifo_depth;
		if (hw->rxfifo_depth != 0)
			sc->cfg.rxfifo_depth = hw->rxfifo_depth;
	} else if (ig4_timings == 2) {
		/*
		 * Timings of original ig4 driver:
		 * Program based on a 25000 Hz clock.  This is a bit of a
		 * hack (obviously).  The defaults are 400 and 470 for standard
		 * and 60 and 130 for fast.  The defaults for standard fail
		 * utterly (presumably cause an abort) because the clock time
		 * is ~18.8ms by default.  This brings it down to ~4ms.
		 */
		sc->cfg.bus_speed = IG4_CTL_SPEED_STD;
		sc->cfg.ss_scl_hcnt = sc->cfg.fs_scl_hcnt = 100;
		sc->cfg.ss_scl_lcnt = sc->cfg.fs_scl_lcnt = 125;
		if (sc->version == IG4_SKYLAKE)
			sc->cfg.ss_sda_hold = sc->cfg.fs_sda_hold = 28;
	}

#ifdef DEV_ACPI
	/* Evaluate SSCN and FMCN ACPI methods to fetch timings */
	if (ig4_timings == 0 && (handle = acpi_get_handle(sc->dev)) != NULL) {
		ig4iic_acpi_params(handle, "SSCN", &sc->cfg.ss_scl_hcnt,
		    &sc->cfg.ss_scl_lcnt, &sc->cfg.ss_sda_hold);
		ig4iic_acpi_params(handle, "FMCN", &sc->cfg.fs_scl_hcnt,
		    &sc->cfg.fs_scl_lcnt, &sc->cfg.fs_sda_hold);
	}
#endif

	if (bootverbose) {
		device_printf(sc->dev, "Controller parameters:\n");
		printf("  Speed: %s\n",
		    sc->cfg.bus_speed == IG4_CTL_SPEED_STD ? "Std" : "Fast");
		printf("  Regs:  HCNT  :LCNT  :SDAHLD\n");
		printf("  Std:   0x%04hx:0x%04hx:0x%04hx\n",
		    sc->cfg.ss_scl_hcnt, sc->cfg.ss_scl_lcnt,
		    sc->cfg.ss_sda_hold);
		printf("  Fast:  0x%04hx:0x%04hx:0x%04hx\n",
		    sc->cfg.fs_scl_hcnt, sc->cfg.fs_scl_lcnt,
		    sc->cfg.fs_sda_hold);
		printf("  FIFO:  RX:0x%04x: TX:0x%04x\n",
		    sc->cfg.rxfifo_depth, sc->cfg.txfifo_depth);
	}
}

static int
ig4iic_set_config(ig4iic_softc_t *sc)
{
	uint32_t v;

	v = reg_read(sc, IG4_REG_DEVIDLE_CTRL);
	if (sc->version == IG4_SKYLAKE && (v & IG4_RESTORE_REQUIRED) ) {
		reg_write(sc, IG4_REG_DEVIDLE_CTRL, IG4_DEVICE_IDLE | IG4_RESTORE_REQUIRED);
		reg_write(sc, IG4_REG_DEVIDLE_CTRL, 0);

		reg_write(sc, IG4_REG_RESETS_SKL, IG4_RESETS_ASSERT_SKL);
		reg_write(sc, IG4_REG_RESETS_SKL, IG4_RESETS_DEASSERT_SKL);
		DELAY(1000);
	}

	if (sc->version == IG4_ATOM)
		v = reg_read(sc, IG4_REG_COMP_TYPE);
	
	if (sc->version == IG4_HASWELL || sc->version == IG4_ATOM) {
		v = reg_read(sc, IG4_REG_COMP_PARAM1);
		v = reg_read(sc, IG4_REG_GENERAL);
		/*
		 * The content of IG4_REG_GENERAL is different for each
		 * controller version.
		 */
		if (sc->version == IG4_HASWELL &&
		    (v & IG4_GENERAL_SWMODE) == 0) {
			v |= IG4_GENERAL_SWMODE;
			reg_write(sc, IG4_REG_GENERAL, v);
			v = reg_read(sc, IG4_REG_GENERAL);
		}
	}

	if (sc->version == IG4_HASWELL) {
		v = reg_read(sc, IG4_REG_SW_LTR_VALUE);
		v = reg_read(sc, IG4_REG_AUTO_LTR_VALUE);
	} else if (sc->version == IG4_SKYLAKE) {
		v = reg_read(sc, IG4_REG_ACTIVE_LTR_VALUE);
		v = reg_read(sc, IG4_REG_IDLE_LTR_VALUE);
	}

	if (sc->version == IG4_HASWELL || sc->version == IG4_ATOM) {
		v = reg_read(sc, IG4_REG_COMP_VER);
		if (v < IG4_COMP_MIN_VER)
			return(ENXIO);
	}

	if (set_controller(sc, 0)) {
		device_printf(sc->dev, "controller error during attach-1\n");
		return (ENXIO);
	}

	reg_read(sc, IG4_REG_CLR_INTR);
	reg_write(sc, IG4_REG_INTR_MASK, 0);
	sc->intr_mask = 0;

	reg_write(sc, IG4_REG_SS_SCL_HCNT, sc->cfg.ss_scl_hcnt);
	reg_write(sc, IG4_REG_SS_SCL_LCNT, sc->cfg.ss_scl_lcnt);
	reg_write(sc, IG4_REG_FS_SCL_HCNT, sc->cfg.fs_scl_hcnt);
	reg_write(sc, IG4_REG_FS_SCL_LCNT, sc->cfg.fs_scl_lcnt);
	reg_write(sc, IG4_REG_SDA_HOLD,
	    (sc->cfg.bus_speed  & IG4_CTL_SPEED_MASK) == IG4_CTL_SPEED_STD ?
	      sc->cfg.ss_sda_hold : sc->cfg.fs_sda_hold);

	/*
	 * Use a threshold of 1 so we get interrupted on each character,
	 * allowing us to use mtx_sleep() in our poll code.  Not perfect
	 * but this is better than using DELAY() for receiving data.
	 *
	 * See ig4_var.h for details on interrupt handler synchronization.
	 */
	reg_write(sc, IG4_REG_RX_TL, 0);
	reg_write(sc, IG4_REG_TX_TL, 0);

	reg_write(sc, IG4_REG_CTL,
		  IG4_CTL_MASTER |
		  IG4_CTL_SLAVE_DISABLE |
		  IG4_CTL_RESTARTEN |
		  (sc->cfg.bus_speed & IG4_CTL_SPEED_MASK));

	return (0);
}

/*
 * Called from ig4iic_pci_attach/detach()
 */
int
ig4iic_attach(ig4iic_softc_t *sc)
{
	int error;

	mtx_init(&sc->io_lock, "IG4 I/O lock", NULL, MTX_DEF);
	sx_init(&sc->call_lock, "IG4 call lock");

	ig4iic_get_config(sc);

	error = ig4iic_set_config(sc);
	if (error)
		goto done;

	sc->iicbus = device_add_child(sc->dev, "iicbus", -1);
	if (sc->iicbus == NULL) {
		device_printf(sc->dev, "iicbus driver not found\n");
		error = ENXIO;
		goto done;
	}

#if 0
	/*
	 * Don't do this, it blows up the PCI config
	 */
	if (sc->version == IG4_HASWELL || sc->version == IG4_ATOM) {
		reg_write(sc, IG4_REG_RESETS_HSW, IG4_RESETS_ASSERT_HSW);
		reg_write(sc, IG4_REG_RESETS_HSW, IG4_RESETS_DEASSERT_HSW);
	} else if (sc->version = IG4_SKYLAKE) {
		reg_write(sc, IG4_REG_RESETS_SKL, IG4_RESETS_ASSERT_SKL);
		reg_write(sc, IG4_REG_RESETS_SKL, IG4_RESETS_DEASSERT_SKL);
	}
#endif

	if (set_controller(sc, IG4_I2C_ENABLE)) {
		device_printf(sc->dev, "controller error during attach-2\n");
		error = ENXIO;
		goto done;
	}
	if (set_controller(sc, 0)) {
		device_printf(sc->dev, "controller error during attach-3\n");
		error = ENXIO;
		goto done;
	}
	error = bus_setup_intr(sc->dev, sc->intr_res, INTR_TYPE_MISC | INTR_MPSAFE,
			       NULL, ig4iic_intr, sc, &sc->intr_handle);
	if (error) {
		device_printf(sc->dev,
			      "Unable to setup irq: error %d\n", error);
	}

	error = bus_generic_attach(sc->dev);
	if (error) {
		device_printf(sc->dev,
			      "failed to attach child: error %d\n", error);
	}

done:
	return (error);
}

int
ig4iic_detach(ig4iic_softc_t *sc)
{
	int error;

	if (device_is_attached(sc->dev)) {
		error = bus_generic_detach(sc->dev);
		if (error)
			return (error);
	}
	if (sc->iicbus)
		device_delete_child(sc->dev, sc->iicbus);
	if (sc->intr_handle)
		bus_teardown_intr(sc->dev, sc->intr_res, sc->intr_handle);

	sx_xlock(&sc->call_lock);

	sc->iicbus = NULL;
	sc->intr_handle = NULL;
	reg_write(sc, IG4_REG_INTR_MASK, 0);
	set_controller(sc, 0);

	sx_xunlock(&sc->call_lock);

	mtx_destroy(&sc->io_lock);
	sx_destroy(&sc->call_lock);

	return (0);
}

int
ig4iic_suspend(ig4iic_softc_t *sc)
{
	int error;

	/* suspend all children */
	error = bus_generic_suspend(sc->dev);

	sx_xlock(&sc->call_lock);
	set_controller(sc, 0);
	if (sc->version == IG4_SKYLAKE) {
		/*
		 * Place the device in the idle state, just to be safe
		 */
		reg_write(sc, IG4_REG_DEVIDLE_CTRL, IG4_DEVICE_IDLE);
		/*
		 * Controller can become dysfunctional if I2C lines are pulled
		 * down when suspend procedure turns off power to I2C device.
		 * Place device in the reset state to avoid this.
		 */
		reg_write(sc, IG4_REG_RESETS_SKL, IG4_RESETS_ASSERT_SKL);
	}
	sx_xunlock(&sc->call_lock);

	return (error);
}

int ig4iic_resume(ig4iic_softc_t *sc)
{
	int error;

	sx_xlock(&sc->call_lock);
	if (ig4iic_set_config(sc))
		device_printf(sc->dev, "controller error during resume\n");
	/* Force setting of the target address on the next transfer */
	sc->slave_valid = 0;
	sx_xunlock(&sc->call_lock);

	error = bus_generic_resume(sc->dev);

	return (error);
}

/*
 * Interrupt Operation, see ig4_var.h for locking semantics.
 */
static void
ig4iic_intr(void *cookie)
{
	ig4iic_softc_t *sc = cookie;

	mtx_lock(&sc->io_lock);
	/* Ignore stray interrupts */
	if (sc->intr_mask != 0 && reg_read(sc, IG4_REG_INTR_STAT) != 0) {
		/* Interrupt bits are cleared in wait_intr() loop */
		set_intr_mask(sc, 0);
		wakeup(sc);
	}
	mtx_unlock(&sc->io_lock);
}

#define REGDUMP(sc, reg)	\
	device_printf(sc->dev, "  %-23s %08x\n", #reg, reg_read(sc, reg))

static void
ig4iic_dump(ig4iic_softc_t *sc)
{
	device_printf(sc->dev, "ig4iic register dump:\n");
	REGDUMP(sc, IG4_REG_CTL);
	REGDUMP(sc, IG4_REG_TAR_ADD);
	REGDUMP(sc, IG4_REG_SS_SCL_HCNT);
	REGDUMP(sc, IG4_REG_SS_SCL_LCNT);
	REGDUMP(sc, IG4_REG_FS_SCL_HCNT);
	REGDUMP(sc, IG4_REG_FS_SCL_LCNT);
	REGDUMP(sc, IG4_REG_INTR_STAT);
	REGDUMP(sc, IG4_REG_INTR_MASK);
	REGDUMP(sc, IG4_REG_RAW_INTR_STAT);
	REGDUMP(sc, IG4_REG_RX_TL);
	REGDUMP(sc, IG4_REG_TX_TL);
	REGDUMP(sc, IG4_REG_I2C_EN);
	REGDUMP(sc, IG4_REG_I2C_STA);
	REGDUMP(sc, IG4_REG_TXFLR);
	REGDUMP(sc, IG4_REG_RXFLR);
	REGDUMP(sc, IG4_REG_SDA_HOLD);
	REGDUMP(sc, IG4_REG_TX_ABRT_SOURCE);
	REGDUMP(sc, IG4_REG_SLV_DATA_NACK);
	REGDUMP(sc, IG4_REG_DMA_CTRL);
	REGDUMP(sc, IG4_REG_DMA_TDLR);
	REGDUMP(sc, IG4_REG_DMA_RDLR);
	REGDUMP(sc, IG4_REG_SDA_SETUP);
	REGDUMP(sc, IG4_REG_ENABLE_STATUS);
	REGDUMP(sc, IG4_REG_COMP_PARAM1);
	REGDUMP(sc, IG4_REG_COMP_VER);
	if (sc->version == IG4_ATOM) {
		REGDUMP(sc, IG4_REG_COMP_TYPE);
		REGDUMP(sc, IG4_REG_CLK_PARMS);
	}
	if (sc->version == IG4_HASWELL || sc->version == IG4_ATOM) {
		REGDUMP(sc, IG4_REG_RESETS_HSW);
		REGDUMP(sc, IG4_REG_GENERAL);
	} else if (sc->version == IG4_SKYLAKE) {
		REGDUMP(sc, IG4_REG_RESETS_SKL);
	}
	if (sc->version == IG4_HASWELL) {
		REGDUMP(sc, IG4_REG_SW_LTR_VALUE);
		REGDUMP(sc, IG4_REG_AUTO_LTR_VALUE);
	} else if (sc->version == IG4_SKYLAKE) {
		REGDUMP(sc, IG4_REG_ACTIVE_LTR_VALUE);
		REGDUMP(sc, IG4_REG_IDLE_LTR_VALUE);
	}
}
#undef REGDUMP

devclass_t ig4iic_devclass;

DRIVER_MODULE(iicbus, ig4iic, iicbus_driver, iicbus_devclass, NULL, NULL);
MODULE_DEPEND(ig4iic, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(ig4iic, 1);
