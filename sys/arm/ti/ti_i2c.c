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

/**
 * Driver for the I2C module on the TI SoC.
 *
 * This driver is heavily based on the TWI driver for the AT91 (at91_twi.c).
 *
 * CAUTION: The I2Ci registers are limited to 16 bit and 8 bit data accesses,
 * 32 bit data access is not allowed and can corrupt register content.
 *
 * This driver currently doesn't use DMA for the transfer, although I hope to
 * incorporate that sometime in the future.  The idea being that for transaction
 * larger than a certain size the DMA engine is used, for anything less the
 * normal interrupt/fifo driven option is used.
 *
 *
 * WARNING: This driver uses mtx_sleep and interrupts to perform transactions,
 * which means you can't do a transaction during startup before the interrupts
 * have been enabled.  Hint - the freebsd function config_intrhook_establish().
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/ti/ti_cpuid.h>
#include <arm/ti/ti_prcm.h>
#include <arm/ti/ti_i2c.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbus_if.h"

/**
 *	I2C device driver context, a pointer to this is stored in the device
 *	driver structure.
 */
struct ti_i2c_softc
{
	device_t		sc_dev;
	uint32_t		device_id;
	struct resource*	sc_irq_res;
	struct resource*	sc_mem_res;
	device_t		sc_iicbus;

	void*			sc_irq_h;

	struct mtx		sc_mtx;

	volatile uint16_t	sc_stat_flags;	/* contains the status flags last IRQ */

	uint16_t		sc_rev;
};

struct ti_i2c_clock_config
{
	int speed;
	int bitrate;
	uint8_t psc;		/* Fast/Standard mode prescale divider */
	uint8_t scll;		/* Fast/Standard mode SCL low time */
	uint8_t sclh;		/* Fast/Standard mode SCL high time */
	uint8_t hsscll;		/* High Speed mode SCL low time */
	uint8_t hssclh;		/* High Speed mode SCL high time */
};

#if defined(SOC_OMAP4)
static struct ti_i2c_clock_config ti_omap4_i2c_clock_configs[] = {
	{ IIC_SLOW,      100000, 23,  13,  15, 0,  0},
	{ IIC_FAST,      400000,  9,   5,   7, 0,  0},
	{ IIC_FASTEST,	3310000,  1, 113, 115, 7, 10},
	{ -1, 0 }
};
#endif

#if defined(SOC_TI_AM335X)
static struct ti_i2c_clock_config ti_am335x_i2c_clock_configs[] = {
	{ IIC_SLOW,      100000,  3,  53,  55, 0,  0},
	{ IIC_FAST,      400000,  3,   8,  10, 0,  0},
	{ IIC_FASTEST,   400000,  3,   8,  10, 0,  0}, /* This might be higher */
	{ -1, 0 }
};
#endif


#define TI_I2C_REV1  0x003C      /* OMAP3 */
#define TI_I2C_REV2  0x000A      /* OMAP4 */

/**
 *	Locking macros used throughout the driver
 */
#define TI_I2C_LOCK(_sc)             mtx_lock(&(_sc)->sc_mtx)
#define	TI_I2C_UNLOCK(_sc)           mtx_unlock(&(_sc)->sc_mtx)
#define TI_I2C_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	         "ti_i2c", MTX_DEF)
#define TI_I2C_LOCK_DESTROY(_sc)      mtx_destroy(&_sc->sc_mtx);
#define TI_I2C_ASSERT_LOCKED(_sc)     mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define TI_I2C_ASSERT_UNLOCKED(_sc)   mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

