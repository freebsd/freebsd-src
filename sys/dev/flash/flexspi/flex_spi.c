/*-
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <geom/geom_disk.h>

#include <machine/bus.h>

#include <dev/clk/clk.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <vm/pmap.h>

#include "flex_spi.h"

static MALLOC_DEFINE(SECTOR_BUFFER, "flex_spi", "FSL QSPI sector buffer memory");

#define	AHB_LUT_ID	31
#define	MHZ(x)			((x)*1000*1000)
#define	SPI_DEFAULT_CLK_RATE	(MHZ(10))

static int driver_flags = 0;
SYSCTL_NODE(_hw, OID_AUTO, flex_spi, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "FlexSPI driver parameters");
SYSCTL_INT(_hw_flex_spi, OID_AUTO, driver_flags, CTLFLAG_RDTUN, &driver_flags, 0,
    "Configuration flags and quirks");

static struct ofw_compat_data flex_spi_compat_data[] = {
	{"nxp,lx2160a-fspi",  true},
	{NULL,               false}
};

struct flex_spi_flash_info {
	char*		name;
	uint32_t	jedecid;
	uint32_t	sectorsize;
	uint32_t	sectorcount;
	uint32_t	erasesize;
	uint32_t	maxclk;
};

/* Add information about supported Flashes. TODO: use SFDP instead */
static struct flex_spi_flash_info flex_spi_flash_info[] = {
		{"W25Q128JW", 0x001860ef, 64*1024, 256, 4096, MHZ(100)},
		{NULL, 0, 0, 0, 0, 0}
};

struct flex_spi_softc
{
	device_t		dev;
	unsigned int		flags;

	struct bio_queue_head	bio_queue;
	struct mtx		disk_mtx;
	struct disk		*disk;
	struct proc		*p;
	unsigned int		taskstate;
	uint8_t			*buf;

	struct resource		*ahb_mem_res;
	struct resource		*mem_res;

	clk_t			fspi_clk_en;
	clk_t			fspi_clk;
	uint64_t		fspi_clk_en_hz;
	uint64_t		fspi_clk_hz;

	/* TODO: support more than one Flash per bus */
	uint64_t		fspi_max_clk;
	uint32_t		quirks;

	/* Flash parameters */
	uint32_t		sectorsize;
	uint32_t		sectorcount;
	uint32_t		erasesize;
};

static int flex_spi_read(struct flex_spi_softc *sc, off_t offset, caddr_t data,
    size_t count);
static int flex_spi_write(struct flex_spi_softc *sc, off_t offset,
    uint8_t *data, size_t size);

static int flex_spi_attach(device_t dev);
static int flex_spi_probe(device_t dev);
static int flex_spi_detach(device_t dev);

/* disk routines */
static int flex_spi_open(struct disk *dp);
static int flex_spi_close(struct disk *dp);
static int flex_spi_ioctl(struct disk *, u_long, void *, int, struct thread *);
static void flex_spi_strategy(struct bio *bp);
static int flex_spi_getattr(struct bio *bp);
static void flex_spi_task(void *arg);

static uint32_t
read_reg(struct flex_spi_softc *sc, uint32_t offset)
{

	return ((bus_read_4(sc->mem_res, offset)));
}

static void
write_reg(struct flex_spi_softc *sc, uint32_t offset, uint32_t value)
{

	bus_write_4(sc->mem_res, offset, (value));
}

static int
reg_read_poll_tout(struct flex_spi_softc *sc, uint32_t offset, uint32_t mask,
    uint32_t delay_us, uint32_t iterations, bool positive)
{
	uint32_t reg;
	uint32_t condition = 0;

	do {
		reg = read_reg(sc, offset);
		if (positive)
			condition = ((reg & mask) == 0);
		else
			condition = ((reg & mask) != 0);

		if (condition == 0)
			break;

		DELAY(delay_us);
	} while (condition && (--iterations > 0));

	return (condition != 0);
}

