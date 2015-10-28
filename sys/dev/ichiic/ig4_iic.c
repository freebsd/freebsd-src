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
 * Intel fourth generation mobile cpus integrated I2C device, smbus driver.
 *
 * See ig4_reg.h for datasheet reference and notes.
 * See ig4_var.h for locking semantics.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/syslog.h>
#include <sys/bus.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/smbus/smbconf.h>

#include <dev/ichiic/ig4_reg.h>
#include <dev/ichiic/ig4_var.h>

#define TRANS_NORMAL	1
#define TRANS_PCALL	2
#define TRANS_BLOCK	3

static void ig4iic_start(void *xdev);
static void ig4iic_intr(void *cookie);
static void ig4iic_dump(ig4iic_softc_t *sc);

static int ig4_dump;
SYSCTL_INT(_debug, OID_AUTO, ig4_dump, CTLFLAG_RW,
	   &ig4_dump, 0, "Dump controller registers");

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

	reg_write(sc, IG4_REG_I2C_EN, ctl);
	error = SMB_ETIMEOUT;

	for (retry = 100; retry > 0; --retry) {
		v = reg_read(sc, IG4_REG_ENABLE_STATUS);
		if (((v ^ ctl) & IG4_I2C_ENABLE) == 0) {
			error = 0;
			break;
		}
		mtx_sleep(sc, &sc->io_lock, 0, "i2cslv", 1);
	}
	return (error);
}

/*
 * Wait up to 25ms for the requested status using a 25uS polling loop.
 */
static int
wait_status(ig4iic_softc_t *sc, uint32_t status)
{
	uint32_t v;
	int error;
	int txlvl = -1;
	u_int count_us = 0;
	u_int limit_us = 25000; /* 25ms */

	error = SMB_ETIMEOUT;

	for (;;) {
		/*
		 * Check requested status
		 */
		v = reg_read(sc, IG4_REG_I2C_STA);
		if (v & status) {
			error = 0;
			break;
		}

		/*
		 * When waiting for receive data break-out if the interrupt
		 * loaded data into the FIFO.
		 */
		if (status & IG4_STATUS_RX_NOTEMPTY) {
			if (sc->rpos != sc->rnext) {
				error = 0;
				break;
			}
		}

		/*
		 * When waiting for the transmit FIFO to become empty,
		 * reset the timeout if we see a change in the transmit
		 * FIFO level as progress is being made.
		 */
		if (status & IG4_STATUS_TX_EMPTY) {
			v = reg_read(sc, IG4_REG_TXFLR) & IG4_FIFOLVL_MASK;
			if (txlvl != v) {
				txlvl = v;
				count_us = 0;
			}
		}

		/*
		 * Stop if we've run out of time.
		 */
		if (count_us >= limit_us)
			break;

		/*
		 * When waiting for receive data let the interrupt do its
		 * work, otherwise poll with the lock held.
		 */
		if (status & IG4_STATUS_RX_NOTEMPTY) {
			mtx_sleep(sc, &sc->io_lock, 0, "i2cwait",
				  (hz + 99) / 100); /* sleep up to 10ms */
			count_us += 10000;
		} else {
			DELAY(25);
			count_us += 25;
		}
	}

	return (error);
}

/*
 * Read I2C data.  The data might have already been read by
 * the interrupt code, otherwise it is sitting in the data
 * register.
 */
static uint8_t
data_read(ig4iic_softc_t *sc)
{
	uint8_t c;

	if (sc->rpos == sc->rnext) {
		c = (uint8_t)reg_read(sc, IG4_REG_DATA_CMD);
	} else {
		c = sc->rbuf[sc->rpos & IG4_RBUFMASK];
		++sc->rpos;
	}
	return (c);
}

/*
 * Set the slave address.  The controller must be disabled when
 * changing the address.
 *
 * This operation does not issue anything to the I2C bus but sets
 * the target address for when the controller later issues a START.
 */
