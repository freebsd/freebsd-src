/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019-2020 Thomas Skibo <thomasskibo@yahoo.com>
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

/* Cadence / Zynq i2c driver.
 *
 * Reference: Zynq-7000 All Programmable SoC Technical Reference Manual.
 * (v1.12.2) July 1, 2018.  Xilinx doc UG585.  I2C Controller is documented
 * in Chapter 20.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/stdarg.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbus_if.h"

#ifdef I2CDEBUG
#define DPRINTF(...)	do { printf(__VA_ARGS__); } while (0)
#else
#define DPRINTF(...)    do { } while (0)
#endif

#if 0
#define HWTYPE_CDNS_R1P10	1
#endif
#define HWTYPE_CDNS_R1P14	2

static struct ofw_compat_data compat_data[] = {
#if 0
	{"cdns,i2c-r1p10",		HWTYPE_CDNS_R1P10},
#endif
	{"cdns,i2c-r1p14",		HWTYPE_CDNS_R1P14},
	{NULL,				0}
};

struct cdnc_i2c_softc {
	device_t		dev;
	device_t		iicbus;
	struct mtx		sc_mtx;
	struct resource		*mem_res;
	struct resource		*irq_res;
	void			*intrhandle;

	uint16_t		cfg_reg_shadow;
	uint16_t		istat;
	clk_t			ref_clk;
	uint32_t		ref_clock_freq;
	uint32_t		i2c_clock_freq;

	int			hwtype;
	int			hold;

	/* sysctls */
	unsigned int		i2c_clk_real_freq;
	unsigned int		interrupts;
	unsigned int		timeout_ints;
};

#define I2C_SC_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define	I2C_SC_UNLOCK(sc)	mtx_unlock(&(sc)->sc_mtx)
#define I2C_SC_LOCK_INIT(sc) \
	mtx_init(&(sc)->sc_mtx, device_get_nameunit((sc)->dev),	NULL, MTX_DEF)
#define I2C_SC_LOCK_DESTROY(sc)	mtx_destroy(&(sc)->sc_mtx)
#define I2C_SC_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)

#define RD2(sc, off)		(bus_read_2((sc)->mem_res, (off)))
#define WR2(sc, off, val)	(bus_write_2((sc)->mem_res, (off), (val)))
#define RD1(sc, off)		(bus_read_1((sc)->mem_res, (off)))
#define WR1(sc, off, val)	(bus_write_1((sc)->mem_res, (off), (val)))

/* Cadence I2C controller device registers. */
#define CDNC_I2C_CR			0x0000	/* Config register. */
#define    CDNC_I2C_CR_DIV_A_MASK		(3 << 14)
#define    CDNC_I2C_CR_DIV_A_SHIFT		14
#define    CDNC_I2C_CR_DIV_A(a)			((a) << 14)
#define    CDNC_I2C_CR_DIV_A_MAX		3
#define    CDNC_I2C_CR_DIV_B_MASK		(0x3f << 8)
#define    CDNC_I2C_CR_DIV_B_SHIFT		8
#define    CDNC_I2C_CR_DIV_B(b)			((b) << 8)
#define    CDNC_I2C_CR_DIV_B_MAX		63
#define    CDNC_I2C_CR_CLR_FIFO			(1 << 6)
#define    CDNC_I2C_CR_SLVMON_MODE		(1 << 5)
#define    CDNC_I2C_CR_HOLD			(1 << 4)
#define    CDNC_I2C_CR_ACKEN			(1 << 3)
#define    CDNC_I2C_CR_NEA			(1 << 2)
#define    CDNC_I2C_CR_MAST			(1 << 1)
#define    CDNC_I2C_CR_RNW			(1 << 0)

#define CDNC_I2C_SR			0x0004	/* Status register. */
#define    CDNC_I2C_SR_BUS_ACTIVE		(1 << 8)
#define    CDNC_I2C_SR_RX_OVF			(1 << 7)
#define    CDNC_I2C_SR_TX_VALID			(1 << 6)
#define    CDNC_I2C_SR_RX_VALID			(1 << 5)
#define    CDNC_I2C_SR_RXRW			(1 << 3)

#define CDNC_I2C_ADDR			0x0008	/* i2c address register. */
#define CDNC_I2C_DATA			0x000C	/* i2c data register. */