static int
flex_spi_clk_setup(struct flex_spi_softc *sc, uint32_t rate)
{
	int ret = 0;

	/* disable to avoid glitching */
	ret |= clk_disable(sc->fspi_clk_en);
	ret |= clk_disable(sc->fspi_clk);

	ret |= clk_set_freq(sc->fspi_clk, rate, 0);
	sc->fspi_clk_hz = rate;

	/* enable clocks back */
	ret |= clk_enable(sc->fspi_clk_en);
	ret |= clk_enable(sc->fspi_clk);

	if (ret)
		return (EINVAL);

	return (0);
}

static void
flex_spi_prepare_lut(struct flex_spi_softc *sc, uint8_t op)
{
	uint32_t lut_id;
	uint32_t lut;

	/* unlock LUT */
	write_reg(sc, FSPI_LUTKEY, FSPI_LUTKEY_VALUE);
	write_reg(sc, FSPI_LCKCR, FSPI_LCKER_UNLOCK);

	/* Read JEDEC ID */
	lut_id = 0;

	switch (op) {
	case LUT_FLASH_CMD_JEDECID:
		lut = LUT_DEF(0, LUT_CMD, LUT_PAD(1), FSPI_CMD_READ_IDENT);
		lut |= LUT_DEF(1, LUT_NXP_READ, LUT_PAD(1), 0);
		write_reg(sc, FSPI_LUT_REG(lut_id), lut);
		write_reg(sc, FSPI_LUT_REG(lut_id) + 4, 0);
		break;
	case LUT_FLASH_CMD_READ:
		lut = LUT_DEF(0, LUT_CMD, LUT_PAD(1), FSPI_CMD_FAST_READ);
		lut |= LUT_DEF(1, LUT_ADDR, LUT_PAD(1), 3*8);
		write_reg(sc, FSPI_LUT_REG(lut_id), lut);
		lut = LUT_DEF(0, LUT_DUMMY, LUT_PAD(1), 1*8);
		lut |= LUT_DEF(1, LUT_NXP_READ, LUT_PAD(1), 0);
		write_reg(sc, FSPI_LUT_REG(lut_id) + 4, lut);
		write_reg(sc, FSPI_LUT_REG(lut_id) + 8, 0);
		break;
	case LUT_FLASH_CMD_STATUS_READ:
		lut = LUT_DEF(0, LUT_CMD, LUT_PAD(1), FSPI_CMD_READ_STATUS);
		lut |= LUT_DEF(1, LUT_NXP_READ, LUT_PAD(1), 0);
		write_reg(sc, FSPI_LUT_REG(lut_id), lut);
		write_reg(sc, FSPI_LUT_REG(lut_id) + 4, 0);
		break;
	case LUT_FLASH_CMD_PAGE_PROGRAM:
		lut = LUT_DEF(0, LUT_CMD, LUT_PAD(1), FSPI_CMD_PAGE_PROGRAM);
		lut |= LUT_DEF(1, LUT_ADDR, LUT_PAD(1), 3*8);
		write_reg(sc, FSPI_LUT_REG(lut_id), lut);
		lut = LUT_DEF(0, LUT_NXP_WRITE, LUT_PAD(1), 0);
		write_reg(sc, FSPI_LUT_REG(lut_id) + 4, lut);
		write_reg(sc, FSPI_LUT_REG(lut_id) + 8, 0);
		break;
	case LUT_FLASH_CMD_WRITE_ENABLE:
		lut = LUT_DEF(0, LUT_CMD, LUT_PAD(1), FSPI_CMD_WRITE_ENABLE);
		write_reg(sc, FSPI_LUT_REG(lut_id), lut);
		write_reg(sc, FSPI_LUT_REG(lut_id) + 4, 0);
		break;
	case LUT_FLASH_CMD_WRITE_DISABLE:
		lut = LUT_DEF(0, LUT_CMD, LUT_PAD(1), FSPI_CMD_WRITE_DISABLE);
		write_reg(sc, FSPI_LUT_REG(lut_id), lut);
		write_reg(sc, FSPI_LUT_REG(lut_id) + 4, 0);
		break;
	case LUT_FLASH_CMD_SECTOR_ERASE:
		lut = LUT_DEF(0, LUT_CMD, LUT_PAD(1), FSPI_CMD_SECTOR_ERASE);
		lut |= LUT_DEF(1, LUT_ADDR, LUT_PAD(1), 3*8);
		write_reg(sc, FSPI_LUT_REG(lut_id), lut);
		write_reg(sc, FSPI_LUT_REG(lut_id) + 4, 0);
		break;
	default:
		write_reg(sc, FSPI_LUT_REG(lut_id), 0);
	}

	/* lock LUT */
	write_reg(sc, FSPI_LUTKEY, FSPI_LUTKEY_VALUE);
	write_reg(sc, FSPI_LCKCR, FSPI_LCKER_LOCK);
}