static void
set_slave_addr(ig4iic_softc_t *sc, uint8_t slave, int trans_op)
{
	uint32_t tar;
	uint32_t ctl;
	int use_10bit;

	use_10bit = sc->use_10bit;
	if (trans_op & SMB_TRANS_7BIT)
		use_10bit = 0;
	if (trans_op & SMB_TRANS_10BIT)
		use_10bit = 1;

	if (sc->slave_valid && sc->last_slave == slave &&
	    sc->use_10bit == use_10bit) {
		return;
	}
	sc->use_10bit = use_10bit;

	/*
	 * Wait for TXFIFO to drain before disabling the controller.
	 *
	 * If a write message has not been completed it's really a
	 * programming error, but for now in that case issue an extra
	 * byte + STOP.
	 *
	 * If a read message has not been completed it's also a programming
	 * error, for now just ignore it.
	 */
	wait_status(sc, IG4_STATUS_TX_NOTFULL);
	if (sc->write_started) {
		reg_write(sc, IG4_REG_DATA_CMD, IG4_DATA_STOP);
		sc->write_started = 0;
	}
	if (sc->read_started)
		sc->read_started = 0;
	wait_status(sc, IG4_STATUS_TX_EMPTY);

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
 * Issue START with byte command, possible count, and a variable length
 * read or write buffer, then possible turn-around read.  The read also
 * has a possible count received.
 *
 * For SMBUS -
 *
 * Quick:		START+ADDR+RD/WR STOP
 *
 * Normal:		START+ADDR+WR CMD DATA..DATA STOP
 *
 *			START+ADDR+RD CMD
 *			RESTART+ADDR RDATA..RDATA STOP
 *			(can also be used for I2C transactions)
 *
 * Process Call:	START+ADDR+WR CMD DATAL DATAH
 *			RESTART+ADDR+RD RDATAL RDATAH STOP
 *
 * Block:		START+ADDR+RD CMD
 *			RESTART+ADDR+RD RCOUNT DATA... STOP
 *
 * 			START+ADDR+WR CMD
 *			RESTART+ADDR+WR WCOUNT DATA... STOP
 *
 * For I2C - basically, no *COUNT fields, possibly no *CMD field.  If the
 *	     sender needs to issue a 2-byte command it will incorporate it
 *	     into the write buffer and also set NOCMD.
 *
 * Generally speaking, the START+ADDR / RESTART+ADDR is handled automatically
 * by the controller at the beginning of a command sequence or on a data
 * direction turn-around, and we only need to tell it when to issue the STOP.
 */
static int
smb_transaction(ig4iic_softc_t *sc, char cmd, int op,
		char *wbuf, int wcount, char *rbuf, int rcount, int *actualp)
{
	int error;
	int unit;
	uint32_t last;

	/*
	 * Debugging - dump registers
	 */
	if (ig4_dump) {
		unit = device_get_unit(sc->dev);
		if (ig4_dump & (1 << unit)) {
			ig4_dump &= ~(1 << unit);
			ig4iic_dump(sc);
		}
	}

	/*
	 * Issue START or RESTART with next data byte, clear any previous
	 * abort condition that may have been holding the txfifo in reset.
	 */
	last = IG4_DATA_RESTART;
	reg_read(sc, IG4_REG_CLR_TX_ABORT);
	if (actualp)
		*actualp = 0;

	/*
	 * Issue command if not told otherwise (smbus).
	 */
	if ((op & SMB_TRANS_NOCMD) == 0) {
		error = wait_status(sc, IG4_STATUS_TX_NOTFULL);
		if (error)
			goto done;
		last |= (u_char)cmd;
		if (wcount == 0 && rcount == 0 && (op & SMB_TRANS_NOSTOP) == 0)
			last |= IG4_DATA_STOP;
		reg_write(sc, IG4_REG_DATA_CMD, last);
		last = 0;
	}

	/*
	 * Clean out any previously received data.
	 */
	if (sc->rpos != sc->rnext &&
	    (op & SMB_TRANS_NOREPORT) == 0) {
		device_printf(sc->dev,
			      "discarding %d bytes of spurious data\n",
			      sc->rnext - sc->rpos);
	}
	sc->rpos = 0;
	sc->rnext = 0;

	/*
	 * If writing and not told otherwise, issue the write count (smbus).
	 */
	if (wcount && (op & SMB_TRANS_NOCNT) == 0) {
		error = wait_status(sc, IG4_STATUS_TX_NOTFULL);
		if (error)
			goto done;
		last |= (u_char)cmd;
		reg_write(sc, IG4_REG_DATA_CMD, last);
		last = 0;
	}

	/*
	 * Bulk write (i2c)
	 */
	while (wcount) {
		error = wait_status(sc, IG4_STATUS_TX_NOTFULL);
		if (error)
			goto done;
		last |= (u_char)*wbuf;
		if (wcount == 1 && rcount == 0 && (op & SMB_TRANS_NOSTOP) == 0)
			last |= IG4_DATA_STOP;
		reg_write(sc, IG4_REG_DATA_CMD, last);
		--wcount;
		++wbuf;
		last = 0;
	}

	/*
	 * Issue reads to xmit FIFO (strange, I know) to tell the controller
	 * to clock in data.  At the moment just issue one read ahead to
	 * pipeline the incoming data.
	 *
	 * NOTE: In the case of NOCMD and wcount == 0 we still issue a
	 *	 RESTART here, even if the data direction has not changed
	 *	 from the previous CHAINing call.  This we force the RESTART.
	 *	 (A new START is issued automatically by the controller in
	 *	 the other nominal cases such as a data direction change or
	 *	 a previous STOP was issued).
	 *
	 * If this will be the last byte read we must also issue the STOP
	 * at the end of the read.
	 */
	if (rcount) {
		last = IG4_DATA_RESTART | IG4_DATA_COMMAND_RD;
		if (rcount == 1 &&
		    (op & (SMB_TRANS_NOSTOP | SMB_TRANS_NOCNT)) ==
		    SMB_TRANS_NOCNT) {
			last |= IG4_DATA_STOP;
		}
		reg_write(sc, IG4_REG_DATA_CMD, last);
		last = IG4_DATA_COMMAND_RD;
	}

	/*
	 * Bulk read (i2c) and count field handling (smbus)
	 */
	while (rcount) {
		/*
		 * Maintain a pipeline by queueing the allowance for the next
		 * read before waiting for the current read.
		 */
		if (rcount > 1) {
			if (op & SMB_TRANS_NOCNT)
				last = (rcount == 2) ? IG4_DATA_STOP : 0;
			else
				last = 0;
			reg_write(sc, IG4_REG_DATA_CMD, IG4_DATA_COMMAND_RD |
							last);
		}
		error = wait_status(sc, IG4_STATUS_RX_NOTEMPTY);
		if (error) {
			if ((op & SMB_TRANS_NOREPORT) == 0) {
				device_printf(sc->dev,
					      "rx timeout addr 0x%02x\n",
					      sc->last_slave);
			}
			goto done;
		}
		last = data_read(sc);

		if (op & SMB_TRANS_NOCNT) {
			*rbuf = (u_char)last;
			++rbuf;
			--rcount;
			if (actualp)
				++*actualp;
		} else {
			/*
			 * Handle count field (smbus), which is not part of
			 * the rcount'ed buffer.  The first read data in a
			 * bulk transfer is the count.
			 *
			 * XXX if rcount is loaded as 0 how do I generate a
			 *     STOP now without issuing another RD or WR?
			 */
			if (rcount > (u_char)last)
				rcount = (u_char)last;
			op |= SMB_TRANS_NOCNT;
		}
	}
	error = 0;
done:
	/* XXX wait for xmit buffer to become empty */
	last = reg_read(sc, IG4_REG_TX_ABRT_SOURCE);

	return (error);
}

/*
 *				SMBUS API FUNCTIONS
 *
 * Called from ig4iic_pci_attach/detach()
 */
int
ig4iic_attach(ig4iic_softc_t *sc)
{
	int error;
	uint32_t v;

	v = reg_read(sc, IG4_REG_COMP_TYPE);
	v = reg_read(sc, IG4_REG_COMP_PARAM1);
	v = reg_read(sc, IG4_REG_GENERAL);
	if ((v & IG4_GENERAL_SWMODE) == 0) {
		v |= IG4_GENERAL_SWMODE;
		reg_write(sc, IG4_REG_GENERAL, v);
		v = reg_read(sc, IG4_REG_GENERAL);
	}

	v = reg_read(sc, IG4_REG_SW_LTR_VALUE);
	v = reg_read(sc, IG4_REG_AUTO_LTR_VALUE);

	v = reg_read(sc, IG4_REG_COMP_VER);
	if (v != IG4_COMP_VER) {
		error = ENXIO;
		goto done;
	}
	v = reg_read(sc, IG4_REG_SS_SCL_HCNT);
	v = reg_read(sc, IG4_REG_SS_SCL_LCNT);
	v = reg_read(sc, IG4_REG_FS_SCL_HCNT);
	v = reg_read(sc, IG4_REG_FS_SCL_LCNT);
	v = reg_read(sc, IG4_REG_SDA_HOLD);

	v = reg_read(sc, IG4_REG_SS_SCL_HCNT);
	reg_write(sc, IG4_REG_FS_SCL_HCNT, v);
	v = reg_read(sc, IG4_REG_SS_SCL_LCNT);
	reg_write(sc, IG4_REG_FS_SCL_LCNT, v);

	/*
	 * Program based on a 25000 Hz clock.  This is a bit of a
	 * hack (obviously).  The defaults are 400 and 470 for standard
	 * and 60 and 130 for fast.  The defaults for standard fail
	 * utterly (presumably cause an abort) because the clock time
	 * is ~18.8ms by default.  This brings it down to ~4ms (for now).
	 */
	reg_write(sc, IG4_REG_SS_SCL_HCNT, 100);
	reg_write(sc, IG4_REG_SS_SCL_LCNT, 125);
	reg_write(sc, IG4_REG_FS_SCL_HCNT, 100);
	reg_write(sc, IG4_REG_FS_SCL_LCNT, 125);

	/*
	 * Use a threshold of 1 so we get interrupted on each character,
	 * allowing us to use mtx_sleep() in our poll code.  Not perfect
	 * but this is better than using DELAY() for receiving data.
	 *
	 * See ig4_var.h for details on interrupt handler synchronization.
	 */
	reg_write(sc, IG4_REG_RX_TL, 1);

	reg_write(sc, IG4_REG_CTL,
		  IG4_CTL_MASTER |
		  IG4_CTL_SLAVE_DISABLE |
		  IG4_CTL_RESTARTEN |
		  IG4_CTL_SPEED_STD);

	sc->smb = device_add_child(sc->dev, "smbus", -1);
	if (sc->smb == NULL) {
		device_printf(sc->dev, "smbus driver not found\n");
		error = ENXIO;
		goto done;
	}

#if 0
	/*
	 * Don't do this, it blows up the PCI config
	 */
	reg_write(sc, IG4_REG_RESETS, IG4_RESETS_ASSERT);
	reg_write(sc, IG4_REG_RESETS, IG4_RESETS_DEASSERT);
#endif

	/*
	 * Interrupt on STOP detect or receive character ready
	 */
	reg_write(sc, IG4_REG_INTR_MASK, IG4_INTR_STOP_DET |
					 IG4_INTR_RX_FULL);
	mtx_lock(&sc->io_lock);
	if (set_controller(sc, 0))
		device_printf(sc->dev, "controller error during attach-1\n");
	if (set_controller(sc, IG4_I2C_ENABLE))
		device_printf(sc->dev, "controller error during attach-2\n");
	mtx_unlock(&sc->io_lock);
	error = bus_setup_intr(sc->dev, sc->intr_res, INTR_TYPE_MISC | INTR_MPSAFE,
			       NULL, ig4iic_intr, sc, &sc->intr_handle);
	if (error) {
		device_printf(sc->dev,
			      "Unable to setup irq: error %d\n", error);
	}

	sc->enum_hook.ich_func = ig4iic_start;
	sc->enum_hook.ich_arg = sc->dev;

	/* We have to wait until interrupts are enabled. I2C read and write
	 * only works if the interrupts are available.
	 */
	if (config_intrhook_establish(&sc->enum_hook) != 0)
		error = ENOMEM;
	else
		error = 0;

done:
	return (error);
}

void
ig4iic_start(void *xdev)
{
	int error;
	ig4iic_softc_t *sc;
	device_t dev = (device_t)xdev;

	sc = device_get_softc(dev);

	config_intrhook_disestablish(&sc->enum_hook);

	/* Attach us to the smbus */
	error = bus_generic_attach(sc->dev);
	if (error) {
		device_printf(sc->dev,
			      "failed to attach child: error %d\n", error);
	}
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
	if (sc->smb)
		device_delete_child(sc->dev, sc->smb);
	if (sc->intr_handle)
		bus_teardown_intr(sc->dev, sc->intr_res, sc->intr_handle);

	sx_xlock(&sc->call_lock);
	mtx_lock(&sc->io_lock);

	sc->smb = NULL;
	sc->intr_handle = NULL;
	reg_write(sc, IG4_REG_INTR_MASK, 0);
	reg_read(sc, IG4_REG_CLR_INTR);
	set_controller(sc, 0);

	mtx_unlock(&sc->io_lock);
	sx_xunlock(&sc->call_lock);
	return (0);
}

int
ig4iic_smb_callback(device_t dev, int index, void *data)
{
	int error;

	switch (index) {
	case SMB_REQUEST_BUS:
		error = 0;
		break;
	case SMB_RELEASE_BUS:
		error = 0;
		break;
	default:
		error = SMB_EABORT;
		break;
	}

	return (error);
}

/*
 * Quick command.  i.e. START + cmd + R/W + STOP and no data.  It is
 * unclear to me how I could implement this with the intel i2c controller
 * because the controler sends STARTs and STOPs automatically with data.
 */
int
ig4iic_smb_quick(device_t dev, u_char slave, int how)
{

	return (SMB_ENOTSUPP);
}

/*
 * Incremental send byte without stop (?).  It is unclear why the slave
 * address is specified if this presumably is used in combination with
 * ig4iic_smb_quick().
 *
 * (Also, how would this work anyway?  Issue the last byte with writeb()?)
 */
int
ig4iic_smb_sendb(device_t dev, u_char slave, char byte)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	uint32_t cmd;
	int error;

	sx_xlock(&sc->call_lock);
	mtx_lock(&sc->io_lock);

	set_slave_addr(sc, slave, 0);
	cmd = byte;
	if (wait_status(sc, IG4_STATUS_TX_NOTFULL) == 0) {
		reg_write(sc, IG4_REG_DATA_CMD, cmd);
		error = 0;
	} else {
		error = SMB_ETIMEOUT;
	}

	mtx_unlock(&sc->io_lock);
	sx_xunlock(&sc->call_lock);
	return (error);
}