#define CDNC_I2C_ISR			0x0010	/* Int status register. */
#define    CDNC_I2C_ISR_ARB_LOST		(1 << 9)
#define    CDNC_I2C_ISR_RX_UNDF			(1 << 7)
#define    CDNC_I2C_ISR_TX_OVF			(1 << 6)
#define    CDNC_I2C_ISR_RX_OVF			(1 << 5)
#define    CDNC_I2C_ISR_SLV_RDY			(1 << 4)
#define    CDNC_I2C_ISR_XFER_TMOUT		(1 << 3)
#define    CDNC_I2C_ISR_XFER_NACK		(1 << 2)
#define    CDNC_I2C_ISR_XFER_DATA		(1 << 1)
#define    CDNC_I2C_ISR_XFER_DONE		(1 << 0)
#define    CDNC_I2C_ISR_ALL			0x2ff
#define CDNC_I2C_TRANS_SIZE		0x0014	/* Transfer size. */
#define CDNC_I2C_PAUSE			0x0018	/* Slv Monitor Pause reg. */
#define CDNC_I2C_TIME_OUT		0x001C	/* Time-out register. */
#define    CDNC_I2C_TIME_OUT_MIN		31
#define    CDNC_I2C_TIME_OUT_MAX		255
#define CDNC_I2C_IMR			0x0020	/* Int mask register. */
#define CDNC_I2C_IER			0x0024	/* Int enable register. */
#define CDNC_I2C_IDR			0x0028	/* Int disable register. */

#define CDNC_I2C_FIFO_SIZE		16
#define CDNC_I2C_DEFAULT_I2C_CLOCK	400000	/* 400Khz default */

#define CDNC_I2C_ISR_ERRS (CDNC_I2C_ISR_ARB_LOST | CDNC_I2C_ISR_RX_UNDF | \
	CDNC_I2C_ISR_TX_OVF | CDNC_I2C_ISR_RX_OVF | CDNC_I2C_ISR_XFER_TMOUT | \
	CDNC_I2C_ISR_XFER_NACK)

/* Configure clock dividers. */
static int
cdnc_i2c_set_freq(struct cdnc_i2c_softc *sc)
{
	uint32_t div_a, div_b, err, clk_out;
	uint32_t best_div_a, best_div_b, best_err;

	best_div_a = 0;
	best_div_b = 0;
	best_err = ~0U;

	/*
	 * The i2c controller has a two-stage clock divider to create
	 * the "clock enable" signal used to sample the incoming SCL and
	 * SDA signals.  The Clock Enable signal is divided by 22 to create
	 * the outgoing SCL signal.
	 *
	 * Try all div_a values and pick best match.
	 */
	for (div_a = 0; div_a <= CDNC_I2C_CR_DIV_A_MAX; div_a++) {
		div_b = sc->ref_clock_freq / (22 * sc->i2c_clock_freq *
		    (div_a + 1));
		if (div_b > CDNC_I2C_CR_DIV_B_MAX)
			continue;
		clk_out = sc->ref_clock_freq / (22 * (div_a + 1) *
		    (div_b + 1));
		err = clk_out > sc->i2c_clock_freq ?
		    clk_out - sc->i2c_clock_freq :
		    sc->i2c_clock_freq - clk_out;
		if (err < best_err) {
			best_err = err;
			best_div_a = div_a;
			best_div_b = div_b;
		}
	}

	if (best_err == ~0U) {
		device_printf(sc->dev, "cannot configure clock divider.\n");
		return (EINVAL); /* out of range */
	}

	clk_out = sc->ref_clock_freq / (22 * (best_div_a + 1) *
	    (best_div_b + 1));

	DPRINTF("%s: ref_clock_freq=%d i2c_clock_freq=%d\n", __func__,
	    sc->ref_clock_freq, sc->i2c_clock_freq);
	DPRINTF("%s: div_a=%d div_b=%d real-freq=%d\n", __func__, best_div_a,
	    best_div_b, clk_out);

	sc->cfg_reg_shadow &= ~(CDNC_I2C_CR_DIV_A_MASK |
	    CDNC_I2C_CR_DIV_B_MASK);
	sc->cfg_reg_shadow |= CDNC_I2C_CR_DIV_A(best_div_a) |
	    CDNC_I2C_CR_DIV_B(best_div_b);
	WR2(sc, CDNC_I2C_CR, sc->cfg_reg_shadow);

	sc->i2c_clk_real_freq = clk_out;

	return (0);
}