static void
flex_spi_prepare_ahb_lut(struct flex_spi_softc *sc)
{
	uint32_t lut_id;
	uint32_t lut;

	/* unlock LUT */
	write_reg(sc, FSPI_LUTKEY, FSPI_LUTKEY_VALUE);
	write_reg(sc, FSPI_LCKCR, FSPI_LCKER_UNLOCK);

	lut_id = AHB_LUT_ID;
	lut = LUT_DEF(0, LUT_CMD, LUT_PAD(1), FSPI_CMD_FAST_READ);
	lut |= LUT_DEF(1, LUT_ADDR, LUT_PAD(1), 3*8);
	write_reg(sc, FSPI_LUT_REG(lut_id), lut);
	lut = LUT_DEF(0, LUT_DUMMY, LUT_PAD(1), 1*8);
	lut |= LUT_DEF(1, LUT_NXP_READ, LUT_PAD(1), 0);
	write_reg(sc, FSPI_LUT_REG(lut_id) + 4, lut);
	write_reg(sc, FSPI_LUT_REG(lut_id) + 8, 0);

	/* lock LUT */
	write_reg(sc, FSPI_LUTKEY, FSPI_LUTKEY_VALUE);
	write_reg(sc, FSPI_LCKCR, FSPI_LCKER_LOCK);
}

#define	DIR_READ	0
#define	DIR_WRITE	1

static void
flex_spi_read_rxfifo(struct flex_spi_softc *sc, uint8_t *buf, uint8_t size)
{
	int i, ret, reg;

	/*
	 * Default value of water mark level is 8 bytes, hence in single
	 * read request controller can read max 8 bytes of data.
	 */
	for (i = 0; i < size; i += 4) {
		/* Wait for RXFIFO available */
		if (i % 8 == 0) {
			ret = reg_read_poll_tout(sc, FSPI_INTR, FSPI_INTR_IPRXWA,
			    1, 50000, 1);
			if (ret)
				device_printf(sc->dev,
				    "timed out waiting for FSPI_INTR_IPRXWA\n");
		}

		if (i % 8 == 0)
			reg = read_reg(sc, FSPI_RFDR);
		else
			reg = read_reg(sc, FSPI_RFDR + 4);

		if (size  >= (i + 4))
			*(uint32_t *)(buf + i) = reg;
		else
			memcpy(buf + i, &reg, size - i);

		/* move the FIFO pointer */
		if (i % 8 != 0)
			write_reg(sc, FSPI_INTR, FSPI_INTR_IPRXWA);
	}

	/* invalid the RXFIFO */
	write_reg(sc, FSPI_IPRXFCR, FSPI_IPRXFCR_CLR);
	/* move the FIFO pointer */
	write_reg(sc, FSPI_INTR, FSPI_INTR_IPRXWA);
}

