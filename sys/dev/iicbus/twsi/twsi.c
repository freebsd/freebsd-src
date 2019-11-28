/*-
 * Copyright (C) 2008 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

/*
 * Driver for the TWSI (aka I2C, aka IIC) bus controller found on Marvell
 * and Allwinner SoCs. Supports master operation only.
 *
 * Calls to DELAY() are needed per Application Note AN-179 "TWSI Software
 * Guidelines for Discovery(TM), Horizon (TM) and Feroceon(TM) Devices".
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>

#include <machine/_inttypes.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <sys/lock.h>
#include <sys/mutex.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iicbus/twsi/twsi.h>

#include "iicbus_if.h"

#define	TWSI_CONTROL_ACK	(1 << 2)
#define	TWSI_CONTROL_IFLG	(1 << 3)
#define	TWSI_CONTROL_STOP	(1 << 4)
#define	TWSI_CONTROL_START	(1 << 5)
#define	TWSI_CONTROL_TWSIEN	(1 << 6)
#define	TWSI_CONTROL_INTEN	(1 << 7)

#define	TWSI_STATUS_START		0x08
#define	TWSI_STATUS_RPTD_START		0x10
#define	TWSI_STATUS_ADDR_W_ACK		0x18
#define	TWSI_STATUS_ADDR_W_NACK		0x20
#define	TWSI_STATUS_DATA_WR_ACK		0x28
#define	TWSI_STATUS_DATA_WR_NACK	0x30
#define	TWSI_STATUS_ADDR_R_ACK		0x40
#define	TWSI_STATUS_ADDR_R_NACK		0x48
#define	TWSI_STATUS_DATA_RD_ACK		0x50
#define	TWSI_STATUS_DATA_RD_NOACK	0x58

#define	TWSI_DEBUG
#undef TWSI_DEBUG

#ifdef TWSI_DEBUG
#define	debugf(dev, fmt, args...) device_printf(dev, "%s: " fmt, __func__, ##args)
#else
#define	debugf(dev, fmt, args...)
#endif

static struct resource_spec res_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	{ SYS_RES_IRQ, 0, RF_ACTIVE | RF_SHAREABLE},
	{ -1, 0 }
};

static __inline uint32_t
TWSI_READ(struct twsi_softc *sc, bus_size_t off)
{
	uint32_t val;

	val = bus_read_4(sc->res[0], off);
	debugf(sc->dev, "read %x from %lx\n", val, off);
	return (val);
}

static __inline void
TWSI_WRITE(struct twsi_softc *sc, bus_size_t off, uint32_t val)
{

	debugf(sc->dev, "Writing %x to %lx\n", val, off);
	bus_write_4(sc->res[0], off, val);
}

static __inline void
twsi_control_clear(struct twsi_softc *sc, uint32_t mask)
{
	uint32_t val;

	val = TWSI_READ(sc, sc->reg_control);
	debugf(sc->dev, "read val=%x\n", val);
	val &= ~(TWSI_CONTROL_STOP | TWSI_CONTROL_START);
	val &= ~mask;
	debugf(sc->dev, "write val=%x\n", val);
	TWSI_WRITE(sc, sc->reg_control, val);
}

static __inline void
twsi_control_set(struct twsi_softc *sc, uint32_t mask)
{
	uint32_t val;

	val = TWSI_READ(sc, sc->reg_control);
	debugf(sc->dev, "read val=%x\n", val);
	val &= ~(TWSI_CONTROL_STOP | TWSI_CONTROL_START);
	val |= mask;
	debugf(sc->dev, "write val=%x\n", val);
	TWSI_WRITE(sc, sc->reg_control, val);
}

static __inline void
twsi_clear_iflg(struct twsi_softc *sc)
{

	DELAY(1000);
	twsi_control_clear(sc, TWSI_CONTROL_IFLG);
	DELAY(1000);
}


/*
 * timeout given in us
 * returns
 *   0 on successful mask change
 *   non-zero on timeout
 */