/*
 * Incremental receive byte without stop (?).  It is unclear why the slave
 * address is specified if this presumably is used in combination with
 * ig4iic_smb_quick().
 */
int
ig4iic_smb_recvb(device_t dev, u_char slave, char *byte)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int error;

	sx_xlock(&sc->call_lock);
	mtx_lock(&sc->io_lock);

	set_slave_addr(sc, slave, 0);
	reg_write(sc, IG4_REG_DATA_CMD, IG4_DATA_COMMAND_RD);
	if (wait_status(sc, IG4_STATUS_RX_NOTEMPTY) == 0) {
		*byte = data_read(sc);
		error = 0;
	} else {
		*byte = 0;
		error = SMB_ETIMEOUT;
	}

	mtx_unlock(&sc->io_lock);
	sx_xunlock(&sc->call_lock);
	return (error);
}

/*
 * Write command and single byte in transaction.
 */
int
ig4iic_smb_writeb(device_t dev, u_char slave, char cmd, char byte)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int error;

	sx_xlock(&sc->call_lock);
	mtx_lock(&sc->io_lock);

	set_slave_addr(sc, slave, 0);
	error = smb_transaction(sc, cmd, SMB_TRANS_NOCNT,
				&byte, 1, NULL, 0, NULL);

	mtx_unlock(&sc->io_lock);
	sx_xunlock(&sc->call_lock);
	return (error);
}