static void
flex_spi_write_txfifo(struct flex_spi_softc *sc, uint8_t *buf, uint8_t size)
{
	int i, ret, reg;

	/* invalid the TXFIFO */
	write_reg(sc, FSPI_IPRXFCR, FSPI_IPTXFCR_CLR);

	/*
	 * Default value of water mark level is 8 bytes, hence in single
	 * read request controller can read max 8 bytes of data.
	 */
	for (i = 0; i < size; i += 4) {
		/* Wait for RXFIFO available */
		if (i % 8 == 0) {
			ret = reg_read_poll_tout(sc, FSPI_INTR, FSPI_INTR_IPTXWE,
			    1, 50000, 1);
			if (ret)
				device_printf(sc->dev,
				    "timed out waiting for FSPI_INTR_IPRXWA\n");
		}

		if (size  >= (i + 4))
			reg = *(uint32_t *)(buf + i);
		else {
			reg = 0;
			memcpy(&reg, buf + i, size - i);
		}

		if (i % 8 == 0)
			write_reg(sc, FSPI_TFDR, reg);
		else
			write_reg(sc, FSPI_TFDR + 4, reg);

		/* move the FIFO pointer */
		if (i % 8 != 0)
			write_reg(sc, FSPI_INTR, FSPI_INTR_IPTXWE);
	}

	/* move the FIFO pointer */
	write_reg(sc, FSPI_INTR, FSPI_INTR_IPTXWE);
}

static int
flex_spi_do_op(struct flex_spi_softc *sc, uint32_t op, uint32_t addr,
    uint8_t *buf, uint8_t size, uint8_t dir)
{

	uint32_t cnt = 1000, reg;

	reg = read_reg(sc, FSPI_IPRXFCR);
	/* invalidate RXFIFO first */
	reg &= ~FSPI_IPRXFCR_DMA_EN;
	reg |= FSPI_IPRXFCR_CLR;
	write_reg(sc, FSPI_IPRXFCR, reg);

	/* Prepare LUT */
	flex_spi_prepare_lut(sc, op);

	write_reg(sc, FSPI_IPCR0, addr);
	/*
	 * Always start the sequence at the same index since we update
	 * the LUT at each BIO operation. And also specify the DATA
	 * length, since it's has not been specified in the LUT.
	 */
	write_reg(sc, FSPI_IPCR1, size |
	    (0 << FSPI_IPCR1_SEQID_SHIFT) | (0 << FSPI_IPCR1_SEQNUM_SHIFT));

	if ((size != 0) && (dir == DIR_WRITE))
		flex_spi_write_txfifo(sc, buf, size);

	/* Trigger the LUT now. */
	write_reg(sc, FSPI_IPCMD, FSPI_IPCMD_TRG);


	/* Wait for completion. */
	do {
		reg = read_reg(sc, FSPI_INTR);
		if (reg & FSPI_INTR_IPCMDDONE) {
			write_reg(sc, FSPI_INTR, FSPI_INTR_IPCMDDONE);
			break;
		}
		DELAY(1);
	} while (--cnt);
	if (cnt == 0) {
		device_printf(sc->dev, "timed out waiting for command completion\n");
		return (ETIMEDOUT);
	}

	/* Invoke IP data read, if request is of data read. */
	if ((size != 0) && (dir == DIR_READ))
		flex_spi_read_rxfifo(sc, buf, size);

	return (0);
}

static int
flex_spi_wait_for_controller(struct flex_spi_softc *sc)
{
	int err;

	/* Wait for controller being ready. */
	err = reg_read_poll_tout(sc, FSPI_STS0,
	   FSPI_STS0_ARB_IDLE, 1, POLL_TOUT, 1);

	return (err);
}

static int
flex_spi_wait_for_flash(struct flex_spi_softc *sc)
{
	int ret;
	uint32_t status = 0;

	ret = flex_spi_wait_for_controller(sc);
	if (ret != 0) {
		device_printf(sc->dev, "%s: timed out waiting for controller", __func__);
		return (ret);
	}

	do {
		ret = flex_spi_do_op(sc, LUT_FLASH_CMD_STATUS_READ, 0, (void*)&status,
		    1, DIR_READ);
		if (ret != 0) {
			device_printf(sc->dev, "ERROR: failed to get flash status\n");
			return (ret);
		}

	} while (status & STATUS_WIP);

	return (0);
}