static int
twsi_poll_ctrl(struct twsi_softc *sc, int timeout, uint32_t mask)
{

	timeout /= 10;
	debugf(sc->dev, "Waiting for ctrl reg to match mask %x\n", mask);
	while (!(TWSI_READ(sc, sc->reg_control) & mask)) {
		DELAY(10);
		if (--timeout < 0)
			return (timeout);
	}
	debugf(sc->dev, "done\n");
	return (0);
}


/*
 * 'timeout' is given in us. Note also that timeout handling is not exact --
 * twsi_locked_start() total wait can be more than 2 x timeout
 * (twsi_poll_ctrl() is called twice). 'mask' can be either TWSI_STATUS_START
 * or TWSI_STATUS_RPTD_START
 */
static int
twsi_locked_start(device_t dev, struct twsi_softc *sc, int32_t mask,
    u_char slave, int timeout)
{
	int read_access, iflg_set = 0;
	uint32_t status;

	mtx_assert(&sc->mutex, MA_OWNED);

	if (mask == TWSI_STATUS_RPTD_START)
		/* read IFLG to know if it should be cleared later; from NBSD */
		iflg_set = TWSI_READ(sc, sc->reg_control) & TWSI_CONTROL_IFLG;

	debugf(dev, "send start\n");
	twsi_control_set(sc, TWSI_CONTROL_START);

	if (mask == TWSI_STATUS_RPTD_START && iflg_set) {
		debugf(dev, "IFLG set, clearing (mask=%x)\n", mask);
		twsi_clear_iflg(sc);
	}

	/*
	 * Without this delay we timeout checking IFLG if the timeout is 0.
	 * NBSD driver always waits here too.
	 */
	DELAY(1000);

	if (twsi_poll_ctrl(sc, timeout, TWSI_CONTROL_IFLG)) {
		debugf(dev, "timeout sending %sSTART condition\n",
		    mask == TWSI_STATUS_START ? "" : "repeated ");
		return (IIC_ETIMEOUT);
	}

	status = TWSI_READ(sc, sc->reg_status);
	debugf(dev, "status=%x\n", status);

	if (status != mask) {
		debugf(dev, "wrong status (%02x) after sending %sSTART condition\n",
		    status, mask == TWSI_STATUS_START ? "" : "repeated ");
		return (IIC_ESTATUS);
	}

	TWSI_WRITE(sc, sc->reg_data, slave);
	twsi_clear_iflg(sc);
	DELAY(1000);

	if (twsi_poll_ctrl(sc, timeout, TWSI_CONTROL_IFLG)) {
		debugf(dev, "timeout sending slave address (timeout=%d)\n", timeout);
		return (IIC_ETIMEOUT);
	}

	read_access = (slave & 0x1) ? 1 : 0;
	status = TWSI_READ(sc, sc->reg_status);
	if (status != (read_access ?
	    TWSI_STATUS_ADDR_R_ACK : TWSI_STATUS_ADDR_W_ACK)) {
		debugf(dev, "no ACK (status: %02x) after sending slave address\n",
		    status);
		return (IIC_ENOACK);
	}

	return (IIC_NOERR);
}

#ifdef EXT_RESOURCES
#define	TWSI_BAUD_RATE_RAW(C,M,N)	((C)/((10*(M+1))<<(N)))
#define	ABSSUB(a,b)	(((a) > (b)) ? (a) - (b) : (b) - (a))

static int
twsi_calc_baud_rate(struct twsi_softc *sc, const u_int target,
  int *param)
{
	uint64_t clk;
	uint32_t cur, diff, diff0;
	int m, n, m0, n0;

	/* Calculate baud rate. */
	diff0 = 0xffffffff;

	if (clk_get_freq(sc->clk_core, &clk) < 0)
		return (-1);

	debugf(sc->dev, "Bus clock is at %ju\n", clk);

	for (n = 0; n < 8; n++) {
		for (m = 0; m < 16; m++) {
			cur = TWSI_BAUD_RATE_RAW(clk,m,n);
			diff = ABSSUB(target, cur);
			if (diff < diff0) {
				m0 = m;
				n0 = n;
				diff0 = diff;
			}
		}
	}
	*param = TWSI_BAUD_RATE_PARAM(m0, n0);

	return (0);
}
#endif /* EXT_RESOURCES */

