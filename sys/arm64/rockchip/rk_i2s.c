/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/resource.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/syscon/syscon.h>

#include "syscon_if.h"

#include "opt_snd.h"
#include <dev/sound/pcm/sound.h>
#include <dev/sound/fdt/audio_dai.h>
#include "audio_dai_if.h"

#define	AUDIO_BUFFER_SIZE	48000 * 4

#define	I2S_TXCR	0x0000
#define		I2S_CSR_2		(0 << 15)
#define		I2S_CSR_4		(1 << 15)
#define		I2S_CSR_6		(2 << 15)
#define		I2S_CSR_8		(3 << 15)
#define		I2S_TXCR_IBM_NORMAL	(0 << 9)
#define		I2S_TXCR_IBM_LJ		(1 << 9)
#define		I2S_TXCR_IBM_RJ		(2 << 9)
#define		I2S_TXCR_PBM_NODELAY	(0 << 7)
#define		I2S_TXCR_PBM_1		(1 << 7)
#define		I2S_TXCR_PBM_2		(2 << 7)
#define		I2S_TXCR_PBM_3		(3 << 7)
#define		I2S_TXCR_TFS_I2S	(0 << 5)
#define		I2S_TXCR_TFS_PCM	(1 << 5)
#define		I2S_TXCR_VDW_16		(0xf << 0)
#define	I2S_RXCR	0x0004
#define		I2S_RXCR_IBM_NORMAL	(0 << 9)
#define		I2S_RXCR_IBM_LJ		(1 << 9)
#define		I2S_RXCR_IBM_RJ		(2 << 9)
#define		I2S_RXCR_PBM_NODELAY	(0 << 7)
#define		I2S_RXCR_PBM_1		(1 << 7)
#define		I2S_RXCR_PBM_2		(2 << 7)
#define		I2S_RXCR_PBM_3		(3 << 7)
#define		I2S_RXCR_TFS_I2S	(0 << 5)
#define		I2S_RXCR_TFS_PCM	(1 << 5)
#define		I2S_RXCR_VDW_16		(0xf << 0)
#define	I2S_CKR		0x0008
#define		I2S_CKR_MSS_MASK	(1 << 27)
#define		I2S_CKR_MSS_MASTER	(0 << 27)
#define		I2S_CKR_MSS_SLAVE	(1 << 27)
#define		I2S_CKR_CKP		(1 << 26)
#define		I2S_CKR_MDIV(n)		(((n) - 1) << 16)
#define		I2S_CKR_MDIV_MASK	(0xff << 16)
#define		I2S_CKR_RSD(n)		(((n) - 1) << 8)
#define		I2S_CKR_RSD_MASK	(0xff << 8)
#define		I2S_CKR_TSD(n)		(((n) - 1) << 0)
#define		I2S_CKR_TSD_MASK	(0xff << 0)
#define	I2S_TXFIFOLR	0x000c
#define		TXFIFO0LR_MASK		0x3f
#define	I2S_DMACR	0x0010
#define		I2S_DMACR_RDE_ENABLE	(1 << 24)
#define		I2S_DMACR_RDL(n)	((n) << 16)
#define		I2S_DMACR_TDE_ENABLE	(1 << 8)
#define		I2S_DMACR_TDL(n)	((n) << 0)
#define	I2S_INTCR	0x0014
#define		I2S_INTCR_RFT(n)	(((n) - 1) << 20)
#define		I2S_INTCR_TFT(n)	(((n) - 1) << 4)
#define		I2S_INTCR_RXFIE		(1 << 16)
#define		I2S_INTCR_TXUIC		(1 << 2)
#define		I2S_INTCR_TXEIE		(1 << 0)
#define	I2S_INTSR	0x0018
#define		I2S_INTSR_RXFI		(1 << 16)
#define		I2S_INTSR_TXUI		(1 << 1)
#define		I2S_INTSR_TXEI		(1 << 0)
#define	I2S_XFER	0x001c
#define		I2S_XFER_RXS_START	(1 << 1)
#define		I2S_XFER_TXS_START	(1 << 0)
#define	I2S_CLR		0x0020
#define		I2S_CLR_RXC		(1 << 1)
#define		I2S_CLR_TXC		(1 << 0)
#define	I2S_TXDR	0x0024
#define	I2S_RXDR	0x0028
#define	I2S_RXFIFOLR	0x002c
#define		RXFIFO0LR_MASK		0x3f

