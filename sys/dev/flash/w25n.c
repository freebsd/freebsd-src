/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Adrian Chadd <adrian@FreeBSD.org>.
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
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <geom/geom_disk.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>
#endif

#include <dev/spibus/spi.h>
#include "spibus_if.h"

#include <dev/flash/w25nreg.h>

#define	W25N_SECTORSIZE	512

struct w25n_flash_ident
{
	const char	*name;
	uint8_t		manufacturer_id;
	uint16_t	device_id;
	unsigned int	sectorsize;
	unsigned int	sectorcount;
	unsigned int	erasesize;
	unsigned int	flags;
};

struct w25n_softc
{
	device_t	sc_dev;
	device_t	sc_parent;
	uint8_t		sc_manufacturer_id;
	uint16_t	sc_device_id;
	unsigned int	sc_erasesize;
	struct mtx	sc_mtx;
	struct disk	*sc_disk;
	struct proc	*sc_p;
	struct bio_queue_head sc_bio_queue;
	unsigned int	sc_flags;
	unsigned int	sc_taskstate;
};

#define	TSTATE_STOPPED		0
#define	TSTATE_STOPPING		1
#define	TSTATE_RUNNING		2

#define W25N_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	W25N_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define W25N_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	    "w25n", MTX_DEF)
#define W25N_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define W25N_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define W25N_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

/* disk routines */
static int w25n_open(struct disk *dp);
static int w25n_close(struct disk *dp);
static int w25n_ioctl(struct disk *, u_long, void *, int, struct thread *);
static void w25n_strategy(struct bio *bp);
static int w25n_getattr(struct bio *bp);
static void w25n_task(void *arg);

#define	FL_NONE		0x00000000

static struct w25n_flash_ident flash_devices[] = {

	{ "w25n01gv",	0xef, 0xaa21, 2048, 64 * 1024, 128 * 1024, FL_NONE },
};

static int
w25n_read_status_register(struct w25n_softc *sc, uint8_t reg,
    uint8_t *retval)
{
	uint8_t txBuf[3], rxBuf[3];
	struct spi_command cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));

	txBuf[0] = CMD_READ_STATUS;
	txBuf[1] = reg;
	cmd.tx_cmd = txBuf;
	cmd.rx_cmd = rxBuf;
	cmd.rx_cmd_sz = 3;
	cmd.tx_cmd_sz = 3;
	err = SPIBUS_TRANSFER(sc->sc_parent, sc->sc_dev, &cmd);
	if (err != 0)
		return (err);
	*retval = rxBuf[2];
	return (0);
}

static int
w25n_wait_for_device_ready(struct w25n_softc *sc)
{
	int err;
	uint8_t val;

	do {
		err = w25n_read_status_register(sc, STATUS_REG_3, &val);
	} while (err == 0 && (val & STATUS_REG_3_BUSY));

	return (err);
}

static int
w25n_set_page_address(struct w25n_softc *sc, uint16_t page_idx)
{
	uint8_t txBuf[4], rxBuf[4];
	struct spi_command cmd;
	int err;

	txBuf[0] = CMD_PAGE_DATA_READ;
	txBuf[1] = 0; /* dummy */
	txBuf[2] = (page_idx >> 8) & 0xff;
	txBuf[3] = (page_idx >> 0) & 0xff;
	cmd.tx_cmd = txBuf;
	cmd.rx_cmd = rxBuf;
	cmd.rx_cmd_sz = 4;
	cmd.tx_cmd_sz = 4;
	err = SPIBUS_TRANSFER(sc->sc_parent, sc->sc_dev, &cmd);
	if (err != 0)
		return (err);
	return (0);
}