/*
 * Only slave mode supported, disregard [old]addr
 */
static int
twsi_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct twsi_softc *sc;
	uint32_t param;
#ifdef EXT_RESOURCES
	u_int busfreq;
#endif

	sc = device_get_softc(dev);

#ifdef EXT_RESOURCES
	busfreq = IICBUS_GET_FREQUENCY(sc->iicbus, speed);

	if (twsi_calc_baud_rate(sc, busfreq, &param) == -1) {
#endif
		switch (speed) {
		case IIC_SLOW:
		case IIC_FAST:
			param = sc->baud_rate[speed].param;
			debugf(dev, "Using IIC_FAST mode with speed param=%x\n", param);
			break;
		case IIC_FASTEST:
		case IIC_UNKNOWN:
		default:
			param = sc->baud_rate[IIC_FAST].param;
			debugf(dev, "Using IIC_FASTEST/UNKNOWN mode with speed param=%x\n", param);
			break;
		}
#ifdef EXT_RESOURCES
	}
#endif

	debugf(dev, "Using clock param=%x\n", param);

	mtx_lock(&sc->mutex);
	TWSI_WRITE(sc, sc->reg_soft_reset, 0x0);
	TWSI_WRITE(sc, sc->reg_baud_rate, param);
	TWSI_WRITE(sc, sc->reg_control, TWSI_CONTROL_TWSIEN);
	DELAY(1000);
	mtx_unlock(&sc->mutex);

	return (0);
}

static int
twsi_stop(device_t dev)
{
	struct twsi_softc *sc;

	sc = device_get_softc(dev);

	debugf(dev, "%s\n", __func__);
	mtx_lock(&sc->mutex);
	twsi_control_clear(sc, TWSI_CONTROL_ACK);
	twsi_control_set(sc, TWSI_CONTROL_STOP);
	twsi_clear_iflg(sc);
	DELAY(1000);
	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

/*
 * timeout is given in us
 */
static int
twsi_repeated_start(device_t dev, u_char slave, int timeout)
{
	struct twsi_softc *sc;
	int rv;

	sc = device_get_softc(dev);

	debugf(dev, "%s: slave=%x\n", __func__, slave);
	mtx_lock(&sc->mutex);
	rv = twsi_locked_start(dev, sc, TWSI_STATUS_RPTD_START, slave,
	    timeout);
	mtx_unlock(&sc->mutex);

	if (rv) {
		twsi_stop(dev);
		return (rv);
	} else
		return (IIC_NOERR);
}

/*
 * timeout is given in us
 */
static int
twsi_start(device_t dev, u_char slave, int timeout)
{
	struct twsi_softc *sc;
	int rv;

	sc = device_get_softc(dev);

	debugf(dev, "%s: slave=%x\n", __func__, slave);
	mtx_lock(&sc->mutex);
	rv = twsi_locked_start(dev, sc, TWSI_STATUS_START, slave, timeout);
	mtx_unlock(&sc->mutex);

	if (rv) {
		twsi_stop(dev);
		return (rv);
	} else
		return (IIC_NOERR);
}

static int
twsi_read(device_t dev, char *buf, int len, int *read, int last, int delay)
{
	struct twsi_softc *sc;
	uint32_t status;
	int last_byte, rv;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mutex);
	*read = 0;
	while (*read < len) {
		/*
		 * Check if we are reading last byte of the last buffer,
		 * do not send ACK then, per I2C specs
		 */
		last_byte = ((*read == len - 1) && last) ? 1 : 0;
		if (last_byte)
			twsi_control_clear(sc, TWSI_CONTROL_ACK);
		else
			twsi_control_set(sc, TWSI_CONTROL_ACK);

		twsi_clear_iflg(sc);
		DELAY(1000);

		if (twsi_poll_ctrl(sc, delay, TWSI_CONTROL_IFLG)) {
			debugf(dev, "timeout reading data (delay=%d)\n", delay);
			rv = IIC_ETIMEOUT;
			goto out;
		}

		status = TWSI_READ(sc, sc->reg_status);
		if (status != (last_byte ?
		    TWSI_STATUS_DATA_RD_NOACK : TWSI_STATUS_DATA_RD_ACK)) {
			debugf(dev, "wrong status (%02x) while reading\n", status);
			rv = IIC_ESTATUS;
			goto out;
		}

		*buf++ = TWSI_READ(sc, sc->reg_data);
		(*read)++;
	}
	rv = IIC_NOERR;
out:
	mtx_unlock(&sc->mutex);
	return (rv);
}