#ifdef DEBUG
#define ti_i2c_dbg(_sc, fmt, args...) \
    device_printf((_sc)->sc_dev, fmt, ##args)
#else
#define ti_i2c_dbg(_sc, fmt, args...)
#endif

static devclass_t ti_i2c_devclass;

/* bus entry points */

static int ti_i2c_probe(device_t dev);
static int ti_i2c_attach(device_t dev);
static int ti_i2c_detach(device_t dev);
static void ti_i2c_intr(void *);

/* OFW routine */
static phandle_t ti_i2c_get_node(device_t bus, device_t dev);

/* helper routines */
static int ti_i2c_activate(device_t dev);
static void ti_i2c_deactivate(device_t dev);

/**
 *	ti_i2c_read_2 - reads a 16-bit value from one of the I2C registers
 *	@sc: I2C device context
 *	@off: the byte offset within the register bank to read from.
 *
 *
 *	LOCKING:
 *	No locking required
 *
 *	RETURNS:
 *	16-bit value read from the register.
 */
static inline uint16_t
ti_i2c_read_2(struct ti_i2c_softc *sc, bus_size_t off)
{
	return bus_read_2(sc->sc_mem_res, off);
}

/**
 *	ti_i2c_write_2 - writes a 16-bit value to one of the I2C registers
 *	@sc: I2C device context
 *	@off: the byte offset within the register bank to read from.
 *	@val: the value to write into the register
 *
 *	LOCKING:
 *	No locking required
 *
 *	RETURNS:
 *	16-bit value read from the register.
 */
static inline void
ti_i2c_write_2(struct ti_i2c_softc *sc, bus_size_t off, uint16_t val)
{
	bus_write_2(sc->sc_mem_res, off, val);
}

/**
 *	ti_i2c_read_reg - reads a 16-bit value from one of the I2C registers
 *	    take into account revision-dependent register offset
 *	@sc: I2C device context
 *	@off: the byte offset within the register bank to read from.
 *
 *
 *	LOCKING:
 *	No locking required
 *
 *	RETURNS:
 *	16-bit value read from the register.
 */
static inline uint16_t
ti_i2c_read_reg(struct ti_i2c_softc *sc, bus_size_t off)
{
	/* XXXOMAP3: FIXME add registers mapping here */
	return bus_read_2(sc->sc_mem_res, off);
}

/**
 *	ti_i2c_write_reg - writes a 16-bit value to one of the I2C registers
 *	    take into account revision-dependent register offset
 *	@sc: I2C device context
 *	@off: the byte offset within the register bank to read from.
 *	@val: the value to write into the register
 *
 *	LOCKING:
 *	No locking required
 *
 *	RETURNS:
 *	16-bit value read from the register.
 */
static inline void
ti_i2c_write_reg(struct ti_i2c_softc *sc, bus_size_t off, uint16_t val)
{
	/* XXXOMAP3: FIXME add registers mapping here */
	bus_write_2(sc->sc_mem_res, off, val);
}

/**
 *	ti_i2c_set_intr_enable - writes the interrupt enable register
 *	@sc: I2C device context
 *	@ie: bitmask of the interrupts to enable
 *
 *	This function is needed as writing the I2C_IE register on the OMAP4 devices
 *	doesn't seem to actually enable the interrupt, rather you have to write
 *	through the I2C_IRQENABLE_CLR and I2C_IRQENABLE_SET registers.
 *
 *	LOCKING:
 *	No locking required
 *
 *	RETURNS:
 *	Nothing.
 */
static inline void
ti_i2c_set_intr_enable(struct ti_i2c_softc *sc, uint16_t ie)
{
	/* XXXOMAP3: FIXME */
	ti_i2c_write_2(sc, I2C_REG_IRQENABLE_CLR, 0xffff);
	if (ie)
		ti_i2c_write_2(sc, I2C_REG_IRQENABLE_SET, ie);
}

/**
 *	ti_i2c_reset - attach function for the driver
 *	@dev: i2c device handle
 *
 *
 *
 *	LOCKING:
 *	Called from timer context
 *
 *	RETURNS:
 *	EH_HANDLED or EH_NOT_HANDLED
 */
static int
ti_i2c_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct ti_i2c_softc *sc = device_get_softc(dev);
	struct ti_i2c_clock_config *clkcfg;
	uint16_t con_reg;

	switch (ti_chip()) {
#ifdef SOC_OMAP4
	case CHIP_OMAP_4:
		clkcfg = ti_omap4_i2c_clock_configs;
		break;
#endif
#ifdef SOC_TI_AM335X
	case CHIP_AM335X:
		clkcfg = ti_am335x_i2c_clock_configs;
		break;
#endif
	default:
		panic("Unknown Ti SoC, unable to reset the i2c");
	}
	while (clkcfg->speed != -1) {
		if (clkcfg->speed == speed)
			break;
		/* take slow if speed is unknown */
		if ((speed == IIC_UNKNOWN) && (clkcfg->speed == IIC_SLOW))
			break;
		clkcfg++;
	}
	if (clkcfg->speed == -1)
		return (EINVAL);

	TI_I2C_LOCK(sc);

	/* First disable the controller while changing the clocks */
	con_reg = ti_i2c_read_reg(sc, I2C_REG_CON);
	ti_i2c_write_reg(sc, I2C_REG_CON, 0x0000);

	/* Program the prescaler */
	ti_i2c_write_reg(sc, I2C_REG_PSC, clkcfg->psc);

	/* Set the bitrate */
	ti_i2c_write_reg(sc, I2C_REG_SCLL, clkcfg->scll | (clkcfg->hsscll<<8));
	ti_i2c_write_reg(sc, I2C_REG_SCLH, clkcfg->sclh | (clkcfg->hssclh<<8));

	/* Check if we are dealing with high speed mode */
	if ((clkcfg->hsscll + clkcfg->hssclh) > 0)
		con_reg  = I2C_CON_OPMODE_HS;
	else
		con_reg  = I2C_CON_OPMODE_STD;

	/* Enable the I2C module again */
	ti_i2c_write_reg(sc, I2C_REG_CON, I2C_CON_I2C_EN | con_reg);

	TI_I2C_UNLOCK(sc);

	return (IIC_ENOADDR);
}