/* Initialize hardware. */
static int
cdnc_i2c_init_hw(struct cdnc_i2c_softc *sc)
{

	/* Reset config register and clear FIFO. */
	sc->cfg_reg_shadow = 0;
	WR2(sc, CDNC_I2C_CR, CDNC_I2C_CR_CLR_FIFO);
	sc->hold = 0;

	/* Clear and disable all interrupts. */
	WR2(sc, CDNC_I2C_ISR, CDNC_I2C_ISR_ALL);
	WR2(sc, CDNC_I2C_IDR, CDNC_I2C_ISR_ALL);

	/* Max out bogus time-out register. */
	WR1(sc, CDNC_I2C_TIME_OUT, CDNC_I2C_TIME_OUT_MAX);

	/* Set up clock dividers. */
	return (cdnc_i2c_set_freq(sc));
}

static int
cdnc_i2c_errs(struct cdnc_i2c_softc *sc, uint16_t istat)
{

	DPRINTF("%s: istat=0x%x\n", __func__, istat);

	/* XXX: clean up after errors. */

	/* Reset config register and clear FIFO. */
	sc->cfg_reg_shadow &= CDNC_I2C_CR_DIV_A_MASK | CDNC_I2C_CR_DIV_B_MASK;
	WR2(sc, CDNC_I2C_CR, sc->cfg_reg_shadow | CDNC_I2C_CR_CLR_FIFO);
	sc->hold = 0;

	if (istat & CDNC_I2C_ISR_XFER_TMOUT)
		return (IIC_ETIMEOUT);
	else if (istat & CDNC_I2C_ISR_RX_UNDF)
		return (IIC_EUNDERFLOW);
	else if (istat & (CDNC_I2C_ISR_RX_OVF | CDNC_I2C_ISR_TX_OVF))
		return (IIC_EOVERFLOW);
	else if (istat & CDNC_I2C_ISR_XFER_NACK)
		return (IIC_ENOACK);
	else if (istat & CDNC_I2C_ISR_ARB_LOST)
		return (IIC_EBUSERR); /* XXX: ???? */
	else
		/* Should not happen */
		return (IIC_NOERR);
}

static int
cdnc_i2c_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct cdnc_i2c_softc *sc = device_get_softc(dev);
	int error;

	DPRINTF("%s: speed=%d addr=0x%x\n", __func__, speed, addr);

	I2C_SC_LOCK(sc);

	sc->i2c_clock_freq = IICBUS_GET_FREQUENCY(sc->iicbus, speed);

	error = cdnc_i2c_init_hw(sc);

	I2C_SC_UNLOCK(sc);

	return (error ? IIC_ENOTSUPP : IIC_NOERR);
}

static void
cdnc_i2c_intr(void *arg)
{
	struct cdnc_i2c_softc *sc = (struct cdnc_i2c_softc *)arg;
	uint16_t status;

	I2C_SC_LOCK(sc);

	sc->interrupts++;

	/* Read active interrupts. */
	status = RD2(sc, CDNC_I2C_ISR) & ~RD2(sc, CDNC_I2C_IMR);

	/* Clear interrupts. */
	WR2(sc, CDNC_I2C_ISR, status);

	if (status & CDNC_I2C_ISR_XFER_TMOUT)
		sc->timeout_ints++;

	sc->istat |= status;

	if (status)
		wakeup(sc);

	I2C_SC_UNLOCK(sc);
}