static int
twsi_write(device_t dev, const char *buf, int len, int *sent, int timeout)
{
	struct twsi_softc *sc;
	uint32_t status;
	int rv;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mutex);
	*sent = 0;
	while (*sent < len) {
		TWSI_WRITE(sc, sc->reg_data, *buf++);

		twsi_clear_iflg(sc);
		DELAY(1000);
		if (twsi_poll_ctrl(sc, timeout, TWSI_CONTROL_IFLG)) {
			debugf(dev, "timeout writing data (timeout=%d)\n", timeout);
			rv = IIC_ETIMEOUT;
			goto out;
		}

		status = TWSI_READ(sc, sc->reg_status);
		if (status != TWSI_STATUS_DATA_WR_ACK) {
			debugf(dev, "wrong status (%02x) while writing\n", status);
			rv = IIC_ESTATUS;
			goto out;
		}
		(*sent)++;
	}
	rv = IIC_NOERR;
out:
	mtx_unlock(&sc->mutex);
	return (rv);
}

static int
twsi_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	struct twsi_softc *sc;
	int i;

	sc = device_get_softc(dev);

	if (sc->have_intr == false)
		return (iicbus_transfer_gen(dev, msgs, nmsgs));

	sc->error = 0;

	sc->control_val = TWSI_CONTROL_TWSIEN |
		TWSI_CONTROL_INTEN | TWSI_CONTROL_ACK;
	TWSI_WRITE(sc, sc->reg_control, sc->control_val);
	debugf(dev, "transmitting %d messages\n", nmsgs);
	debugf(sc->dev, "status=%x\n", TWSI_READ(sc, sc->reg_status));
	for (i = 0; i < nmsgs && sc->error == 0; i++) {
		sc->transfer = 1;
		sc->msg = &msgs[i];
		debugf(dev, "msg[%d] flags: %x\n", i, msgs[i].flags);
		debugf(dev, "msg[%d] len: %d\n", i, msgs[i].len);

		/* Send start and re-enable interrupts */
		sc->control_val = TWSI_CONTROL_TWSIEN |
			TWSI_CONTROL_INTEN | TWSI_CONTROL_ACK;
		if (sc->msg->len == 1)
			sc->control_val &= ~TWSI_CONTROL_ACK;
		TWSI_WRITE(sc, sc->reg_control, sc->control_val | TWSI_CONTROL_START);
		while (sc->error == 0 && sc->transfer != 0) {
			pause_sbt("twsi", SBT_1MS * 30, SBT_1MS, 0);
		}

		debugf(dev, "Done with msg[%d]\n", i);
		if (sc->error) {
			debugf(sc->dev, "Error, aborting (%d)\n", sc->error);
			TWSI_WRITE(sc, sc->reg_control, 0);
			goto out;
		}
	}

	/* Disable module and interrupts */
	debugf(sc->dev, "status=%x\n", TWSI_READ(sc, sc->reg_status));
	TWSI_WRITE(sc, sc->reg_control, 0);
	debugf(sc->dev, "status=%x\n", TWSI_READ(sc, sc->reg_status));