/**
 *	ti_i2c_intr - interrupt handler for the I2C module
 *	@dev: i2c device handle
 *
 *
 *
 *	LOCKING:
 *	Called from timer context
 *
 *	RETURNS:
 *	EH_HANDLED or EH_NOT_HANDLED
 */
static void
ti_i2c_intr(void *arg)
{
	struct ti_i2c_softc *sc = (struct ti_i2c_softc*) arg;
	uint16_t status;

	status = ti_i2c_read_reg(sc, I2C_REG_STAT);
	if (status == 0)
		return;

	TI_I2C_LOCK(sc);

	/* save the flags */
	sc->sc_stat_flags |= status;

	/* clear the status flags */
	ti_i2c_write_reg(sc, I2C_REG_STAT, status);

	/* wakeup the process the started the transaction */
	wakeup(sc);

	TI_I2C_UNLOCK(sc);

	return;
}

/**
 *	ti_i2c_wait - waits for the specific event to occur
 *	@sc: i2c driver context
 *	@flags: the event(s) to wait on, this is a bitmask of the I2C_STAT_??? flags
 *	@statp: if not null will contain the status flags upon return
 *	@timo: the number of ticks to wait
 *
 *
 *
 *	LOCKING:
 *	The driver context must be locked before calling this function. Internally
 *	the function sleeps, releasing the lock as it does so, however the lock is
 *	always retaken before this function returns.
 *
 *	RETURNS:
 *	0 if the event(s) were tripped within timeout period
 *	EBUSY if timedout waiting for the events
 *	ENXIO if a NACK event was received
 */
static int
ti_i2c_wait(struct ti_i2c_softc *sc, uint16_t flags, uint16_t *statp, int timo)
{
	int waittime = timo;
	int start_ticks = ticks;
	int rc;

	TI_I2C_ASSERT_LOCKED(sc);

	/* check if the condition has already occured, the interrupt routine will
	 * clear the status flags.
	 */
	if ((sc->sc_stat_flags & flags) == 0) {

		/* condition(s) haven't occured so sleep on the IRQ */
		while (waittime > 0) {

			rc = mtx_sleep(sc, &sc->sc_mtx, 0, "I2Cwait", waittime);
			if (rc == EWOULDBLOCK) {
				/* timed-out, simply break out of the loop */
				break;
			} else {
				/* IRQ has been tripped, but need to sanity check we have the
				 * right events in the status flag.
				 */
				if ((sc->sc_stat_flags & flags) != 0)
					break;

				/* event hasn't been tripped so wait some more */
				waittime -= (ticks - start_ticks);
				start_ticks = ticks;
			}
		}
	}

	/* copy the actual status bits */
	if (statp != NULL)
		*statp = sc->sc_stat_flags;

	/* return the status found */
	if ((sc->sc_stat_flags & flags) != 0)
		rc = 0;
	else
		rc = EBUSY;

	/* clear the flags set by the interrupt handler */
	sc->sc_stat_flags = 0;

	return (rc);
}

/**
 *	ti_i2c_wait_for_free_bus - waits for the bus to become free
 *	@sc: i2c driver context
 *	@timo: the time to wait for the bus to become free
 *
 *
 *
 *	LOCKING:
 *	The driver context must be locked before calling this function. Internally
 *	the function sleeps, releasing the lock as it does so, however the lock is
 *	always taken before this function returns.
 *
 *	RETURNS:
 *	0 if the event(s) were tripped within timeout period
 *	EBUSY if timedout waiting for the events
 *	ENXIO if a NACK event was received
 */
static int
ti_i2c_wait_for_free_bus(struct ti_i2c_softc *sc, int timo)
{
	/* check if the bus is free, BB bit = 0 */
	if ((ti_i2c_read_reg(sc, I2C_REG_STAT) & I2C_STAT_BB) == 0)
		return 0;

	/* enable bus free interrupts */
	ti_i2c_set_intr_enable(sc, I2C_IE_BF);

	/* wait for the bus free interrupt to be tripped */
	return ti_i2c_wait(sc, I2C_STAT_BF, NULL, timo);
}