static int
flex_spi_identify(struct flex_spi_softc *sc)
{
	int ret;
	uint32_t id = 0;
	struct flex_spi_flash_info *finfo = flex_spi_flash_info;

	ret = flex_spi_do_op(sc, LUT_FLASH_CMD_JEDECID, 0, (void*)&id, sizeof(id), DIR_READ);
	if (ret != 0) {
		device_printf(sc->dev, "ERROR: failed to identify device\n");
		return (ret);
	}

	/* XXX TODO: SFDP to be implemented */
	while (finfo->jedecid != 0) {
		if (id == finfo->jedecid) {
			device_printf(sc->dev, "found %s Flash\n", finfo->name);
			sc->sectorsize = finfo->sectorsize;
			sc->sectorcount = finfo->sectorcount;
			sc->erasesize = finfo->erasesize;
			sc->fspi_max_clk = finfo->maxclk;
			return (0);
		}
		finfo++;
	}

	return (EINVAL);
}

static inline int
flex_spi_force_ip_mode(struct flex_spi_softc *sc)
{

	if (sc->quirks & FSPI_QUIRK_USE_IP_ONLY)
		return (1);
	if (driver_flags & FSPI_QUIRK_USE_IP_ONLY)
		return (1);

	return (0);
}

static int
flex_spi_read(struct flex_spi_softc *sc, off_t offset, caddr_t data,
    size_t count)
{
	int err;
	size_t len;

	/* Wait for controller being ready. */
	err = flex_spi_wait_for_controller(sc);
	if (err)
		device_printf(sc->dev,
		    "warning: spi_read, timed out waiting for controller");

	/* Use AHB access whenever we can */
	if (flex_spi_force_ip_mode(sc) != 0) {
		do {
			if (((offset % 4) != 0) || (count < 4)) {
				*(uint8_t*)data = bus_read_1(sc->ahb_mem_res, offset);
				data++;
				count--;
				offset++;
			} else {
				*(uint32_t*)data = bus_read_4(sc->ahb_mem_res, offset);
				data += 4;
				count -= 4;
				offset += 4;
			}
		} while (count);

		return (0);
	}

	do {
		len = min(64, count);
		err = flex_spi_do_op(sc, LUT_FLASH_CMD_READ, offset, (void*)data,
				len, DIR_READ);
		if (err)
			return (err);
		offset += len;
		data += len;
		count -= len;
	} while (count);

	return (0);
}

static int
flex_spi_write(struct flex_spi_softc *sc, off_t offset, uint8_t *data,
    size_t size)
{
	int ret = 0;
	size_t ptr;

	flex_spi_wait_for_flash(sc);
	ret = flex_spi_do_op(sc, LUT_FLASH_CMD_WRITE_ENABLE, offset, NULL,
				0, DIR_READ);
	if (ret != 0) {
		device_printf(sc->dev, "ERROR: failed to enable writes\n");
		return (ret);
	}
	flex_spi_wait_for_flash(sc);

	/* per-sector write */
	while (size > 0) {
		uint32_t sector_base = rounddown2(offset, sc->erasesize);
		size_t size_in_sector = size;

		if (size_in_sector + offset > sector_base + sc->erasesize)
			size_in_sector = sector_base + sc->erasesize - offset;

		/* Read sector */
		ret = flex_spi_read(sc, sector_base, sc->buf, sc->erasesize);
		if (ret != 0) {
			device_printf(sc->dev, "ERROR: failed to read sector %d\n",
			    sector_base);
			goto exit;
		}

		/* Erase sector */
		flex_spi_wait_for_flash(sc);
		ret = flex_spi_do_op(sc, LUT_FLASH_CMD_SECTOR_ERASE, offset, NULL,
				0, DIR_READ);
		if (ret != 0) {
			device_printf(sc->dev, "ERROR: failed to erase sector %d\n",
			    sector_base);
			goto exit;
		}

		/* Update buffer with input data */
		memcpy(sc->buf + (offset - sector_base), data, size_in_sector);

		/* Write buffer back to the flash
		 * Up to 32 bytes per single request, request cannot spread
		 * across 256-byte page boundary
		 */
		for (ptr = 0; ptr < sc->erasesize; ptr += 32) {
			flex_spi_wait_for_flash(sc);
			ret = flex_spi_do_op(sc, LUT_FLASH_CMD_PAGE_PROGRAM,
			    sector_base + ptr, (void*)(sc->buf + ptr), 32, DIR_WRITE);
			if (ret != 0) {
				device_printf(sc->dev, "ERROR: failed to write address %ld\n",
				   sector_base + ptr);
				goto exit;
			}
		}

		/* update pointers */
		size = size - size_in_sector;
		offset = offset + size;
	}

	flex_spi_wait_for_flash(sc);
	ret = flex_spi_do_op(sc, LUT_FLASH_CMD_WRITE_DISABLE, offset, (void*)sc->buf,
				0, DIR_READ);
	if (ret != 0) {
		device_printf(sc->dev, "ERROR: failed to disable writes\n");
		goto exit;
	}
	flex_spi_wait_for_flash(sc);

exit:

	return (ret);
}