static int
cdnc_i2c_xfer_rd(struct cdnc_i2c_softc *sc, struct iic_msg *msg)
{
	int error = IIC_NOERR;
	uint16_t flags = msg->flags;
	uint16_t len = msg->len;
	int idx = 0, nbytes, last, first = 1;
	uint16_t statr;

	DPRINTF("%s: flags=0x%x len=%d\n", __func__, flags, len);

#if 0
	if (sc->hwtype == HWTYPE_CDNS_R1P10 && (flags & IIC_M_NOSTOP))
		return (IIC_ENOTSUPP);
#endif

	I2C_SC_ASSERT_LOCKED(sc);

	/* Program config register. */
	sc->cfg_reg_shadow &= CDNC_I2C_CR_DIV_A_MASK | CDNC_I2C_CR_DIV_B_MASK;
	sc->cfg_reg_shadow |= CDNC_I2C_CR_HOLD | CDNC_I2C_CR_ACKEN |
	    CDNC_I2C_CR_NEA | CDNC_I2C_CR_MAST | CDNC_I2C_CR_RNW;
	WR2(sc, CDNC_I2C_CR, sc->cfg_reg_shadow | CDNC_I2C_CR_CLR_FIFO);
	sc->hold = 1;

	while (len > 0) {
		nbytes = MIN(CDNC_I2C_FIFO_SIZE - 2, len);
		WR1(sc, CDNC_I2C_TRANS_SIZE, nbytes);

		last = nbytes == len && !(flags & IIC_M_NOSTOP);
		if (last) {
			/* Clear HOLD bit on last transfer. */
			sc->cfg_reg_shadow &= ~CDNC_I2C_CR_HOLD;
			WR2(sc, CDNC_I2C_CR, sc->cfg_reg_shadow);
			sc->hold = 0;
		}

		/* Writing slv address for a start or repeated start. */
		if (first && !(flags & IIC_M_NOSTART))
			WR2(sc, CDNC_I2C_ADDR, msg->slave >> 1);
		first = 0;

		/* Enable FIFO interrupts and wait. */
		if (last)
			WR2(sc, CDNC_I2C_IER, CDNC_I2C_ISR_XFER_DONE |
			    CDNC_I2C_ISR_ERRS);
		else
			WR2(sc, CDNC_I2C_IER, CDNC_I2C_ISR_XFER_DATA |
			    CDNC_I2C_ISR_ERRS);

		error = mtx_sleep(sc, &sc->sc_mtx, 0, "cdi2c", hz);

		/* Disable FIFO interrupts. */
		WR2(sc, CDNC_I2C_IDR, CDNC_I2C_ISR_XFER_DATA |
		    CDNC_I2C_ISR_XFER_DONE | CDNC_I2C_ISR_ERRS);

		if (error == EWOULDBLOCK)
			error = cdnc_i2c_errs(sc, CDNC_I2C_ISR_XFER_TMOUT);
		else if (sc->istat & CDNC_I2C_ISR_ERRS)
			error = cdnc_i2c_errs(sc, sc->istat);
		sc->istat = 0;

		if (error != IIC_NOERR)
			break;

		/* Read nbytes from FIFO. */
		while (nbytes-- > 0) {
			statr = RD2(sc, CDNC_I2C_SR);
			if (!(statr & CDNC_I2C_SR_RX_VALID)) {
				printf("%s: RX FIFO underflow?\n", __func__);
				break;
			}
			msg->buf[idx++] = RD2(sc, CDNC_I2C_DATA);
			len--;
		}
	}

	return (error);
}