/**
 *	ti_i2c_read_bytes - attempts to perform a read operation
 *	@sc: i2c driver context
 *	@buf: buffer to hold the received bytes
 *	@len: the number of bytes to read
 *
 *	This function assumes the slave address is already set
 *
 *	LOCKING:
 *	The context lock should be held before calling this function
 *
 *	RETURNS:
 *	0 on function succeeded
 *	EINVAL if invalid message is passed as an arg
 */
static int
ti_i2c_read_bytes(struct ti_i2c_softc *sc, uint8_t *buf, uint16_t len)
{
	int      timo = (hz / 4);
	int      err = 0;
	uint16_t con_reg;
	uint16_t events;
	uint16_t status;
	uint32_t amount = 0;
	uint32_t sofar = 0;
	uint32_t i;

	/* wait for the bus to become free */
	err = ti_i2c_wait_for_free_bus(sc, timo);
	if (err != 0) {
		device_printf(sc->sc_dev, "bus never freed\n");
		return (err);
	}

	/* set the events to wait for */
	events = I2C_IE_RDR |   /* Receive draining interrupt */
	         I2C_IE_RRDY |  /* Receive Data Ready interrupt */
	         I2C_IE_ARDY |  /* Register Access Ready interrupt */
	         I2C_IE_NACK |  /* No Acknowledgment interrupt */
	         I2C_IE_AL;

	/* enable interrupts for the events we want */
	ti_i2c_set_intr_enable(sc, events);

	/* write the number of bytes to read */
	ti_i2c_write_reg(sc, I2C_REG_CNT, len);

	/* clear the write bit and initiate the read transaction. Setting the STT
	 * (start) bit initiates the transfer.
	 */
	con_reg = ti_i2c_read_reg(sc, I2C_REG_CON);
	con_reg &= ~I2C_CON_TRX;
	con_reg |=  I2C_CON_MST | I2C_CON_STT | I2C_CON_STP;
	ti_i2c_write_reg(sc, I2C_REG_CON, con_reg);

	/* reading loop */
	while (1) {

		/* wait for an event */
		err = ti_i2c_wait(sc, events, &status, timo);
		if (err != 0) {
			break;
		}

		/* check for the error conditions */
		if (status & I2C_STAT_NACK) {
			/* no ACK from slave */
			ti_i2c_dbg(sc, "NACK\n");
			err = ENXIO;
			break;
		}
		if (status & I2C_STAT_AL) {
			/* arbitration lost */
			ti_i2c_dbg(sc, "Arbitration lost\n");
			err = ENXIO;
			break;
		}

		/* check if we have finished */
		if (status & I2C_STAT_ARDY) {
			/* register access ready - transaction complete basically */
			ti_i2c_dbg(sc, "ARDY transaction complete\n");
			err = 0;
			break;
		}

		/* read some data */
		if (status & I2C_STAT_RDR) {
			/* Receive draining interrupt - last data received */
			ti_i2c_dbg(sc, "Receive draining interrupt\n");

			/* get the number of bytes in the FIFO */
			amount = ti_i2c_read_reg(sc, I2C_REG_BUFSTAT);
			amount >>= 8;
			amount &= 0x3f;
		}
		else if (status & I2C_STAT_RRDY) {
			/* Receive data ready interrupt - enough data received */
			ti_i2c_dbg(sc, "Receive data ready interrupt\n");

			/* get the number of bytes in the FIFO */
			amount = ti_i2c_read_reg(sc, I2C_REG_BUF);
			amount >>= 8;
			amount &= 0x3f;
			amount += 1;
		}

		/* sanity check we haven't overwritten the array */
		if ((sofar + amount) > len) {
			ti_i2c_dbg(sc, "to many bytes to read\n");
			amount = (len - sofar);
		}

		/* read the bytes from the fifo */
		for (i = 0; i < amount; i++) {
			buf[sofar++] = (uint8_t)(ti_i2c_read_reg(sc, I2C_REG_DATA) & 0xff);
		}

		/* attempt to clear the receive ready bits */
		ti_i2c_write_reg(sc, I2C_REG_STAT, I2C_STAT_RDR | I2C_STAT_RRDY);
	}

	/* reset the registers regardless if there was an error or not */
	ti_i2c_set_intr_enable(sc, 0x0000);
	ti_i2c_write_reg(sc, I2C_REG_CON, I2C_CON_I2C_EN | I2C_CON_MST | I2C_CON_STP);

	return (err);
}

/**
 *	ti_i2c_write_bytes - attempts to perform a read operation
 *	@sc: i2c driver context
 *	@buf: buffer containing the bytes to write
 *	@len: the number of bytes to write
 *
 *	This function assumes the slave address is already set
 *
 *	LOCKING:
 *	The context lock should be held before calling this function
 *
 *	RETURNS:
 *	0 on function succeeded
 *	EINVAL if invalid message is passed as an arg
 */