static struct w25n_flash_ident*
w25n_get_device_ident(struct w25n_softc *sc)
{
	uint8_t txBuf[8], rxBuf[8];
	struct spi_command cmd;
	uint8_t manufacturer_id;
	uint16_t dev_id;
	int err, i;

	memset(&cmd, 0, sizeof(cmd));
	memset(txBuf, 0, sizeof(txBuf));
	memset(rxBuf, 0, sizeof(rxBuf));

	txBuf[0] = CMD_READ_IDENT;
	cmd.tx_cmd = &txBuf;
	cmd.rx_cmd = &rxBuf;

	cmd.tx_cmd_sz = 5;
	cmd.rx_cmd_sz = 5;
	err = SPIBUS_TRANSFER(sc->sc_parent, sc->sc_dev, &cmd);
	if (err)
		return (NULL);

	manufacturer_id = rxBuf[2];
	dev_id = (rxBuf[3] << 8) | (rxBuf[4]);

	for (i = 0; i < nitems(flash_devices); i++) {
		if ((flash_devices[i].manufacturer_id == manufacturer_id) &&
		    (flash_devices[i].device_id == dev_id))
			return &flash_devices[i];
	}

	device_printf(sc->sc_dev,
	    "Unknown SPI NAND flash device. Vendor: %02x, device id: %04x\n",
	    manufacturer_id, dev_id);
	return (NULL);
}

static int
w25n_write(struct w25n_softc *sc, off_t offset, caddr_t data, off_t count)
{

	return (ENXIO);

}

static int
w25n_read(struct w25n_softc *sc, off_t offset, caddr_t data, off_t count)
{
	uint8_t txBuf[4], rxBuf[4];
	struct spi_command cmd;
	int err;
	int read_size;
	uint16_t page_idx;
	uint8_t st3, ecc_status;

	/*
	 * We only support reading things at multiples of the page size.
	 */
	if (count % sc->sc_disk->d_sectorsize != 0) {
		device_printf(sc->sc_dev, "%s: invalid count\n", __func__);
		return (EIO);
	}
	if (offset % sc->sc_disk->d_sectorsize != 0) {
		device_printf(sc->sc_dev, "%s: invalid offset\n", __func__);
		return (EIO);
	}

	page_idx = offset / sc->sc_disk->d_sectorsize;

	while (count > 0) {
		/* Wait until we're ready */
		err = w25n_wait_for_device_ready(sc);
		if (err != 0) {
			device_printf(sc->sc_dev, "%s: failed to wait\n",
			    __func__);
			return (err);
		}

		/* Issue the page change */
		err = w25n_set_page_address(sc, page_idx);
		if (err != 0) {
			device_printf(sc->sc_dev, "%s: page change failed\n",
			    __func__);
			return (err);
		}

		/* Wait until the page change has read in data */
		err = w25n_wait_for_device_ready(sc);
		if (err != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed to wait again\n",
			    __func__);
			return (err);
		}

		/*
		 * Now we can issue a read command for the data
		 * in the buffer.  We'll read into the data buffer
		 * until we run out of data in this page.
		 *
		 * To simplify things we're not starting at an
		 * arbitrary offset; so the column address here
		 * inside the page is 0.  If we later want to support
		 * that kind of operation then we could do the math
		 * here.
		 */
		read_size = MIN(count, sc->sc_disk->d_sectorsize);

		memset(data, 0xef, read_size);

		txBuf[0] = CMD_FAST_READ;
		txBuf[1] = 0; /* column address 15:8 */
		txBuf[2] = 0; /* column address 7:0 */
		txBuf[3] = 0; /* dummy byte */
		cmd.tx_cmd_sz = 4;
		cmd.rx_cmd_sz = 4;
		cmd.tx_cmd = txBuf;
		cmd.rx_cmd = rxBuf;

		cmd.tx_data = data;
		cmd.rx_data = data;
		cmd.tx_data_sz = read_size;
		cmd.rx_data_sz = read_size;

		err = SPIBUS_TRANSFER(sc->sc_parent, sc->sc_dev, &cmd);
		if (err != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: failed to do FAST_READ (%u)\n",
			    err);
			return (err);
		}

		/*
		 * Now, check ECC status bits, see if we had an ECC
		 * error.
		 */
		err = w25n_read_status_register(sc, STATUS_REG_3, &st3);
		if (err != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed to wait again\n", __func__);
			return (err);
		}
		ecc_status = (st3 >> STATUS_REG_3_ECC_STATUS_SHIFT)
		    & STATUS_REG_3_ECC_STATUS_MASK;
		if ((ecc_status != STATUS_ECC_OK)
		    && (ecc_status != STATUS_ECC_1BIT_OK)) {
			device_printf(sc->sc_dev,
			    "%s: ECC status failed\n", __func__);
			return (EIO);
		}

		count -= read_size;
		data += read_size;
		page_idx += 1;
	}

	return (0);
}