static int
flex_spi_default_setup(struct flex_spi_softc *sc)
{
	int ret, i;
	uint32_t reg;

	/* Default clock speed */
	ret = flex_spi_clk_setup(sc, SPI_DEFAULT_CLK_RATE);
	if (ret)
		return (ret);

	/* Reset the module */
	/* w1c register, wait unit clear */
	reg = read_reg(sc, FSPI_MCR0);
	reg |= FSPI_MCR0_SWRST;
	write_reg(sc, FSPI_MCR0, reg);
	ret = reg_read_poll_tout(sc, FSPI_MCR0, FSPI_MCR0_SWRST, 1000, POLL_TOUT, 0);
	if (ret != 0) {
		device_printf(sc->dev, "time out waiting for reset");
		return (ret);
	}

	/* Disable the module */
	write_reg(sc, FSPI_MCR0, FSPI_MCR0_MDIS);

	/* Reset the DLL register to default value */
	write_reg(sc, FSPI_DLLACR, FSPI_DLLACR_OVRDEN);
	write_reg(sc, FSPI_DLLBCR, FSPI_DLLBCR_OVRDEN);

	/* enable module */
	write_reg(sc, FSPI_MCR0, FSPI_MCR0_AHB_TIMEOUT(0xFF) |
		    FSPI_MCR0_IP_TIMEOUT(0xFF) | (uint32_t) FSPI_MCR0_OCTCOMB_EN);

	/*
	 * Disable same device enable bit and configure all slave devices
	 * independently.
	 */
	reg = read_reg(sc, FSPI_MCR2);
	reg = reg & ~(FSPI_MCR2_SAMEDEVICEEN);
	write_reg(sc, FSPI_MCR2, reg);

	/* AHB configuration for access buffer 0~7. */
	for (i = 0; i < 7; i++)
		write_reg(sc, FSPI_AHBRX_BUF0CR0 + 4 * i, 0);

	/*
	 * Set ADATSZ with the maximum AHB buffer size to improve the read
	 * performance.
	 */
	write_reg(sc, FSPI_AHBRX_BUF7CR0, (2048 / 8 |
		  FSPI_AHBRXBUF0CR7_PREF));

	/* prefetch and no start address alignment limitation */
	write_reg(sc, FSPI_AHBCR, FSPI_AHBCR_PREF_EN | FSPI_AHBCR_RDADDROPT);

	/* AHB Read - Set lut sequence ID for all CS. */
	flex_spi_prepare_ahb_lut(sc);
	write_reg(sc, FSPI_FLSHA1CR2, AHB_LUT_ID);
	write_reg(sc, FSPI_FLSHA2CR2, AHB_LUT_ID);
	write_reg(sc, FSPI_FLSHB1CR2, AHB_LUT_ID);
	write_reg(sc, FSPI_FLSHB2CR2, AHB_LUT_ID);

	/* disable interrupts */
	write_reg(sc, FSPI_INTEN, 0);

	return (0);
}

static int
flex_spi_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, flex_spi_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "NXP FlexSPI Flash");
	return (BUS_PROBE_SPECIFIC);
}