static int
ti_i2c_write_bytes(struct ti_i2c_softc *sc, const uint8_t *buf, uint16_t len)
{
	int      timo = (hz / 4);
	int      err = 0;
	uint16_t con_reg;
	uint16_t events;
	uint16_t status;
	uint32_t amount = 0;
	uint32_t sofar = 0;
	uint32_t i;

	/* wait for the bus to become free */
	err = ti_i2c_wait_for_free_bus(sc, timo);
	if (err != 0)
		return (err);

	/* set the events to wait for */
	events = I2C_IE_XDR |   /* Transmit draining interrupt */
	         I2C_IE_XRDY |  /* Transmit Data Ready interrupt */
	         I2C_IE_ARDY |  /* Register Access Ready interrupt */
	         I2C_IE_NACK |  /* No Acknowledgment interrupt */
	         I2C_IE_AL;

	/* enable interrupts for the events we want*/
	ti_i2c_set_intr_enable(sc, events);

	/* write the number of bytes to write */
	ti_i2c_write_reg(sc, I2C_REG_CNT, len);

	/* set the write bit and initiate the write transaction. Setting the STT
	 * (start) bit initiates the transfer.
	 */
	con_reg = ti_i2c_read_reg(sc, I2C_REG_CON);
	con_reg |= I2C_CON_TRX | I2C_CON_MST | I2C_CON_STT | I2C_CON_STP;
	ti_i2c_write_reg(sc, I2C_REG_CON, con_reg);

	/* writing loop */
	while (1) {

		/* wait for an event */
		err = ti_i2c_wait(sc, events, &status, timo);
		if (err != 0) {
			break;
		}

		/* check for the error conditions */
		if (status & I2C_STAT_NACK) {
			/* no ACK from slave */
			ti_i2c_dbg(sc, "NACK\n");
			err = ENXIO;
			break;
		}
		if (status & I2C_STAT_AL) {
			/* arbitration lost */
			ti_i2c_dbg(sc, "Arbitration lost\n");
			err = ENXIO;
			break;
		}

		/* check if we have finished */
		if (status & I2C_STAT_ARDY) {
			/* register access ready - transaction complete basically */
			ti_i2c_dbg(sc, "ARDY transaction complete\n");
			err = 0;
			break;
		}

		/* read some data */
		if (status & I2C_STAT_XDR) {
			/* Receive draining interrupt - last data received */
			ti_i2c_dbg(sc, "Transmit draining interrupt\n");

			/* get the number of bytes in the FIFO */
			amount = ti_i2c_read_reg(sc, I2C_REG_BUFSTAT);
			amount &= 0x3f;
		}
		else if (status & I2C_STAT_XRDY) {
			/* Receive data ready interrupt - enough data received */
			ti_i2c_dbg(sc, "Transmit data ready interrupt\n");

			/* get the number of bytes in the FIFO */
			amount = ti_i2c_read_reg(sc, I2C_REG_BUF);
			amount &= 0x3f;
			amount += 1;
		}

		/* sanity check we haven't overwritten the array */
		if ((sofar + amount) > len) {
			ti_i2c_dbg(sc, "to many bytes to write\n");
			amount = (len - sofar);
		}

		/* write the bytes from the fifo */
		for (i = 0; i < amount; i++) {
			ti_i2c_write_reg(sc, I2C_REG_DATA, buf[sofar++]);
		}

		/* attempt to clear the transmit ready bits */
		ti_i2c_write_reg(sc, I2C_REG_STAT, I2C_STAT_XDR | I2C_STAT_XRDY);
	}

	/* reset the registers regardless if there was an error or not */
	ti_i2c_set_intr_enable(sc, 0x0000);
	ti_i2c_write_reg(sc, I2C_REG_CON, I2C_CON_I2C_EN | I2C_CON_MST | I2C_CON_STP);

	return (err);
}

/**
 *	ti_i2c_transfer - called to perform the transfer
 *	@dev: i2c device handle
 *	@msgs: the messages to send/receive
 *	@nmsgs: the number of messages in the msgs array
 *
 *
 *	LOCKING:
 *	Internally locked
 *
 *	RETURNS:
 *	0 on function succeeded
 *	EINVAL if invalid message is passed as an arg
 */