static int
cdnc_i2c_xfer_wr(struct cdnc_i2c_softc *sc, struct iic_msg *msg)
{
	int error = IIC_NOERR;
	uint16_t flags = msg->flags;
	uint16_t len = msg->len;
	int idx = 0, nbytes, last, first = 1;

	DPRINTF("%s: flags=0x%x len=%d\n", __func__, flags, len);

	I2C_SC_ASSERT_LOCKED(sc);

	/* Program config register. */
	sc->cfg_reg_shadow &= CDNC_I2C_CR_DIV_A_MASK | CDNC_I2C_CR_DIV_B_MASK;
	sc->cfg_reg_shadow |= CDNC_I2C_CR_HOLD | CDNC_I2C_CR_ACKEN |
	    CDNC_I2C_CR_NEA | CDNC_I2C_CR_MAST;
	WR2(sc, CDNC_I2C_CR, sc->cfg_reg_shadow | CDNC_I2C_CR_CLR_FIFO);
	sc->hold = 1;

	while (len > 0) {
		/* Put as much data into fifo as you can. */
		nbytes = MIN(len, CDNC_I2C_FIFO_SIZE -
		    RD1(sc, CDNC_I2C_TRANS_SIZE) - 1);
		len -= nbytes;
		while (nbytes-- > 0)
			WR2(sc, CDNC_I2C_DATA, msg->buf[idx++]);

		last = len == 0 && !(flags & IIC_M_NOSTOP);
		if (last) {
			/* Clear HOLD bit on last transfer. */
			sc->cfg_reg_shadow &= ~CDNC_I2C_CR_HOLD;
			WR2(sc, CDNC_I2C_CR, sc->cfg_reg_shadow);
			sc->hold = 0;
		}

		/* Perform START if this is start or repeated start. */
		if (first && !(flags & IIC_M_NOSTART))
			WR2(sc, CDNC_I2C_ADDR, msg->slave >> 1);
		first = 0;

		/* Enable FIFO interrupts. */
		WR2(sc, CDNC_I2C_IER, CDNC_I2C_ISR_XFER_DONE |
		    CDNC_I2C_ISR_ERRS);

		/* Wait for end of data transfer. */
		error = mtx_sleep(sc, &sc->sc_mtx, 0, "cdi2c", hz);

		/* Disable FIFO interrupts. */
		WR2(sc, CDNC_I2C_IDR, CDNC_I2C_ISR_XFER_DONE |
		    CDNC_I2C_ISR_ERRS);

		if (error == EWOULDBLOCK)
			error = cdnc_i2c_errs(sc, CDNC_I2C_ISR_XFER_TMOUT);
		else if (sc->istat & CDNC_I2C_ISR_ERRS)
			error = cdnc_i2c_errs(sc, sc->istat);
		sc->istat = 0;
		if (error)
			break;
	}

	return (error);
}

static int
cdnc_i2c_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	struct cdnc_i2c_softc *sc = device_get_softc(dev);
	int i, error = IIC_NOERR;

	DPRINTF("%s: nmsgs=%d\n", __func__, nmsgs);

	I2C_SC_LOCK(sc);

	for (i = 0; i < nmsgs; i++) {
		DPRINTF("%s: msg[%d]: hold=%d slv=0x%x flags=0x%x len=%d\n",
		    __func__, i, sc->hold, msgs[i].slave, msgs[i].flags,
		    msgs[i].len);

		if (!sc->hold && (msgs[i].flags & IIC_M_NOSTART))
			return (IIC_ENOTSUPP);

		if (msgs[i].flags & IIC_M_RD) {
			error = cdnc_i2c_xfer_rd(sc, &msgs[i]);
			if (error != IIC_NOERR)
				break;
		} else {
			error = cdnc_i2c_xfer_wr(sc, &msgs[i]);
			if (error != IIC_NOERR)
				break;
		}
	}

	I2C_SC_UNLOCK(sc);

	return (error);
}

static void
cdnc_i2c_add_sysctls(device_t dev)
{
	struct cdnc_i2c_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *child;

	ctx = device_get_sysctl_ctx(dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "i2c_clk_real_freq", CTLFLAG_RD,
	    &sc->i2c_clk_real_freq, 0, "i2c clock real frequency");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "_interrupts", CTLFLAG_RD,
	    &sc->interrupts, 0, "interrupt calls");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "_timeouts", CTLFLAG_RD,
	    &sc->timeout_ints, 0, "hardware timeout interrupts");
}


static int
cdnc_i2c_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Cadence I2C Controller");

	return (BUS_PROBE_DEFAULT);
}


static int cdnc_i2c_detach(device_t);