/*
 * Write command and single word in transaction.
 */
int
ig4iic_smb_writew(device_t dev, u_char slave, char cmd, short word)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	char buf[2];
	int error;

	sx_xlock(&sc->call_lock);
	mtx_lock(&sc->io_lock);

	set_slave_addr(sc, slave, 0);
	buf[0] = word & 0xFF;
	buf[1] = word >> 8;
	error = smb_transaction(sc, cmd, SMB_TRANS_NOCNT,
				buf, 2, NULL, 0, NULL);

	mtx_unlock(&sc->io_lock);
	sx_xunlock(&sc->call_lock);
	return (error);
}

/*
 * write command and read single byte in transaction.
 */
int
ig4iic_smb_readb(device_t dev, u_char slave, char cmd, char *byte)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int error;

	sx_xlock(&sc->call_lock);
	mtx_lock(&sc->io_lock);

	set_slave_addr(sc, slave, 0);
	error = smb_transaction(sc, cmd, SMB_TRANS_NOCNT,
				NULL, 0, byte, 1, NULL);

	mtx_unlock(&sc->io_lock);
	sx_xunlock(&sc->call_lock);
	return (error);
}

/*
 * write command and read word in transaction.
 */
int
ig4iic_smb_readw(device_t dev, u_char slave, char cmd, short *word)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	char buf[2];
	int error;

	sx_xlock(&sc->call_lock);
	mtx_lock(&sc->io_lock);

	set_slave_addr(sc, slave, 0);
	if ((error = smb_transaction(sc, cmd, SMB_TRANS_NOCNT,
				     NULL, 0, buf, 2, NULL)) == 0) {
		*word = (u_char)buf[0] | ((u_char)buf[1] << 8);
	}

	mtx_unlock(&sc->io_lock);
	sx_xunlock(&sc->call_lock);
	return (error);
}

