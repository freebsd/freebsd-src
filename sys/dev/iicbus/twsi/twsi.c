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
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/_inttypes.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <dev/iicbus/twsi/twsi.h>

#include "iicbus_if.h"

#define	TWSI_CONTROL_ACK	(1 << 2)
#define	TWSI_CONTROL_IFLG	(1 << 3)
#define	TWSI_CONTROL_STOP	(1 << 4)
#define	TWSI_CONTROL_START	(1 << 5)
#define	TWSI_CONTROL_TWSIEN	(1 << 6)
#define	TWSI_CONTROL_INTEN	(1 << 7)

#define	TWSI_STATUS_BUS_ERROR		0x00
#define	TWSI_STATUS_START		0x08
#define	TWSI_STATUS_RPTD_START		0x10
#define	TWSI_STATUS_ADDR_W_ACK		0x18
#define	TWSI_STATUS_ADDR_W_NACK		0x20
#define	TWSI_STATUS_DATA_WR_ACK		0x28
#define	TWSI_STATUS_DATA_WR_NACK	0x30
#define	TWSI_STATUS_ARBITRATION_LOST	0x38
#define	TWSI_STATUS_ADDR_R_ACK		0x40
#define	TWSI_STATUS_ADDR_R_NACK		0x48
#define	TWSI_STATUS_DATA_RD_ACK		0x50
#define	TWSI_STATUS_DATA_RD_NOACK	0x58
#define	TWSI_STATUS_IDLE		0xf8

#define	TWSI_DEBUG
#undef TWSI_DEBUG

#define	debugf(sc, fmt, args...)	if ((sc)->debug)	\
    device_printf((sc)->dev, "%s: " fmt, __func__, ##args)

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
	if (sc->debug > 1)
		debugf(sc, "read %x from %lx\n", val, off);
	return (val);
}

static __inline void
TWSI_WRITE(struct twsi_softc *sc, bus_size_t off, uint32_t val)
{

	if (sc->debug > 1)
		debugf(sc, "Writing %x to %lx\n", val, off);
	bus_write_4(sc->res[0], off, val);
}

static __inline void
twsi_control_clear(struct twsi_softc *sc, uint32_t mask)
{
	uint32_t val;

	val = TWSI_READ(sc, sc->reg_control);
	debugf(sc, "read val=%x\n", val);
	val &= ~(TWSI_CONTROL_STOP | TWSI_CONTROL_START);
	val &= ~mask;
	debugf(sc, "write val=%x\n", val);
	TWSI_WRITE(sc, sc->reg_control, val);
}

static __inline void
twsi_control_set(struct twsi_softc *sc, uint32_t mask)
{
	uint32_t val;

	val = TWSI_READ(sc, sc->reg_control);
	debugf(sc, "read val=%x\n", val);
	val &= ~(TWSI_CONTROL_STOP | TWSI_CONTROL_START);
	val |= mask;
	debugf(sc, "write val=%x\n", val);
	TWSI_WRITE(sc, sc->reg_control, val);
}