static int
ti_i2c_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	struct ti_i2c_softc *sc = device_get_softc(dev);
	int err = 0;
	uint32_t i;
	uint16_t len;
	uint8_t *buf;

	TI_I2C_LOCK(sc);

	for (i = 0; i < nmsgs; i++) {

		len = msgs[i].len;
		buf = msgs[i].buf;

		/* zero byte transfers aren't allowed */
		if (len == 0 || buf == NULL) {
			err = EINVAL;
			goto out;
		}

		/* set the slave address */
		ti_i2c_write_reg(sc, I2C_REG_SA, msgs[i].slave >> 1);

		/* perform the read or write */
		if (msgs[i].flags & IIC_M_RD) {
			err = ti_i2c_read_bytes(sc, buf, len);
		} else {
			err = ti_i2c_write_bytes(sc, buf, len);
		}

	}

out:
	TI_I2C_UNLOCK(sc);

	return (err);
}

/**
 *	ti_i2c_callback - not sure about this one
 *	@dev: i2c device handle
 *
 *
 *
 *	LOCKING:
 *	Called from timer context
 *
 *	RETURNS:
 *	EH_HANDLED or EH_NOT_HANDLED
 */
static int
ti_i2c_callback(device_t dev, int index, caddr_t data)
{
	int error = 0;

	switch (index) {
		case IIC_REQUEST_BUS:
			break;

		case IIC_RELEASE_BUS:
			break;

		default:
			error = EINVAL;
	}

	return (error);
}

/**
 *	ti_i2c_activate - initialises and activates an I2C bus
 *	@dev: i2c device handle
 *	@num: the number of the I2C controller to activate; 1, 2 or 3
 *
 *
 *	LOCKING:
 *	Assumed called in an atomic context.
 *
 *	RETURNS:
 *	nothing
 */