/* syscon */
#define	GRF_SOC_CON8			0xe220
#define	I2S_IO_DIRECTION_MASK		7
#define	I2S_IO_DIRECTION_SHIFT		11
#define	I2S_IO_8CH_OUT_2CH_IN		0
#define	I2S_IO_6CH_OUT_4CH_IN		4
#define	I2S_IO_4CH_OUT_6CH_IN		6
#define	I2S_IO_2CH_OUT_8CH_IN		7

#define DIV_ROUND_CLOSEST(n,d)  (((n) + (d) / 2) / (d))

#define	RK_I2S_SAMPLING_RATE	48000
#define	FIFO_SIZE	32

static struct ofw_compat_data compat_data[] = {
	{ "rockchip,rk3066-i2s",		1 },
	{ "rockchip,rk3399-i2s",		1 },
	{ NULL,					0 }
};

static struct resource_spec rk_i2s_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

struct rk_i2s_softc {
	device_t	dev;
	struct resource	*res[2];
	struct mtx	mtx;
	clk_t		clk;
	clk_t		hclk;
	void *		intrhand;
	struct syscon	*grf;
	/* pointers to playback/capture buffers */
	uint32_t	play_ptr;
	uint32_t	rec_ptr;
};

#define	RK_I2S_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	RK_I2S_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)
#define	RK_I2S_READ_4(sc, reg)		bus_read_4((sc)->res[0], (reg))
#define	RK_I2S_WRITE_4(sc, reg, val)	bus_write_4((sc)->res[0], (reg), (val))

static int rk_i2s_probe(device_t dev);
static int rk_i2s_attach(device_t dev);
static int rk_i2s_detach(device_t dev);

static uint32_t sc_fmt[] = {
	SND_FORMAT(AFMT_S16_LE, 2, 0),
	0
};
static struct pcmchan_caps rk_i2s_caps = {RK_I2S_SAMPLING_RATE, RK_I2S_SAMPLING_RATE, sc_fmt, 0};


static int
rk_i2s_init(struct rk_i2s_softc *sc)
{
	uint32_t val;
	int error;

	clk_set_freq(sc->clk, RK_I2S_SAMPLING_RATE * 256,
	    CLK_SET_ROUND_DOWN);
	error = clk_enable(sc->clk);
	if (error != 0) {
		device_printf(sc->dev, "cannot enable i2s_clk clock\n");
		return (ENXIO);
	}

	val = I2S_INTCR_TFT(FIFO_SIZE/2);
	val |= I2S_INTCR_RFT(FIFO_SIZE/2);
	RK_I2S_WRITE_4(sc, I2S_INTCR, val);

	if (sc->grf && ofw_bus_is_compatible(sc->dev, "rockchip,rk3399-i2s")) {
		val = (I2S_IO_2CH_OUT_8CH_IN << I2S_IO_DIRECTION_SHIFT);
		val |= (I2S_IO_DIRECTION_MASK << I2S_IO_DIRECTION_SHIFT) << 16;
		SYSCON_WRITE_4(sc->grf, GRF_SOC_CON8, val);

		#if 0
		// HACK: enable IO domain
		val = (1 << 1);
		val |= (1 << 1) << 16;
		SYSCON_WRITE_4(sc->grf, 0xe640, val);
		#endif
	}

	RK_I2S_WRITE_4(sc, I2S_XFER, 0);

	return (0);
}

static int
rk_i2s_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Rockchip I2S");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_i2s_attach(device_t dev)
{
	struct rk_i2s_softc *sc;
	int error;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	if (bus_alloc_resources(dev, rk_i2s_spec, sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		error = ENXIO;
		goto fail;
	}

	error = clk_get_by_ofw_name(dev, 0, "i2s_hclk", &sc->hclk);
	if (error != 0) {
		device_printf(dev, "cannot get i2s_hclk clock\n");
		goto fail;
	}

	error = clk_get_by_ofw_name(dev, 0, "i2s_clk", &sc->clk);
	if (error != 0) {
		device_printf(dev, "cannot get i2s_clk clock\n");
		goto fail;
	}

	/* Activate the module clock. */
	error = clk_enable(sc->hclk);
	if (error != 0) {
		device_printf(dev, "cannot enable i2s_hclk clock\n");
		goto fail;
	}

	node = ofw_bus_get_node(dev);
	if (OF_hasprop(node, "rockchip,grf") &&
	    syscon_get_by_ofw_property(dev, node,
	    "rockchip,grf", &sc->grf) != 0) {
		device_printf(dev, "cannot get grf driver handle\n");
		return (ENXIO);
	}

	rk_i2s_init(sc);

	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);

fail:
	rk_i2s_detach(dev);
	return (error);
}