#ifdef	FDT
static struct ofw_compat_data compat_data[] = {
	{ "spi-nand",		1 },
	{ NULL,			0 },
};
#endif

static int
w25n_probe(device_t dev)
{
#ifdef FDT
	int i;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	/* First try to match the compatible property to the compat_data */
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 1)
		goto found;

	/*
	 * Next, try to find a compatible device using the names in the
	 * flash_devices structure
	 */
	for (i = 0; i < nitems(flash_devices); i++)
		if (ofw_bus_is_compatible(dev, flash_devices[i].name))
			goto found;

	return (ENXIO);
found:
#endif
	device_set_desc(dev, "W25N NAND Flash Family");

	return (0);
}

static int
w25n_attach(device_t dev)
{
	struct w25n_softc *sc;
	struct w25n_flash_ident *ident;
	int err;
	uint8_t st1, st2, st3;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_parent = device_get_parent(sc->sc_dev);

	W25N_LOCK_INIT(sc);

	ident = w25n_get_device_ident(sc);
	if (ident == NULL)
		return (ENXIO);

	if ((err = w25n_wait_for_device_ready(sc)) != 0)
		return (err);

	/*
	 * Read the configuration, protection and status registers.
	 * Print them out here so the initial configuration can be checked.
	 */
	err = w25n_read_status_register(sc, STATUS_REG_1, &st1);
	if (err != 0)
		return (err);
	err = w25n_read_status_register(sc, STATUS_REG_2, &st2);
	if (err != 0)
		return (err);
	err = w25n_read_status_register(sc, STATUS_REG_3, &st3);
	if (err != 0)
		return (err);

	device_printf(sc->sc_dev,
	    "device type %s, size %dK in %d sectors of %dK, erase size %dK\n",
	    ident->name,
	    ident->sectorcount * ident->sectorsize / 1024,
	    ident->sectorcount, ident->sectorsize / 1024,
	    ident->erasesize / 1024);

	if (bootverbose)
		device_printf(sc->sc_dev,
		    "status1=0x%08x, status2=0x%08x, status3=0x%08x\n",
		    st1, st2, st3);

	/*
	 * For now we're only going to support parts that have
	 * device ECC enabled.  Later on it may be interesting
	 * to do software driven ECC and figure out how we
	 * expose it over GEOM, but that day isn't today.
	 */
	if ((st2 & STATUS_REG_2_ECC_EN) == 0) {
		device_printf(sc->sc_dev,
		    "ERROR: only ECC in HW is supported\n");
		return (err);
	}
	if ((st2 & STATUS_REG_2_BUF_EN) == 0) {
		device_printf(sc->sc_dev,
		    "ERROR: only BUF mode is supported\n");
		return (err);
	}

	sc->sc_flags = ident->flags;
	sc->sc_erasesize = ident->erasesize;

	sc->sc_disk = disk_alloc();
	sc->sc_disk->d_open = w25n_open;
	sc->sc_disk->d_close = w25n_close;
	sc->sc_disk->d_strategy = w25n_strategy;
	sc->sc_disk->d_getattr = w25n_getattr;
	sc->sc_disk->d_ioctl = w25n_ioctl;
	sc->sc_disk->d_name = "nand_flash/spi";
	sc->sc_disk->d_drv1 = sc;
	sc->sc_disk->d_maxsize = DFLTPHYS;
	sc->sc_disk->d_sectorsize = ident->sectorsize;
	sc->sc_disk->d_mediasize = ident->sectorsize * ident->sectorcount;
	sc->sc_disk->d_stripesize = sc->sc_erasesize;
	sc->sc_disk->d_unit = device_get_unit(sc->sc_dev);
	sc->sc_disk->d_dump = NULL;		/* NB: no dumps */
	strlcpy(sc->sc_disk->d_descr, ident->name,
	    sizeof(sc->sc_disk->d_descr));

	disk_create(sc->sc_disk, DISK_VERSION);
	bioq_init(&sc->sc_bio_queue);
	kproc_create(&w25n_task, sc, &sc->sc_p, 0, 0, "task: w25n flash");
	sc->sc_taskstate = TSTATE_RUNNING;

	return (0);
}