static int
ti_i2c_activate(device_t dev)
{
	struct ti_i2c_softc *sc = (struct ti_i2c_softc*) device_get_softc(dev);
	unsigned int timeout = 0;
	uint16_t con_reg;
	int err;
	clk_ident_t clk;

	/*
	 * The following sequence is taken from the OMAP3530 technical reference
	 *
	 * 1. Enable the functional and interface clocks (see Section 18.3.1.1.1).
	 */
	clk = I2C0_CLK + sc->device_id;
	err = ti_prcm_clk_enable(clk);
	if (err)
		return (err);

	/* There seems to be a bug in the I2C reset mechanism, for some reason you
	 * need to disable the I2C module before issuing the reset and then enable
	 * it again after to detect the reset done.
	 *
	 * I found this out by looking at the Linux driver implementation, thanks
	 * linux guys!
	 */

	/* Disable the I2C controller */
	ti_i2c_write_reg(sc, I2C_REG_CON, 0x0000);

	/* Issue a softreset to the controller */
	/* XXXOMAP3: FIXME */
	bus_write_2(sc->sc_mem_res, I2C_REG_SYSC, 0x0002);

	/* Re-enable the module and then check for the reset done */
	ti_i2c_write_reg(sc, I2C_REG_CON, I2C_CON_I2C_EN);

	while ((ti_i2c_read_reg(sc, I2C_REG_SYSS) & 0x01) == 0x00) {
		if (timeout++ > 100) {
			return (EBUSY);
		}
		DELAY(100);
	}

	/* Disable the I2C controller once again, now that the reset has finished */
	ti_i2c_write_reg(sc, I2C_REG_CON, 0x0000);

	/* 2. Program the prescaler to obtain an approximately 12-MHz internal
	 *    sampling clock (I2Ci_INTERNAL_CLK) by programming the corresponding
	 *    value in the I2Ci.I2C_PSC[3:0] PSC field.
	 *    This value depends on the frequency of the functional clock (I2Ci_FCLK).
	 *    Because this frequency is 96MHz, the I2Ci.I2C_PSC[7:0] PSC field value
	 *    is 0x7.
	 */

	/* Program the prescaler to obtain an approximately 12-MHz internal
	 * sampling clock.
	 */
	ti_i2c_write_reg(sc, I2C_REG_PSC, 0x0017);

	/* 3. Program the I2Ci.I2C_SCLL[7:0] SCLL and I2Ci.I2C_SCLH[7:0] SCLH fields
	 *    to obtain a bit rate of 100K bps or 400K bps. These values depend on
	 *    the internal sampling clock frequency (see Table 18-12).
	 */

	/* Set the bitrate to 100kbps */
	ti_i2c_write_reg(sc, I2C_REG_SCLL, 0x000d);
	ti_i2c_write_reg(sc, I2C_REG_SCLH, 0x000f);

	/* 4. (Optional) Program the I2Ci.I2C_SCLL[15:8] HSSCLL and
	 *    I2Ci.I2C_SCLH[15:8] HSSCLH fields to obtain a bit rate of 400K bps or
	 *    3.4M bps (for the second phase of HS mode). These values depend on the
	 *    internal sampling clock frequency (see Table 18-12).
	 *
	 * 5. (Optional) If a bit rate of 3.4M bps is used and the bus line
	 *    capacitance exceeds 45 pF, program the CONTROL.CONTROL_DEVCONF1[12]
	 *    I2C1HSMASTER bit for I2C1, the CONTROL.CONTROL_DEVCONF1[13]
	 *    I2C2HSMASTER bit for I2C2, or the CONTROL.CONTROL_DEVCONF1[14]
	 *    I2C3HSMASTER bit for I2C3.
	 */

	/* 6. Configure the Own Address of the I2C controller by storing it in the
	 *    I2Ci.I2C_OA0 register. Up to four Own Addresses can be programmed in
	 *    the I2Ci.I2C_OAi registers (with I = 0, 1, 2, 3) for each I2C
	 *    controller.
	 *
	 * Note: For a 10-bit address, set the corresponding expand Own Address bit
	 * in the I2Ci.I2C_CON register.
	 */

	/* Driver currently always in single master mode so ignore this step */

	/* 7. Set the TX threshold (in transmitter mode) and the RX threshold (in
	 *    receiver mode) by setting the I2Ci.I2C_BUF[5:0]XTRSH field to (TX
	 *    threshold - 1) and the I2Ci.I2C_BUF[13:8]RTRSH field to (RX threshold
	 *    - 1), where the TX and RX thresholds are greater than or equal to 1.
	 */

	/* Set the FIFO buffer threshold, note I2C1 & I2C2 have 8 byte FIFO, whereas
	 * I2C3 has 64 bytes.  Threshold set to 5 for now.
	 */
	ti_i2c_write_reg(sc, I2C_REG_BUF, 0x0404);

	/*
	 * 8. Take the I2C controller out of reset by setting the I2Ci.I2C_CON[15]
	 *    I2C_EN bit to 1.
	 */
	ti_i2c_write_reg(sc, I2C_REG_CON, I2C_CON_I2C_EN | I2C_CON_OPMODE_STD);

	/*
	 * To initialize the I2C controller, perform the following steps:
	 *
	 * 1. Configure the I2Ci.I2C_CON register:
	 *    · For master or slave mode, set the I2Ci.I2C_CON[10] MST bit (0: slave,
	 *      1: master).
	 *    · For transmitter or receiver mode, set the I2Ci.I2C_CON[9] TRX bit
	 *      (0: receiver, 1: transmitter).
	 */
	con_reg = ti_i2c_read_reg(sc, I2C_REG_CON);
	con_reg |= I2C_CON_MST;
	ti_i2c_write_reg(sc, I2C_REG_CON, con_reg);

	/* 2. If using an interrupt to transmit/receive data, set to 1 the
	 *    corresponding bit in the I2Ci.I2C_IE register (the I2Ci.I2C_IE[4]
	 *    XRDY_IE bit for the transmit interrupt, the I2Ci.I2C_IE[3] RRDY bit
	 *    for the receive interrupt).
	 */
	ti_i2c_set_intr_enable(sc, I2C_IE_XRDY | I2C_IE_RRDY);

	/* 3. If using DMA to receive/transmit data, set to 1 the corresponding bit
	 *    in the I2Ci.I2C_BUF register (the I2Ci.I2C_BUF[15] RDMA_EN bit for the
	 *    receive DMA channel, the I2Ci.I2C_BUF[7] XDMA_EN bit for the transmit
	 *    DMA channel).
	 */

	/* not using DMA for now, so ignore this */

	return (0);
}

/**
 *	ti_i2c_deactivate - deactivates the controller and releases resources
 *	@dev: i2c device handle
 *
 *
 *
 *	LOCKING:
 *	Assumed called in an atomic context.
 *
 *	RETURNS:
 *	nothing
 */
static void
ti_i2c_deactivate(device_t dev)
{
	struct ti_i2c_softc *sc = device_get_softc(dev);
	clk_ident_t clk;

	/* Disable the controller - cancel all transactions */
	ti_i2c_write_reg(sc, I2C_REG_CON, 0x0000);

	/* Release the interrupt handler */
	if (sc->sc_irq_h) {
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_irq_h);
		sc->sc_irq_h = 0;
	}

	bus_generic_detach(sc->sc_dev);

	/* Unmap the I2C controller registers */
	if (sc->sc_mem_res != 0) {
		bus_release_resource(dev, SYS_RES_MEMORY, rman_get_rid(sc->sc_irq_res),
							 sc->sc_mem_res);
		sc->sc_mem_res = NULL;
	}

	/* Release the IRQ resource */
	if (sc->sc_irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, rman_get_rid(sc->sc_irq_res),
							 sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}

	/* Finally disable the functional and interface clocks */
	clk = I2C0_CLK + sc->device_id;
	ti_prcm_clk_disable(clk);

	return;
}