static int
rk_i2s_detach(device_t dev)
{
	struct rk_i2s_softc *i2s;

	i2s = device_get_softc(dev);

	if (i2s->hclk != NULL)
		clk_release(i2s->hclk);
	if (i2s->clk)
		clk_release(i2s->clk);

	if (i2s->intrhand != NULL)
		bus_teardown_intr(i2s->dev, i2s->res[1], i2s->intrhand);

	bus_release_resources(dev, rk_i2s_spec, i2s->res);
	mtx_destroy(&i2s->mtx);

	return (0);
}

static int
rk_i2s_dai_init(device_t dev, uint32_t format)
{
	uint32_t val, txcr, rxcr;
	struct rk_i2s_softc *sc;
	int fmt, pol, clk;

	sc = device_get_softc(dev);

	fmt = AUDIO_DAI_FORMAT_FORMAT(format);
	pol = AUDIO_DAI_FORMAT_POLARITY(format);
	clk = AUDIO_DAI_FORMAT_CLOCK(format);

	/* Set format */
	val = RK_I2S_READ_4(sc, I2S_CKR);

	val &= ~(I2S_CKR_MSS_MASK);
	switch (clk) {
	case AUDIO_DAI_CLOCK_CBM_CFM:
		val |= I2S_CKR_MSS_MASTER;
		break;
	case AUDIO_DAI_CLOCK_CBS_CFS:
		val |= I2S_CKR_MSS_SLAVE;
		break;
	default:
		return (EINVAL);
	}

	switch (pol) {
	case AUDIO_DAI_POLARITY_IB_NF:
		val |= I2S_CKR_CKP;
		break;
	case AUDIO_DAI_POLARITY_NB_NF:
		val &= ~I2S_CKR_CKP;
		break;
	default:
		return (EINVAL);
	}

	RK_I2S_WRITE_4(sc, I2S_CKR, val);

	txcr = I2S_TXCR_VDW_16 | I2S_CSR_2;
	rxcr = I2S_RXCR_VDW_16 | I2S_CSR_2;

	switch (fmt) {
	case AUDIO_DAI_FORMAT_I2S:
		txcr |= I2S_TXCR_IBM_NORMAL;
		rxcr |= I2S_RXCR_IBM_NORMAL;
		break;
	case AUDIO_DAI_FORMAT_LJ:
		txcr |= I2S_TXCR_IBM_LJ;
		rxcr |= I2S_RXCR_IBM_LJ;
		break;
	case AUDIO_DAI_FORMAT_RJ:
		txcr |= I2S_TXCR_IBM_RJ;
		rxcr |= I2S_RXCR_IBM_RJ;
		break;
	case AUDIO_DAI_FORMAT_DSPA:
		txcr |= I2S_TXCR_TFS_PCM;
		rxcr |= I2S_RXCR_TFS_PCM;
		txcr |= I2S_TXCR_PBM_1;
		rxcr |= I2S_RXCR_PBM_1;
		break;
	case AUDIO_DAI_FORMAT_DSPB:
		txcr |= I2S_TXCR_TFS_PCM;
		rxcr |= I2S_RXCR_TFS_PCM;
		txcr |= I2S_TXCR_PBM_2;
		rxcr |= I2S_RXCR_PBM_2;
		break;
	default:
		return EINVAL;
	}

	RK_I2S_WRITE_4(sc, I2S_TXCR, txcr);
	RK_I2S_WRITE_4(sc, I2S_RXCR, rxcr);

	RK_I2S_WRITE_4(sc, I2S_XFER, 0);

	return (0);
}