static int
cdnc_i2c_attach(device_t dev)
{
	struct cdnc_i2c_softc *sc;
	int rid, err;
	phandle_t node;
	pcell_t cell;
	uint64_t freq;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->hwtype = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	I2C_SC_LOCK_INIT(sc);

	/* Get ref-clock and i2c-clock properties. */
	node = ofw_bus_get_node(dev);
	if (OF_getprop(node, "ref-clock", &cell, sizeof(cell)) > 0)
		sc->ref_clock_freq = fdt32_to_cpu(cell);
	else if (clk_get_by_ofw_index(dev, node, 0, &sc->ref_clk) == 0) {
		if ((err = clk_enable(sc->ref_clk)) != 0)
			device_printf(dev, "Cannot enable clock. err=%d\n",
			    err);
		else if ((err = clk_get_freq(sc->ref_clk, &freq)) != 0)
			device_printf(dev,
			    "Cannot get clock frequency. err=%d\n", err);
		else
			sc->ref_clock_freq = freq;
	}
	else {
		device_printf(dev, "must have ref-clock property\n");
		return (ENXIO);
	}
	if (OF_getprop(node, "clock-frequency", &cell, sizeof(cell)) > 0)
		sc->i2c_clock_freq = fdt32_to_cpu(cell);
	else
		sc->i2c_clock_freq = CDNC_I2C_DEFAULT_I2C_CLOCK;

	/* Get memory resource. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "could not allocate memory resources.\n");
		cdnc_i2c_detach(dev);
		return (ENOMEM);
	}

	/* Allocate IRQ. */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "could not allocate IRQ resource.\n");
		cdnc_i2c_detach(dev);
		return (ENOMEM);
	}

	/* Activate the interrupt. */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, cdnc_i2c_intr, sc, &sc->intrhandle);
	if (err) {
		device_printf(dev, "could not setup IRQ.\n");
		cdnc_i2c_detach(dev);
		return (err);
	}

	/* Configure the device. */
	err = cdnc_i2c_init_hw(sc);
	if (err) {
		cdnc_i2c_detach(dev);
		return (err);
	}

	sc->iicbus = device_add_child(dev, "iicbus", DEVICE_UNIT_ANY);

	cdnc_i2c_add_sysctls(dev);

	/* Probe and attach iicbus when interrupts work. */
	bus_delayed_attach_children(dev);
	return (0);
}

static int
cdnc_i2c_detach(device_t dev)
{
	struct cdnc_i2c_softc *sc = device_get_softc(dev);

	bus_generic_detach(dev);

	if (sc->ref_clk != NULL) {
		clk_release(sc->ref_clk);
		sc->ref_clk = NULL;
	}

	/* Disable hardware. */
	if (sc->mem_res != NULL) {
		sc->cfg_reg_shadow = 0;
		WR2(sc, CDNC_I2C_CR, CDNC_I2C_CR_CLR_FIFO);

		/* Clear and disable all interrupts. */
		WR2(sc, CDNC_I2C_ISR, CDNC_I2C_ISR_ALL);
		WR2(sc, CDNC_I2C_IDR, CDNC_I2C_ISR_ALL);
	}

	/* Teardown and release interrupt. */
	if (sc->irq_res != NULL) {
		if (sc->intrhandle)
			bus_teardown_intr(dev, sc->irq_res, sc->intrhandle);
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
		sc->irq_res = NULL;
	}

	/* Release memory resource. */
	if (sc->mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem_res), sc->mem_res);
		sc->mem_res = NULL;
	}

	I2C_SC_LOCK_DESTROY(sc);

	return (0);
}


static phandle_t
cdnc_i2c_get_node(device_t bus, device_t dev)
{

	return (ofw_bus_get_node(bus));
}

static device_method_t cdnc_i2c_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			cdnc_i2c_probe),
	DEVMETHOD(device_attach,		cdnc_i2c_attach),
	DEVMETHOD(device_detach,		cdnc_i2c_detach),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,		cdnc_i2c_get_node),

	/* iicbus methods */
	DEVMETHOD(iicbus_callback,              iicbus_null_callback),
	DEVMETHOD(iicbus_reset,			cdnc_i2c_reset),
	DEVMETHOD(iicbus_transfer,		cdnc_i2c_transfer),

	DEVMETHOD_END
};

static driver_t cdnc_i2c_driver = {
	"cdnc_i2c",
	cdnc_i2c_methods,
	sizeof(struct cdnc_i2c_softc),
};

DRIVER_MODULE(cdnc_i2c, simplebus, cdnc_i2c_driver, NULL, NULL);
DRIVER_MODULE(ofw_iicbus, cdnc_i2c, ofw_iicbus_driver, NULL, NULL);
MODULE_DEPEND(cdnc_i2c, iicbus, 1, 1, 1);
MODULE_DEPEND(cdnc_i2c, ofw_iicbus, 1, 1, 1);
SIMPLEBUS_PNP_INFO(compat_data);