/*
 * write command and word and read word in transaction
 */
int
ig4iic_smb_pcall(device_t dev, u_char slave, char cmd,
		 short sdata, short *rdata)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	char rbuf[2];
	char wbuf[2];
	int error;

	sx_xlock(&sc->call_lock);
	mtx_lock(&sc->io_lock);

	set_slave_addr(sc, slave, 0);
	wbuf[0] = sdata & 0xFF;
	wbuf[1] = sdata >> 8;
	if ((error = smb_transaction(sc, cmd, SMB_TRANS_NOCNT,
				     wbuf, 2, rbuf, 2, NULL)) == 0) {
		*rdata = (u_char)rbuf[0] | ((u_char)rbuf[1] << 8);
	}

	mtx_unlock(&sc->io_lock);
	sx_xunlock(&sc->call_lock);
	return (error);
}

int
ig4iic_smb_bwrite(device_t dev, u_char slave, char cmd,
		  u_char wcount, char *buf)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int error;

	sx_xlock(&sc->call_lock);
	mtx_lock(&sc->io_lock);

	set_slave_addr(sc, slave, 0);
	error = smb_transaction(sc, cmd, 0,
				buf, wcount, NULL, 0, NULL);

	mtx_unlock(&sc->io_lock);
	sx_xunlock(&sc->call_lock);
	return (error);
}