static int
flex_spi_attach(device_t dev)
{
	struct flex_spi_softc *sc;
	phandle_t node;
	int rid;
	uint32_t reg;

	node = ofw_bus_get_node(dev);
	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->disk_mtx, "flex_spi_DISK", "QSPI disk mtx", MTX_DEF);

	/* Get memory resources. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);

	rid = 1;
	sc->ahb_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE | RF_SHAREABLE);

	if (sc->mem_res == NULL || sc->ahb_mem_res == NULL) {
		device_printf(dev, "could not allocate resources\n");
		flex_spi_detach(dev);
		return (ENOMEM);
	}

	/* Get clocks */
	if ((clk_get_by_ofw_name(dev, node, "fspi_en", &sc->fspi_clk_en) != 0)
	    || (clk_get_freq(sc->fspi_clk_en, &sc->fspi_clk_en_hz) != 0)) {
		device_printf(dev, "could not get fspi_en clock\n");
		flex_spi_detach(dev);
		return (EINVAL);
	}
	if ((clk_get_by_ofw_name(dev, node, "fspi", &sc->fspi_clk) != 0)
	    || (clk_get_freq(sc->fspi_clk, &sc->fspi_clk_hz) != 0)) {
		device_printf(dev, "could not get fspi clock\n");
		flex_spi_detach(dev);
		return (EINVAL);
	}

	/* Enable clocks */
	if (clk_enable(sc->fspi_clk_en) != 0 ||
	    clk_enable(sc->fspi_clk) != 0) {
		device_printf(dev, "could not enable clocks\n");
		flex_spi_detach(dev);
		return (EINVAL);
	}

	/* Clear potential interrupts */
	reg = read_reg(sc, FSPI_INTR);
	if (reg)
		write_reg(sc, FSPI_INTR, reg);

	/* Default setup */
	if (flex_spi_default_setup(sc) != 0) {
		device_printf(sc->dev, "Unable to initialize defaults\n");
		flex_spi_detach(dev);
		return (ENXIO);
	}

	/* Identify attached Flash */
	if(flex_spi_identify(sc) != 0) {
		device_printf(sc->dev, "Unable to identify Flash\n");
		flex_spi_detach(dev);
		return (ENXIO);
	}

	if (flex_spi_clk_setup(sc, sc->fspi_max_clk) != 0) {
		device_printf(sc->dev, "Unable to set up SPI max clock\n");
		flex_spi_detach(dev);
		return (ENXIO);
	}

	sc->buf = malloc(sc->erasesize, SECTOR_BUFFER, M_WAITOK);
	if (sc->buf == NULL) {
		device_printf(sc->dev, "Unable to set up allocate internal buffer\n");
		flex_spi_detach(dev);
		return (ENOMEM);
	}

	/* Move it to per-flash */
	sc->disk = disk_alloc();
	sc->disk->d_open = flex_spi_open;
	sc->disk->d_close = flex_spi_close;
	sc->disk->d_strategy = flex_spi_strategy;
	sc->disk->d_getattr = flex_spi_getattr;
	sc->disk->d_ioctl = flex_spi_ioctl;
	sc->disk->d_name = "flash/qspi";
	sc->disk->d_drv1 = sc;
	/* the most that can fit in a single spi transaction */
	sc->disk->d_maxsize = DFLTPHYS;
	sc->disk->d_sectorsize = FLASH_SECTORSIZE;
	sc->disk->d_unit = device_get_unit(sc->dev);
	sc->disk->d_dump = NULL;

	sc->disk->d_mediasize = sc->sectorsize * sc->sectorcount;
	sc->disk->d_stripesize = sc->erasesize;

	bioq_init(&sc->bio_queue);
	sc->taskstate = TSTATE_RUNNING;
	kproc_create(&flex_spi_task, sc, &sc->p, 0, 0, "task: qspi flash");
	disk_create(sc->disk, DISK_VERSION);

	return (0);
}