out:
	return (sc->error);
}

static void
twsi_intr(void *arg)
{
	struct twsi_softc *sc;
	uint32_t status;
	int transfer_done = 0;

	sc = arg;

	debugf(sc->dev, "Got interrupt\n");

	while (TWSI_READ(sc, sc->reg_control) & TWSI_CONTROL_IFLG) {
		status = TWSI_READ(sc, sc->reg_status);
		debugf(sc->dev, "status=%x\n", status);

		switch (status) {
		case TWSI_STATUS_START:
		case TWSI_STATUS_RPTD_START:
			/* Transmit the address */
			debugf(sc->dev, "Send the address\n");

			if (sc->msg->flags & IIC_M_RD)
				TWSI_WRITE(sc, sc->reg_data,
				    sc->msg->slave | LSB);
			else
				TWSI_WRITE(sc, sc->reg_data,
				    sc->msg->slave & ~LSB);

			TWSI_WRITE(sc, sc->reg_control, sc->control_val);
			break;

		case TWSI_STATUS_ADDR_W_ACK:
			debugf(sc->dev, "Ack received after transmitting the address\n");
			/* Directly send the first byte */
			sc->sent_bytes = 0;
			debugf(sc->dev, "Sending byte 0 = %x\n", sc->msg->buf[0]);
			TWSI_WRITE(sc, sc->reg_data, sc->msg->buf[0]);

			TWSI_WRITE(sc, sc->reg_control, sc->control_val);
			break;

		case TWSI_STATUS_ADDR_R_ACK:
			debugf(sc->dev, "Ack received after transmitting the address\n");
			sc->recv_bytes = 0;

			TWSI_WRITE(sc, sc->reg_control, sc->control_val);
			break;

		case TWSI_STATUS_ADDR_W_NACK:
		case TWSI_STATUS_ADDR_R_NACK:
			debugf(sc->dev, "No ack received after transmitting the address\n");
			sc->transfer = 0;
			sc->error = ETIMEDOUT;
			sc->control_val = 0;
			wakeup(sc);
			break;

		case TWSI_STATUS_DATA_WR_ACK:
			debugf(sc->dev, "Ack received after transmitting data\n");
			if (sc->sent_bytes++ == (sc->msg->len - 1)) {
				debugf(sc->dev, "Done sending all the bytes\n");
				/* Send stop, no interrupts on stop */
				if (!(sc->msg->flags & IIC_M_NOSTOP)) {
					debugf(sc->dev, "Done TX data, send stop\n");
					TWSI_WRITE(sc, sc->reg_control,
					  sc->control_val | TWSI_CONTROL_STOP);
				} else {
					sc->control_val &= ~TWSI_CONTROL_INTEN;
					TWSI_WRITE(sc, sc->reg_control,
					    sc->control_val);
				}
				transfer_done = 1;
			} else {
				debugf(sc->dev, "Sending byte %d = %x\n",
				    sc->sent_bytes,
				    sc->msg->buf[sc->sent_bytes]);
				TWSI_WRITE(sc, sc->reg_data,
				    sc->msg->buf[sc->sent_bytes]);
				TWSI_WRITE(sc, sc->reg_control,
				    sc->control_val);
			}

			break;
		case TWSI_STATUS_DATA_RD_ACK:
			debugf(sc->dev, "Ack received after receiving data\n");
			debugf(sc->dev, "msg_len=%d recv_bytes=%d\n", sc->msg->len, sc->recv_bytes);
			sc->msg->buf[sc->recv_bytes++] = TWSI_READ(sc, sc->reg_data);

			/* If we only have one byte left, disable ACK */
			if (sc->msg->len - sc->recv_bytes == 1)
				sc->control_val &= ~TWSI_CONTROL_ACK;
			TWSI_WRITE(sc, sc->reg_control, sc->control_val);
			break;

		case TWSI_STATUS_DATA_RD_NOACK:
			if (sc->msg->len - sc->recv_bytes == 1) {
				sc->msg->buf[sc->recv_bytes++] = TWSI_READ(sc, sc->reg_data);
				debugf(sc->dev, "Done RX data, send stop (2)\n");
				if (!(sc->msg->flags & IIC_M_NOSTOP))
					TWSI_WRITE(sc, sc->reg_control,
					  sc->control_val | TWSI_CONTROL_STOP);
			} else {
				debugf(sc->dev, "No ack when receiving data\n");
				sc->error = ENXIO;
				sc->control_val = 0;
			}
			sc->transfer = 0;
			transfer_done = 1;
			break;

		default:
			debugf(sc->dev, "status=%x hot handled\n", status);
			sc->transfer = 0;
			sc->error = ENXIO;
			sc->control_val = 0;
			wakeup(sc);
			break;
		}

		if (sc->need_ack)
			TWSI_WRITE(sc, sc->reg_control,
			    sc->control_val | TWSI_CONTROL_IFLG);
	}

	debugf(sc->dev, "Done with interrupts\n");
	if (transfer_done == 1) {
		sc->transfer = 0;
		wakeup(sc);
	}
}