static int
rk_i2s_dai_intr(device_t dev, struct snd_dbuf *play_buf, struct snd_dbuf *rec_buf)
{
	struct rk_i2s_softc *sc;
	uint32_t status;
	uint32_t level;
	uint32_t val = 0x00;
	int ret = 0;

	sc = device_get_softc(dev);

	RK_I2S_LOCK(sc);
	status = RK_I2S_READ_4(sc, I2S_INTSR);

	if (status & I2S_INTSR_TXEI) {
		level = RK_I2S_READ_4(sc, I2S_TXFIFOLR) & TXFIFO0LR_MASK;
		uint8_t *samples;
		uint32_t count, size, readyptr, written;
		count = sndbuf_getready(play_buf);
		if (count > FIFO_SIZE - 1)
			count = FIFO_SIZE - 1;
		size = sndbuf_getsize(play_buf);
		readyptr = sndbuf_getreadyptr(play_buf);

		samples = (uint8_t*)sndbuf_getbuf(play_buf);
		written = 0;
		for (; level < count; level++) {
			val  = (samples[readyptr++ % size] << 0);
			val |= (samples[readyptr++ % size] << 8);
			val |= (samples[readyptr++ % size] << 16);
			val |= (samples[readyptr++ % size] << 24);
			written += 4;
			RK_I2S_WRITE_4(sc, I2S_TXDR, val);
		}
		sc->play_ptr += written;
		sc->play_ptr %= size;
		ret |= AUDIO_DAI_PLAY_INTR;
	}

	if (status & I2S_INTSR_RXFI) {
		level = RK_I2S_READ_4(sc, I2S_RXFIFOLR) & RXFIFO0LR_MASK;
		uint8_t *samples;
		uint32_t count, size, freeptr, recorded;
		count = sndbuf_getfree(rec_buf);
		size = sndbuf_getsize(rec_buf);
		freeptr = sndbuf_getfreeptr(rec_buf);
		samples = (uint8_t*)sndbuf_getbuf(rec_buf);
		recorded = 0;
		if (level > count / 4)
			level = count / 4;

		for (; level > 0; level--) {
			val = RK_I2S_READ_4(sc, I2S_RXDR);
			samples[freeptr++ % size] = val & 0xff;
			samples[freeptr++ % size] = (val >> 8) & 0xff;
			samples[freeptr++ % size] = (val >> 16) & 0xff;
			samples[freeptr++ % size] = (val >> 24) & 0xff;
			recorded += 4;
		}
		sc->rec_ptr += recorded;
		sc->rec_ptr %= size;
		ret |= AUDIO_DAI_REC_INTR;
	}

	RK_I2S_UNLOCK(sc);

	return (ret);
}

static struct pcmchan_caps *
rk_i2s_dai_get_caps(device_t dev)
{
	return (&rk_i2s_caps);
}

static int
rk_i2s_dai_trigger(device_t dev, int go, int pcm_dir)
{
	struct rk_i2s_softc 	*sc = device_get_softc(dev);
	uint32_t val;
	uint32_t clear_bit;

	if ((pcm_dir != PCMDIR_PLAY) && (pcm_dir != PCMDIR_REC))
		return (EINVAL);

	switch (go) {
	case PCMTRIG_START:
		val = RK_I2S_READ_4(sc, I2S_INTCR);
		if (pcm_dir == PCMDIR_PLAY)
			val |= I2S_INTCR_TXEIE;
		else if (pcm_dir == PCMDIR_REC)
			val |= I2S_INTCR_RXFIE;
		RK_I2S_WRITE_4(sc, I2S_INTCR, val);

		val = I2S_XFER_TXS_START | I2S_XFER_RXS_START;
		RK_I2S_WRITE_4(sc, I2S_XFER, val);
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		val = RK_I2S_READ_4(sc, I2S_INTCR);
		if (pcm_dir == PCMDIR_PLAY)
			val &= ~I2S_INTCR_TXEIE;
		else if (pcm_dir == PCMDIR_REC)
			val &= ~I2S_INTCR_RXFIE;
		RK_I2S_WRITE_4(sc, I2S_INTCR, val);

		/*
		 * If there is no other activity going on, stop transfers
		 */
		if ((val & (I2S_INTCR_TXEIE | I2S_INTCR_RXFIE)) == 0) {
			RK_I2S_WRITE_4(sc, I2S_XFER, 0);

			if (pcm_dir == PCMDIR_PLAY)
				clear_bit = I2S_CLR_TXC;
			else if (pcm_dir == PCMDIR_REC)
				clear_bit = I2S_CLR_RXC;
			else
				return (EINVAL);

			val = RK_I2S_READ_4(sc, I2S_CLR);
			val |= clear_bit;
			RK_I2S_WRITE_4(sc, I2S_CLR, val);

			while ((RK_I2S_READ_4(sc, I2S_CLR) & clear_bit) != 0)
				DELAY(1);
		}

		RK_I2S_LOCK(sc);
		if (pcm_dir == PCMDIR_PLAY)
			sc->play_ptr = 0;
		else
			sc->rec_ptr = 0;
		RK_I2S_UNLOCK(sc);
		break;
	}

	return (0);
}