static int
flex_spi_detach(device_t dev)
{
	struct flex_spi_softc *sc;
	int err;

	sc = device_get_softc(dev);
	err = 0;

	if (!device_is_attached(dev))
		goto free_resources;

	mtx_lock(&sc->disk_mtx);
	if (sc->taskstate == TSTATE_RUNNING) {
		sc->taskstate = TSTATE_STOPPING;
		wakeup(sc->disk);
		while (err == 0 && sc->taskstate != TSTATE_STOPPED) {
			err = mtx_sleep(sc->disk, &sc->disk_mtx, 0, "flex_spi",
			    hz * 3);
			if (err != 0) {
				sc->taskstate = TSTATE_RUNNING;
				device_printf(sc->dev,
				    "Failed to stop queue task\n");
			}
		}
	}

	mtx_unlock(&sc->disk_mtx);
	mtx_destroy(&sc->disk_mtx);

	if (err == 0 && sc->taskstate == TSTATE_STOPPED) {
		disk_destroy(sc->disk);
		bioq_flush(&sc->bio_queue, NULL, ENXIO);
	}

	/* Disable hardware. */
free_resources:
	/* Release memory resource. */
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem_res), sc->mem_res);

	if (sc->ahb_mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->ahb_mem_res), sc->ahb_mem_res);

	/* Disable clocks */
	if (sc->fspi_clk_en_hz)
		clk_disable(sc->fspi_clk_en);
	if (sc->fspi_clk_hz)
		clk_disable(sc->fspi_clk);

	free(sc->buf, SECTOR_BUFFER);

	return (err);
}

static int
flex_spi_open(struct disk *dp)
{

	return (0);
}

static int
flex_spi_close(struct disk *dp)
{

	return (0);
}

static int
flex_spi_ioctl(struct disk *dp, u_long cmd, void *data, int fflag,
    struct thread *td)
{

	return (ENOTSUP);
}

static void
flex_spi_strategy(struct bio *bp)
{
	struct flex_spi_softc *sc;

	sc = (struct flex_spi_softc *)bp->bio_disk->d_drv1;
	mtx_lock(&sc->disk_mtx);
	bioq_disksort(&sc->bio_queue, bp);
	mtx_unlock(&sc->disk_mtx);
	wakeup(sc->disk);
}

static int
flex_spi_getattr(struct bio *bp)
{
	struct flex_spi_softc *sc;
	device_t dev;

	if (bp->bio_disk == NULL || bp->bio_disk->d_drv1 == NULL) {
		return (ENXIO);
	}

	sc = bp->bio_disk->d_drv1;
	dev = sc->dev;

	if (strcmp(bp->bio_attribute, "SPI::device") != 0) {
		return (-1);
	}

	if (bp->bio_length != sizeof(dev)) {
		return (EFAULT);
	}

	bcopy(&dev, bp->bio_data, sizeof(dev));

	return (0);
}

static void
flex_spi_task(void *arg)
{
	struct flex_spi_softc *sc;
	struct bio *bp;

	sc = (struct flex_spi_softc *)arg;
	for (;;) {
		mtx_lock(&sc->disk_mtx);
		do {
			if (sc->taskstate == TSTATE_STOPPING) {
				sc->taskstate = TSTATE_STOPPED;
				mtx_unlock(&sc->disk_mtx);
				wakeup(sc->disk);
				kproc_exit(0);
			}
			bp = bioq_first(&sc->bio_queue);
			if (bp == NULL)
				mtx_sleep(sc->disk, &sc->disk_mtx, PRIBIO,
				    "flex_spi", 0);
		} while (bp == NULL);
		bioq_remove(&sc->bio_queue, bp);
		mtx_unlock(&sc->disk_mtx);

		switch (bp->bio_cmd) {
		case BIO_READ:
			bp->bio_error = flex_spi_read(sc, bp->bio_offset,
			    bp->bio_data, bp->bio_bcount);
			break;
		case BIO_WRITE:
			bp->bio_error = flex_spi_write(sc, bp->bio_offset,
			    bp->bio_data, bp->bio_bcount);
			break;
		default:
			bp->bio_error = EINVAL;
		}
		biodone(bp);
	}
}

static device_method_t flex_spi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		flex_spi_probe),
	DEVMETHOD(device_attach,	flex_spi_attach),
	DEVMETHOD(device_detach,	flex_spi_detach),

	{ 0, 0 }
};

static driver_t flex_spi_driver = {
	"flex_spi",
	flex_spi_methods,
	sizeof(struct flex_spi_softc),
};

DRIVER_MODULE(flex_spi, simplebus, flex_spi_driver, 0, 0);
SIMPLEBUS_PNP_INFO(flex_spi_compat_data);