static void
twsi_intr_start(void *pdev)
{
	struct twsi_softc *sc;

	sc = device_get_softc(pdev);

	if ((bus_setup_intr(pdev, sc->res[1], INTR_TYPE_MISC | INTR_MPSAFE,
	      NULL, twsi_intr, sc, &sc->intrhand)))
		device_printf(pdev, "unable to register interrupt handler\n");

	sc->have_intr = true;
}

int
twsi_attach(device_t dev)
{
	struct twsi_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->mutex, device_get_nameunit(dev), "twsi", MTX_DEF);

	if (bus_alloc_resources(dev, res_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		twsi_detach(dev);
		return (ENXIO);
	}

	/* Attach the iicbus. */
	if ((sc->iicbus = device_add_child(dev, "iicbus", -1)) == NULL) {
		device_printf(dev, "could not allocate iicbus instance\n");
		twsi_detach(dev);
		return (ENXIO);
	}
	bus_generic_attach(dev);

	config_intrhook_oneshot(twsi_intr_start, dev);

	return (0);
}

int
twsi_detach(device_t dev)
{
	struct twsi_softc *sc;
	int rv;

	sc = device_get_softc(dev);

	if ((rv = bus_generic_detach(dev)) != 0)
		return (rv);

	if (sc->iicbus != NULL)
		if ((rv = device_delete_child(dev, sc->iicbus)) != 0)
			return (rv);

	if (sc->intrhand != NULL)
		bus_teardown_intr(sc->dev, sc->res[1], sc->intrhand);

	bus_release_resources(dev, res_spec, sc->res);

	mtx_destroy(&sc->mutex);
	return (0);
}

static device_method_t twsi_methods[] = {
	/* device interface */
	DEVMETHOD(device_detach,	twsi_detach),

	/* Bus interface */
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource,	bus_generic_adjust_resource),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback, iicbus_null_callback),
	DEVMETHOD(iicbus_repeated_start, twsi_repeated_start),
	DEVMETHOD(iicbus_start,		twsi_start),
	DEVMETHOD(iicbus_stop,		twsi_stop),
	DEVMETHOD(iicbus_write,		twsi_write),
	DEVMETHOD(iicbus_read,		twsi_read),
	DEVMETHOD(iicbus_reset,		twsi_reset),
	DEVMETHOD(iicbus_transfer,	twsi_transfer),
	{ 0, 0 }
};

DEFINE_CLASS_0(twsi, twsi_driver, twsi_methods,
    sizeof(struct twsi_softc));