/**
 *	ti_i2c_probe - probe function for the driver
 *	@dev: i2c device handle
 *
 *
 *
 *	LOCKING:
 *
 *
 *	RETURNS:
 *	Always returns 0
 */
static int
ti_i2c_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "ti,i2c"))
		return (ENXIO);

	device_set_desc(dev, "TI I2C Controller");
	return (0);
}

/**
 *	ti_i2c_attach - attach function for the driver
 *	@dev: i2c device handle
 *
 *	Initialised driver data structures and activates the I2C controller.
 *
 *	LOCKING:
 *
 *
 *	RETURNS:
 *
 */
static int
ti_i2c_attach(device_t dev)
{
	struct ti_i2c_softc *sc = device_get_softc(dev);
	phandle_t node;
	pcell_t did;
	int err;
	int rid;

	sc->sc_dev = dev;

	/* Get the i2c device id from FDT */
	node = ofw_bus_get_node(dev);
	if ((OF_getprop(node, "i2c-device-id", &did, sizeof(did))) <= 0) {
		device_printf(dev, "missing i2c-device-id attribute in FDT\n");
		return (ENXIO);
	}
	sc->device_id = fdt32_to_cpu(did);

	TI_I2C_LOCK_INIT(sc);

	/* Get the memory resource for the register mapping */
	rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	                                        RF_ACTIVE);
	if (sc->sc_mem_res == NULL)
		panic("%s: Cannot map registers", device_get_name(dev));

	/* Allocate an IRQ resource for the MMC controller */
	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	                                        RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_irq_res == NULL) {
		err = ENOMEM;
		goto out;
	}

	/* First we _must_ activate the H/W */
	err = ti_i2c_activate(dev);
	if (err) {
		device_printf(dev, "ti_i2c_activate failed\n");
		goto out;
	}

	/* XXXOMAP3: FIXME get proper revision here */
	/* Read the version number of the I2C module */
	sc->sc_rev = ti_i2c_read_2(sc, I2C_REG_REVNB_HI) & 0xff;

	device_printf(dev, "I2C revision %d.%d\n", sc->sc_rev >> 4,
	    sc->sc_rev & 0xf);

	/* activate the interrupt */
	err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
				NULL, ti_i2c_intr, sc, &sc->sc_irq_h);
	if (err)
		goto out;

	/* Attach to the iicbus */
	if ((sc->sc_iicbus = device_add_child(dev, "iicbus", -1)) == NULL)
		device_printf(dev, "could not allocate iicbus instance\n");

	/* Probe and attach the iicbus */
	bus_generic_attach(dev);

out:
	if (err) {
		ti_i2c_deactivate(dev);
		TI_I2C_LOCK_DESTROY(sc);
	}

	return (err);
}

/**
 *	ti_i2c_detach - detach function for the driver
 *	@dev: i2c device handle
 *
 *
 *
 *	LOCKING:
 *
 *
 *	RETURNS:
 *	Always returns 0
 */
static int
ti_i2c_detach(device_t dev)
{
	struct ti_i2c_softc *sc = device_get_softc(dev);
	int rv;

	ti_i2c_deactivate(dev);

	if (sc->sc_iicbus && (rv = device_delete_child(dev, sc->sc_iicbus)) != 0)
		return (rv);

	TI_I2C_LOCK_DESTROY(sc);

	return (0);
}


static phandle_t
ti_i2c_get_node(device_t bus, device_t dev)
{
	/* 
	 * Share controller node with iibus device
	 */
	return ofw_bus_get_node(bus);
}

static device_method_t ti_i2c_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ti_i2c_probe),
	DEVMETHOD(device_attach,	ti_i2c_attach),
	DEVMETHOD(device_detach,	ti_i2c_detach),

	/* OFW methods */
	DEVMETHOD(ofw_bus_get_node,	ti_i2c_get_node),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback,	ti_i2c_callback),
	DEVMETHOD(iicbus_reset,		ti_i2c_reset),
	DEVMETHOD(iicbus_transfer,	ti_i2c_transfer),
	{ 0, 0 }
};

static driver_t ti_i2c_driver = {
	"iichb",
	ti_i2c_methods,
	sizeof(struct ti_i2c_softc),
};

DRIVER_MODULE(ti_iic, simplebus, ti_i2c_driver, ti_i2c_devclass, 0, 0);
DRIVER_MODULE(iicbus, ti_iic, iicbus_driver, iicbus_devclass, 0, 0);

MODULE_DEPEND(ti_iic, ti_prcm, 1, 1, 1);
MODULE_DEPEND(ti_iic, iicbus, 1, 1, 1);