static __inline void
twsi_clear_iflg(struct twsi_softc *sc)
{

	DELAY(1000);
	/* There are two ways of clearing IFLAG. */
	if (sc->iflag_w1c)
		twsi_control_set(sc, TWSI_CONTROL_IFLG);
	else
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
	debugf(sc, "Waiting for ctrl reg to match mask %x\n", mask);
	while (!(TWSI_READ(sc, sc->reg_control) & mask)) {
		DELAY(10);
		if (--timeout < 0)
			return (timeout);
	}
	debugf(sc, "done\n");
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

	debugf(sc, "send start\n");
	twsi_control_set(sc, TWSI_CONTROL_START);

	if (mask == TWSI_STATUS_RPTD_START && iflg_set) {
		debugf(sc, "IFLG set, clearing (mask=%x)\n", mask);
		twsi_clear_iflg(sc);
	}

	/*
	 * Without this delay we timeout checking IFLG if the timeout is 0.
	 * NBSD driver always waits here too.
	 */
	DELAY(1000);

	if (twsi_poll_ctrl(sc, timeout, TWSI_CONTROL_IFLG)) {
		debugf(sc, "timeout sending %sSTART condition\n",
		    mask == TWSI_STATUS_START ? "" : "repeated ");
		return (IIC_ETIMEOUT);
	}

	status = TWSI_READ(sc, sc->reg_status);
	debugf(sc, "status=%x\n", status);

	if (status != mask) {
		debugf(sc, "wrong status (%02x) after sending %sSTART condition\n",
		    status, mask == TWSI_STATUS_START ? "" : "repeated ");
		return (IIC_ESTATUS);
	}

	TWSI_WRITE(sc, sc->reg_data, slave);
	twsi_clear_iflg(sc);
	DELAY(1000);

	if (twsi_poll_ctrl(sc, timeout, TWSI_CONTROL_IFLG)) {
		debugf(sc, "timeout sending slave address (timeout=%d)\n", timeout);
		return (IIC_ETIMEOUT);
	}

	read_access = (slave & 0x1) ? 1 : 0;
	status = TWSI_READ(sc, sc->reg_status);
	if (status != (read_access ?
	    TWSI_STATUS_ADDR_R_ACK : TWSI_STATUS_ADDR_W_ACK)) {
		debugf(sc, "no ACK (status: %02x) after sending slave address\n",
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

	debugf(sc, "Bus clock is at %ju\n", clk);

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
			debugf(sc, "Using IIC_FAST mode with speed param=%x\n", param);
			break;
		case IIC_FASTEST:
		case IIC_UNKNOWN:
		default:
			param = sc->baud_rate[IIC_FAST].param;
			debugf(sc, "Using IIC_FASTEST/UNKNOWN mode with speed param=%x\n", param);
			break;
		}
#ifdef EXT_RESOURCES
	}
#endif

	debugf(sc, "Using clock param=%x\n", param);

	mtx_lock(&sc->mutex);
	TWSI_WRITE(sc, sc->reg_soft_reset, 0x1);
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

	debugf(sc, "%s\n", __func__);
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

	debugf(sc, "%s: slave=%x\n", __func__, slave);
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

	debugf(sc, "%s: slave=%x\n", __func__, slave);
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
			debugf(sc, "timeout reading data (delay=%d)\n", delay);
			rv = IIC_ETIMEOUT;
			goto out;
		}

		status = TWSI_READ(sc, sc->reg_status);
		if (status != (last_byte ?
		    TWSI_STATUS_DATA_RD_NOACK : TWSI_STATUS_DATA_RD_ACK)) {
			debugf(sc, "wrong status (%02x) while reading\n", status);
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
			debugf(sc, "timeout writing data (timeout=%d)\n", timeout);
			rv = IIC_ETIMEOUT;
			goto out;
		}

		status = TWSI_READ(sc, sc->reg_status);
		if (status != TWSI_STATUS_DATA_WR_ACK) {
			debugf(sc, "wrong status (%02x) while writing\n", status);
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
	uint32_t status;
	int error;

	sc = device_get_softc(dev);

	if (!sc->have_intr)
		return (iicbus_transfer_gen(dev, msgs, nmsgs));

	mtx_lock(&sc->mutex);
	KASSERT(sc->transfer == 0,
	    ("starting a transfer while another is active"));

	debugf(sc, "transmitting %d messages\n", nmsgs);
	status = TWSI_READ(sc, sc->reg_status);
	debugf(sc, "status=0x%x\n", status);
	if (status != TWSI_STATUS_IDLE) {
		debugf(sc, "Bad status at start of transfer\n");
		TWSI_WRITE(sc, sc->reg_control, TWSI_CONTROL_STOP);
		mtx_unlock(&sc->mutex);
		return (IIC_ESTATUS);
	}

	sc->nmsgs = nmsgs;
	sc->msgs = msgs;
	sc->msg_idx = 0;
	sc->transfer = 1;
	sc->error = 0;

#ifdef TWSI_DEBUG
	for (int i = 0; i < nmsgs; i++)
		debugf(sc, "msg %d is %d bytes long\n", i, msgs[i].len);
#endif

	/* Send start and re-enable interrupts */
	sc->control_val = TWSI_CONTROL_TWSIEN | TWSI_CONTROL_INTEN;
	TWSI_WRITE(sc, sc->reg_control, sc->control_val | TWSI_CONTROL_START);
	msleep_sbt(sc, &sc->mutex, 0, "twsi", 3000 * SBT_1MS, SBT_1MS, 0);
	debugf(sc, "pause finish\n");
	if (sc->error == 0 && sc->transfer != 0) {
		device_printf(sc->dev, "transfer timeout\n");
		sc->error = IIC_ETIMEOUT;
		sc->transfer = 0;
	}

	if (sc->error != 0)
		debugf(sc, "Error: %d\n", sc->error);

	/* Disable module and interrupts */
	debugf(sc, "status=0x%x\n", TWSI_READ(sc, sc->reg_status));
	TWSI_WRITE(sc, sc->reg_control, 0);
	debugf(sc, "status=0x%x\n", TWSI_READ(sc, sc->reg_status));
	error = sc->error;
	mtx_unlock(&sc->mutex);

	return (error);
}