int
ig4iic_smb_bread(device_t dev, u_char slave, char cmd,
		 u_char *countp_char, char *buf)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int rcount = *countp_char;
	int error;

	sx_xlock(&sc->call_lock);
	mtx_lock(&sc->io_lock);

	set_slave_addr(sc, slave, 0);
	error = smb_transaction(sc, cmd, 0,
				NULL, 0, buf, rcount, &rcount);
	*countp_char = rcount;

	mtx_unlock(&sc->io_lock);
	sx_xunlock(&sc->call_lock);
	return (error);
}

int
ig4iic_smb_trans(device_t dev, int slave, char cmd, int op,
		 char *wbuf, int wcount, char *rbuf, int rcount,
		 int *actualp)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int error;

	sx_xlock(&sc->call_lock);
	mtx_lock(&sc->io_lock);

	set_slave_addr(sc, slave, op);
	error = smb_transaction(sc, cmd, op,
				wbuf, wcount, rbuf, rcount, actualp);

	mtx_unlock(&sc->io_lock);
	sx_xunlock(&sc->call_lock);
	return (error);
}

/*
 * Interrupt Operation, see ig4_var.h for locking semantics.
 */
static void
ig4iic_intr(void *cookie)
{
	ig4iic_softc_t *sc = cookie;
	uint32_t status;

	mtx_lock(&sc->io_lock);
/*	reg_write(sc, IG4_REG_INTR_MASK, IG4_INTR_STOP_DET);*/
	status = reg_read(sc, IG4_REG_I2C_STA);
	while (status & IG4_STATUS_RX_NOTEMPTY) {
		sc->rbuf[sc->rnext & IG4_RBUFMASK] =
		    (uint8_t)reg_read(sc, IG4_REG_DATA_CMD);
		++sc->rnext;
		status = reg_read(sc, IG4_REG_I2C_STA);
	}
	reg_read(sc, IG4_REG_CLR_INTR);
	wakeup(sc);
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
	REGDUMP(sc, IG4_REG_COMP_TYPE);
	REGDUMP(sc, IG4_REG_CLK_PARMS);
	REGDUMP(sc, IG4_REG_RESETS);
	REGDUMP(sc, IG4_REG_GENERAL);
	REGDUMP(sc, IG4_REG_SW_LTR_VALUE);
	REGDUMP(sc, IG4_REG_AUTO_LTR_VALUE);
}
#undef REGDUMP

DRIVER_MODULE(smbus, ig4iic, smbus_driver, smbus_devclass, NULL, NULL);