static uint32_t
rk_i2s_dai_get_ptr(device_t dev, int pcm_dir)
{
	struct rk_i2s_softc *sc;
	uint32_t ptr;

	sc = device_get_softc(dev);

	RK_I2S_LOCK(sc);
	if (pcm_dir == PCMDIR_PLAY)
		ptr = sc->play_ptr;
	else
		ptr = sc->rec_ptr;
	RK_I2S_UNLOCK(sc);

	return ptr;
}

static int
rk_i2s_dai_setup_intr(device_t dev, driver_intr_t intr_handler, void *intr_arg)
{
	struct rk_i2s_softc 	*sc = device_get_softc(dev);

	if (bus_setup_intr(dev, sc->res[1],
	    INTR_TYPE_MISC | INTR_MPSAFE, NULL, intr_handler, intr_arg,
	    &sc->intrhand)) {
		device_printf(dev, "cannot setup interrupt handler\n");
		return (ENXIO);
	}

	return (0);
}

static uint32_t
rk_i2s_dai_set_chanformat(device_t dev, uint32_t format)
{

	return (0);
}

static int
rk_i2s_dai_set_sysclk(device_t dev, unsigned int rate, int dai_dir)
{
	struct rk_i2s_softc *sc;
	int error;

	sc = device_get_softc(dev);
	error = clk_disable(sc->clk);
	if (error != 0) {
		device_printf(sc->dev, "could not disable i2s_clk clock\n");
		return (error);
	}

	error = clk_set_freq(sc->clk, rate, CLK_SET_ROUND_DOWN);
	if (error != 0)
		device_printf(sc->dev, "could not set i2s_clk freq\n");

	error = clk_enable(sc->clk);
	if (error != 0) {
		device_printf(sc->dev, "could not enable i2s_clk clock\n");
		return (error);
	}

	return (0);
}

static uint32_t
rk_i2s_dai_set_chanspeed(device_t dev, uint32_t speed)
{
	struct rk_i2s_softc *sc;
	int error;
	uint32_t val;
	uint32_t bus_clock_div, lr_clock_div;
	uint64_t bus_clk_freq;
	uint64_t clk_freq;

	 sc = device_get_softc(dev);

	/* Set format */
	val = RK_I2S_READ_4(sc, I2S_CKR);

	if ((val & I2S_CKR_MSS_SLAVE) == 0) {
		error = clk_get_freq(sc->clk, &clk_freq);
		if (error != 0) {
			device_printf(sc->dev, "failed to get clk frequency: err=%d\n", error);
			return (error);
		}
		bus_clk_freq = 2 * 32 * speed;
		bus_clock_div = DIV_ROUND_CLOSEST(clk_freq, bus_clk_freq);
		lr_clock_div = bus_clk_freq / speed;

		val &= ~(I2S_CKR_MDIV_MASK | I2S_CKR_RSD_MASK | I2S_CKR_TSD_MASK);
		val |= I2S_CKR_MDIV(bus_clock_div);
		val |= I2S_CKR_RSD(lr_clock_div);
		val |= I2S_CKR_TSD(lr_clock_div);

		RK_I2S_WRITE_4(sc, I2S_CKR, val);
	}

	return (speed);
}

static device_method_t rk_i2s_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_i2s_probe),
	DEVMETHOD(device_attach,	rk_i2s_attach),
	DEVMETHOD(device_detach,	rk_i2s_detach),

	DEVMETHOD(audio_dai_init,	rk_i2s_dai_init),
	DEVMETHOD(audio_dai_setup_intr,	rk_i2s_dai_setup_intr),
	DEVMETHOD(audio_dai_set_sysclk,	rk_i2s_dai_set_sysclk),
	DEVMETHOD(audio_dai_set_chanspeed,	rk_i2s_dai_set_chanspeed),
	DEVMETHOD(audio_dai_set_chanformat,	rk_i2s_dai_set_chanformat),
	DEVMETHOD(audio_dai_intr,	rk_i2s_dai_intr),
	DEVMETHOD(audio_dai_get_caps,	rk_i2s_dai_get_caps),
	DEVMETHOD(audio_dai_trigger,	rk_i2s_dai_trigger),
	DEVMETHOD(audio_dai_get_ptr,	rk_i2s_dai_get_ptr),

	DEVMETHOD_END
};

static driver_t rk_i2s_driver = {
	"i2s",
	rk_i2s_methods,
	sizeof(struct rk_i2s_softc),
};

DRIVER_MODULE(rk_i2s, simplebus, rk_i2s_driver, 0, 0);
SIMPLEBUS_PNP_INFO(compat_data);