static void
twsi_error(struct twsi_softc *sc, int err)
{
	/*
	 * Must send stop condition to abort the current transfer.
	 */
	debugf(sc, "Sending STOP condition for error %d\n", err);
	sc->transfer = 0;
	sc->error = err;
	sc->control_val = 0;
	TWSI_WRITE(sc, sc->reg_control, sc->control_val | TWSI_CONTROL_STOP);
}

static void
twsi_intr(void *arg)
{
	struct twsi_softc *sc;
	uint32_t status;
	int transfer_done = 0;

	sc = arg;

	mtx_lock(&sc->mutex);
	debugf(sc, "Got interrupt, current msg=%u\n", sc->msg_idx);

	status = TWSI_READ(sc, sc->reg_status);
	debugf(sc, "reg control = 0x%x, status = 0x%x\n",
	    TWSI_READ(sc, sc->reg_control), status);

	if (sc->transfer == 0) {
		device_printf(sc->dev, "interrupt without active transfer, "
		    "status = 0x%x\n", status);
		TWSI_WRITE(sc, sc->reg_control, sc->control_val |
		    TWSI_CONTROL_STOP);
		goto end;
	}

	switch (status) {
	case TWSI_STATUS_START:
	case TWSI_STATUS_RPTD_START:
		/* Transmit the address */
		debugf(sc, "Send address 0x%x\n",
		    sc->msgs[sc->msg_idx].slave);

		if (sc->msgs[sc->msg_idx].flags & IIC_M_RD)
			TWSI_WRITE(sc, sc->reg_data,
			    sc->msgs[sc->msg_idx].slave | LSB);
		else
			TWSI_WRITE(sc, sc->reg_data,
			    sc->msgs[sc->msg_idx].slave & ~LSB);
		TWSI_WRITE(sc, sc->reg_control, sc->control_val);
		break;

	case TWSI_STATUS_ADDR_W_ACK:
		debugf(sc, "Address ACK-ed (write)\n");

		if (sc->msgs[sc->msg_idx].len > 0) {
			/* Directly send the first byte */
			sc->sent_bytes = 1;
			debugf(sc, "Sending byte 0 (of %d) = %x\n",
			    sc->msgs[sc->msg_idx].len,
			    sc->msgs[sc->msg_idx].buf[0]);
			TWSI_WRITE(sc, sc->reg_data,
			    sc->msgs[sc->msg_idx].buf[0]);
		} else {
			debugf(sc, "Zero-length write, sending STOP\n");
			TWSI_WRITE(sc, sc->reg_control,
			    sc->control_val | TWSI_CONTROL_STOP);
		}
		break;

	case TWSI_STATUS_ADDR_R_ACK:
		debugf(sc, "Address ACK-ed (read)\n");
		sc->recv_bytes = 0;

		if (sc->msgs[sc->msg_idx].len == 0) {
			debugf(sc, "Zero-length read, sending STOP\n");
			TWSI_WRITE(sc, sc->reg_control,
			    sc->control_val | TWSI_CONTROL_STOP);
		} else if (sc->msgs[sc->msg_idx].len == 1) {
			sc->control_val &= ~TWSI_CONTROL_ACK;
		} else {
			sc->control_val |= TWSI_CONTROL_ACK;
		}
		break;

	case TWSI_STATUS_ADDR_W_NACK:
	case TWSI_STATUS_ADDR_R_NACK:
		debugf(sc, "Address NACK-ed\n");
		twsi_error(sc, IIC_ENOACK);
		break;
	case TWSI_STATUS_DATA_WR_NACK:
		debugf(sc, "Data byte NACK-ed\n");
		twsi_error(sc, IIC_ENOACK);
		break;
	case TWSI_STATUS_DATA_WR_ACK:
		debugf(sc, "Ack received after transmitting data\n");
		if (sc->sent_bytes == sc->msgs[sc->msg_idx].len) {
			debugf(sc, "Done sending all the bytes for msg %d\n", sc->msg_idx);
			/* Send stop, no interrupts on stop */
			if (!(sc->msgs[sc->msg_idx].flags & IIC_M_NOSTOP)) {
				debugf(sc, "Done TX data, send stop\n");
				TWSI_WRITE(sc, sc->reg_control,
				    sc->control_val | TWSI_CONTROL_STOP);
			} else {
				debugf(sc, "Done TX data with NO_STOP\n");
				TWSI_WRITE(sc, sc->reg_control, sc->control_val | TWSI_CONTROL_START);
			}
			sc->msg_idx++;
			if (sc->msg_idx == sc->nmsgs) {
				debugf(sc, "transfer_done=1\n");
				transfer_done = 1;
				sc->error = 0;
			} else {
				debugf(sc, "Send repeated start\n");
				TWSI_WRITE(sc, sc->reg_control, sc->control_val | TWSI_CONTROL_START);
			}
		} else {
			debugf(sc, "Sending byte %d (of %d) = %x\n",
			    sc->sent_bytes,
			    sc->msgs[sc->msg_idx].len,
			    sc->msgs[sc->msg_idx].buf[sc->sent_bytes]);
			TWSI_WRITE(sc, sc->reg_data,
			    sc->msgs[sc->msg_idx].buf[sc->sent_bytes]);
			TWSI_WRITE(sc, sc->reg_control,
			    sc->control_val);
			sc->sent_bytes++;
		}
		break;

	case TWSI_STATUS_DATA_RD_ACK:
		debugf(sc, "Ack received after receiving data\n");
		sc->msgs[sc->msg_idx].buf[sc->recv_bytes++] = TWSI_READ(sc, sc->reg_data);
		debugf(sc, "msg_len=%d recv_bytes=%d\n", sc->msgs[sc->msg_idx].len, sc->recv_bytes);

		/* If we only have one byte left, disable ACK */
		if (sc->msgs[sc->msg_idx].len - sc->recv_bytes == 1)
			sc->control_val &= ~TWSI_CONTROL_ACK;
		if (sc->msgs[sc->msg_idx].len == sc->recv_bytes) {
			debugf(sc, "Done with msg %d\n", sc->msg_idx);
			sc->msg_idx++;
			if (sc->msg_idx == sc->nmsgs - 1) {
				debugf(sc, "No more msgs\n");
				transfer_done = 1;
				sc->error = 0;
			}
		}
		TWSI_WRITE(sc, sc->reg_control, sc->control_val);
		break;

	case TWSI_STATUS_DATA_RD_NOACK:
		if (sc->msgs[sc->msg_idx].len - sc->recv_bytes == 1) {
			sc->msgs[sc->msg_idx].buf[sc->recv_bytes++] = TWSI_READ(sc, sc->reg_data);
			debugf(sc, "Done RX data, send stop (2)\n");
			if (!(sc->msgs[sc->msg_idx].flags & IIC_M_NOSTOP))
				TWSI_WRITE(sc, sc->reg_control,
				    sc->control_val | TWSI_CONTROL_STOP);
		} else {
			debugf(sc, "No ack when receiving data, sending stop anyway\n");
			if (!(sc->msgs[sc->msg_idx].flags & IIC_M_NOSTOP))
				TWSI_WRITE(sc, sc->reg_control,
				    sc->control_val | TWSI_CONTROL_STOP);
		}
		sc->transfer = 0;
		transfer_done = 1;
		sc->error = 0;
		break;

	case TWSI_STATUS_BUS_ERROR:
		debugf(sc, "Bus error\n");
		twsi_error(sc, IIC_EBUSERR);
		break;
	case TWSI_STATUS_ARBITRATION_LOST:
		debugf(sc, "Arbitration lost\n");
		twsi_error(sc, IIC_EBUSBSY);
		break;
	default:
		debugf(sc, "unexpected status 0x%x\n", status);
		twsi_error(sc, IIC_ESTATUS);
		break;
	}
	debugf(sc, "Refresh reg_control\n");

end:
	/*
	 * Newer Allwinner chips clear IFLG after writing 1 to it.
	 */
	TWSI_WRITE(sc, sc->reg_control, sc->control_val |
	    (sc->iflag_w1c ? TWSI_CONTROL_IFLG : 0));

	if (transfer_done == 1)
		sc->transfer = 0;
	debugf(sc, "Done with interrupt, transfer = %d\n", sc->transfer);
	if (sc->transfer == 0)
		wakeup(sc);
	mtx_unlock(&sc->mutex);
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
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->mutex, device_get_nameunit(dev), "twsi", MTX_DEF);

	if (bus_alloc_resources(dev, res_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		twsi_detach(dev);
		return (ENXIO);
	}

#ifdef TWSI_DEBUG
	sc->debug = 1;
#endif
	ctx = device_get_sysctl_ctx(dev);
	tree_node = device_get_sysctl_tree(dev);
	tree = SYSCTL_CHILDREN(tree_node);
	SYSCTL_ADD_INT(ctx, tree, OID_AUTO, "debug", CTLFLAG_RWTUN,
	    &sc->debug, 0, "Set debug level (zero to disable)");

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