static int
w25n_detach(device_t dev)
{
	struct w25n_softc *sc;
	int err;

	sc = device_get_softc(dev);
	err = 0;

	W25N_LOCK(sc);
	if (sc->sc_taskstate == TSTATE_RUNNING) {
		sc->sc_taskstate = TSTATE_STOPPING;
		wakeup(sc);
		while (err == 0 && sc->sc_taskstate != TSTATE_STOPPED) {
			err = msleep(sc, &sc->sc_mtx, 0, "w25nd", hz * 3);
			if (err != 0) {
				sc->sc_taskstate = TSTATE_RUNNING;
				device_printf(sc->sc_dev,
				    "Failed to stop queue task\n");
			}
		}
	}
	W25N_UNLOCK(sc);

	if (err == 0 && sc->sc_taskstate == TSTATE_STOPPED) {
		disk_destroy(sc->sc_disk);
		bioq_flush(&sc->sc_bio_queue, NULL, ENXIO);
		W25N_LOCK_DESTROY(sc);
	}
	return (err);
}

static int
w25n_open(struct disk *dp)
{
	return (0);
}

static int
w25n_close(struct disk *dp)
{

	return (0);
}

static int
w25n_ioctl(struct disk *dp, u_long cmd, void *data, int fflag,
	struct thread *td)
{

	return (EINVAL);
}

static void
w25n_strategy(struct bio *bp)
{
	struct w25n_softc *sc;

	sc = (struct w25n_softc *)bp->bio_disk->d_drv1;
	W25N_LOCK(sc);
	bioq_disksort(&sc->sc_bio_queue, bp);
	wakeup(sc);
	W25N_UNLOCK(sc);
}

static int
w25n_getattr(struct bio *bp)
{
	struct w25n_softc *sc;
	device_t dev;

	if (bp->bio_disk == NULL || bp->bio_disk->d_drv1 == NULL)
		return (ENXIO);

	sc = bp->bio_disk->d_drv1;
	dev = sc->sc_dev;

	if (strcmp(bp->bio_attribute, "SPI::device") == 0) {
		if (bp->bio_length != sizeof(dev))
			return (EFAULT);
		bcopy(&dev, bp->bio_data, sizeof(dev));
	} else
		return (-1);
	return (0);
}

static void
w25n_task(void *arg)
{
	struct w25n_softc *sc = (struct w25n_softc*)arg;
	struct bio *bp;

	for (;;) {
		W25N_LOCK(sc);
		do {
			if (sc->sc_taskstate == TSTATE_STOPPING) {
				sc->sc_taskstate = TSTATE_STOPPED;
				W25N_UNLOCK(sc);
				wakeup(sc);
				kproc_exit(0);
			}
			bp = bioq_first(&sc->sc_bio_queue);
			if (bp == NULL)
				msleep(sc, &sc->sc_mtx, PRIBIO, "w25nq", 0);
		} while (bp == NULL);
		bioq_remove(&sc->sc_bio_queue, bp);
		W25N_UNLOCK(sc);

		switch (bp->bio_cmd) {
		case BIO_READ:
			bp->bio_error = w25n_read(sc, bp->bio_offset,
			    bp->bio_data, bp->bio_bcount);
			break;
		case BIO_WRITE:
			bp->bio_error = w25n_write(sc, bp->bio_offset,
			    bp->bio_data, bp->bio_bcount);
			break;
		default:
			bp->bio_error = EOPNOTSUPP;
		}


		biodone(bp);
	}
}

static device_method_t w25n_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		w25n_probe),
	DEVMETHOD(device_attach,	w25n_attach),
	DEVMETHOD(device_detach,	w25n_detach),

	{ 0, 0 }
};

static driver_t w25n_driver = {
	"w25n",
	w25n_methods,
	sizeof(struct w25n_softc),
};

DRIVER_MODULE(w25n, spibus, w25n_driver, 0, 0);
MODULE_DEPEND(w25n, spibus, 1, 1, 1);
#ifdef	FDT
MODULE_DEPEND(w25n, fdt_slicer, 1, 1, 1);
SPIBUS_FDT_PNP_INFO(compat_data);
#endif
