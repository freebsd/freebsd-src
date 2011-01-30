/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 * Copyright (c) 2009 Oleksandr Tymoshenko.  All rights reserved.
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
__FBSDID("$FreeBSD$");

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

#include <dev/spibus/spi.h>
#include "spibus_if.h"

#include <dev/flash/mx25lreg.h>

#define	FL_NONE			0x00
#define	FL_ERASE_4K		0x01
#define	FL_ERASE_32K		0x02

struct mx25l_flash_ident
{
	const char	*name;
	uint8_t		manufacturer_id;
	uint16_t	device_id;
	unsigned int	sectorsize;
	unsigned int	sectorcount;
	unsigned int	flags;
};

struct mx25l_softc 
{
	device_t	sc_dev;
	uint8_t		sc_manufacturer_id;
	uint16_t	sc_device_id;
	unsigned int	sc_sectorsize;
	struct mtx	sc_mtx;
	struct disk	*sc_disk;
	struct proc	*sc_p;
	struct bio_queue_head sc_bio_queue;
	unsigned int	sc_flags;
};

#define M25PXX_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	M25PXX_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define M25PXX_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	    "mx25l", MTX_DEF)
#define M25PXX_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define M25PXX_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define M25PXX_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

/* disk routines */
static int mx25l_open(struct disk *dp);
static int mx25l_close(struct disk *dp);
static int mx25l_ioctl(struct disk *, u_long, void *, int, struct thread *);
static void mx25l_strategy(struct bio *bp);
static void mx25l_task(void *arg);

struct mx25l_flash_ident flash_devices[] = {
	{ "mx25ll32",  0xc2, 0x2016, 64 * 1024,  64, FL_NONE },
	{ "m25p64",    0x20, 0x2017, 64 * 1024, 128, FL_NONE },
	{ "mx25ll64",  0xc2, 0x2017, 64 * 1024, 128, FL_NONE },
	{ "mx25ll128", 0xc2, 0x2018, 64 * 1024, 256, FL_ERASE_4K | FL_ERASE_32K },
	{ "s25fl128",  0x01, 0x2018, 64 * 1024, 256, FL_NONE },
	{ "s25sl064a", 0x01, 0x0216, 64 * 1024, 128, FL_NONE },
};

static uint8_t
mx25l_get_status(device_t dev)
{
	uint8_t txBuf[2], rxBuf[2];
	struct spi_command cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));
	memset(txBuf, 0, sizeof(txBuf));
	memset(rxBuf, 0, sizeof(rxBuf));

	txBuf[0] = CMD_READ_STATUS;
	cmd.tx_cmd = txBuf;
	cmd.rx_cmd = rxBuf;
	cmd.rx_cmd_sz = 2;
	cmd.tx_cmd_sz = 2;
	err = SPIBUS_TRANSFER(device_get_parent(dev), dev, &cmd);
	return (rxBuf[1]);
}

static void
mx25l_wait_for_device_ready(device_t dev)
{
	while ((mx25l_get_status(dev) & STATUS_WIP))
		continue;
}

static struct mx25l_flash_ident*
mx25l_get_device_ident(struct mx25l_softc *sc)
{
	device_t dev = sc->sc_dev;
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
	/*
	 * Some compatible devices has extended two-bytes ID
	 * We'll use only manufacturer/deviceid atm
	 */
	cmd.tx_cmd_sz = 4;
	cmd.rx_cmd_sz = 4;
	err = SPIBUS_TRANSFER(device_get_parent(dev), dev, &cmd);
	if (err)
		return (NULL);

	manufacturer_id = rxBuf[1];
	dev_id = (rxBuf[2] << 8) | (rxBuf[3]);

	for (i = 0; 
	    i < sizeof(flash_devices)/sizeof(struct mx25l_flash_ident); i++) {
		if ((flash_devices[i].manufacturer_id == manufacturer_id) &&
		    (flash_devices[i].device_id == dev_id))
			return &flash_devices[i];
	}

	printf("Unknown SPI flash device. Vendor: %02x, device id: %04x\n",
	    manufacturer_id, dev_id);
	return (NULL);
}

static void
mx25l_set_writable(device_t dev, int writable)
{
	uint8_t txBuf[1], rxBuf[1];
	struct spi_command cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));
	memset(txBuf, 0, sizeof(txBuf));
	memset(rxBuf, 0, sizeof(rxBuf));

	txBuf[0] = writable ? CMD_WRITE_ENABLE : CMD_WRITE_DISABLE;
	cmd.tx_cmd = txBuf;
	cmd.rx_cmd = rxBuf;
	cmd.rx_cmd_sz = 1;
	cmd.tx_cmd_sz = 1;
	err = SPIBUS_TRANSFER(device_get_parent(dev), dev, &cmd);
}

static void
mx25l_erase_cmd(device_t dev, off_t sector, uint8_t ecmd)
{
	uint8_t txBuf[4], rxBuf[4];
	struct spi_command cmd;
	int err;

	mx25l_wait_for_device_ready(dev);
	mx25l_set_writable(dev, 1);

	memset(&cmd, 0, sizeof(cmd));
	memset(txBuf, 0, sizeof(txBuf));
	memset(rxBuf, 0, sizeof(rxBuf));

	txBuf[0] = ecmd;
	cmd.tx_cmd = txBuf;
	cmd.rx_cmd = rxBuf;
	cmd.rx_cmd_sz = 4;
	cmd.tx_cmd_sz = 4;
	txBuf[1] = ((sector >> 16) & 0xff);
	txBuf[2] = ((sector >> 8) & 0xff);
	txBuf[3] = (sector & 0xff);
	err = SPIBUS_TRANSFER(device_get_parent(dev), dev, &cmd);
}

static int
mx25l_write(device_t dev, off_t offset, caddr_t data, off_t count)
{
	struct mx25l_softc *sc;
	uint8_t txBuf[8], rxBuf[8];
	struct spi_command cmd;
	off_t write_offset;
	long bytes_to_write, bytes_writen;
	device_t pdev;
	int err = 0;

	pdev = device_get_parent(dev);
	sc = device_get_softc(dev);

	cmd.tx_cmd_sz = 4;
	cmd.rx_cmd_sz = 4;

	bytes_writen = 0;
	write_offset = offset;

	/*
	 * Sanity checks
	 */
	KASSERT(count % sc->sc_sectorsize == 0,
	    ("count for BIO_WRITE is not sector size (%d bytes) aligned",
		sc->sc_sectorsize));

	KASSERT(offset % sc->sc_sectorsize == 0,
	    ("offset for BIO_WRITE is not sector size (%d bytes) aligned",
		sc->sc_sectorsize));

	/*
	 * Assume here that we write per-sector only 
	 * and sector size should be 256 bytes aligned
	 */
	KASSERT(write_offset % FLASH_PAGE_SIZE == 0,
	    ("offset for BIO_WRITE is not page size (%d bytes) aligned",
		FLASH_PAGE_SIZE));

	/*
	 * Maximum write size for CMD_PAGE_PROGRAM is 
	 * FLASH_PAGE_SIZE, so split data to chunks 
	 * FLASH_PAGE_SIZE bytes eash and write them
	 * one by one
	 */
	while (bytes_writen < count) {
		/*
		 * If we crossed sector boundary - erase next sector
		 */
		if (((offset + bytes_writen) % sc->sc_sectorsize) == 0)
			mx25l_erase_cmd(dev, offset + bytes_writen, CMD_SECTOR_ERASE);

		txBuf[0] = CMD_PAGE_PROGRAM;
		txBuf[1] = ((write_offset >> 16) & 0xff);
		txBuf[2] = ((write_offset >> 8) & 0xff);
		txBuf[3] = (write_offset & 0xff);

		bytes_to_write = MIN(FLASH_PAGE_SIZE,
		    count - bytes_writen);
		cmd.tx_cmd = txBuf;
		cmd.rx_cmd = rxBuf;
		cmd.tx_data = data + bytes_writen;
		cmd.tx_data_sz = bytes_to_write;
		cmd.rx_data = data + bytes_writen;
		cmd.rx_data_sz = bytes_to_write;

		/*
		 * Eash completed write operation resets WEL 
		 * (write enable latch) to disabled state,
		 * so we re-enable it here 
		 */
		mx25l_wait_for_device_ready(dev);
		mx25l_set_writable(dev, 1);

		err = SPIBUS_TRANSFER(pdev, dev, &cmd);
		if (err)
			break;

		bytes_writen += bytes_to_write;
		write_offset += bytes_to_write;
	}

	return (err);
}

static int
mx25l_read(device_t dev, off_t offset, caddr_t data, off_t count)
{
	struct mx25l_softc *sc;
	uint8_t txBuf[8], rxBuf[8];
	struct spi_command cmd;
	device_t pdev;
	int err = 0;

	pdev = device_get_parent(dev);
	sc = device_get_softc(dev);

	/*
	 * Sanity checks
	 */
	KASSERT(count % sc->sc_sectorsize == 0,
	    ("count for BIO_READ is not sector size (%d bytes) aligned",
		sc->sc_sectorsize));

	KASSERT(offset % sc->sc_sectorsize == 0,
	    ("offset for BIO_READ is not sector size (%d bytes) aligned",
		sc->sc_sectorsize));

	txBuf[0] = CMD_FAST_READ;
	cmd.tx_cmd_sz = 5;
	cmd.rx_cmd_sz = 5;

	txBuf[1] = ((offset >> 16) & 0xff);
	txBuf[2] = ((offset >> 8) & 0xff);
	txBuf[3] = (offset & 0xff);
	/* Dummy byte */
	txBuf[4] = 0;

	cmd.tx_cmd = txBuf;
	cmd.rx_cmd = rxBuf;
	cmd.tx_data = data;
	cmd.tx_data_sz = count;
	cmd.rx_data = data;
	cmd.rx_data_sz = count;

	err = SPIBUS_TRANSFER(pdev, dev, &cmd);

	return (err);
}

static int
mx25l_probe(device_t dev)
{
	device_set_desc(dev, "M25Pxx Flash Family");
	return (0);
}

static int
mx25l_attach(device_t dev)
{
	struct mx25l_softc *sc;
	struct mx25l_flash_ident *ident;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	M25PXX_LOCK_INIT(sc);

	ident = mx25l_get_device_ident(sc);
	if (ident == NULL)
		return (ENXIO);

	mx25l_wait_for_device_ready(sc->sc_dev);

	sc->sc_disk = disk_alloc();
	sc->sc_disk->d_open = mx25l_open;
	sc->sc_disk->d_close = mx25l_close;
	sc->sc_disk->d_strategy = mx25l_strategy;
	sc->sc_disk->d_ioctl = mx25l_ioctl;
	sc->sc_disk->d_name = "flash/spi";
	sc->sc_disk->d_drv1 = sc;
	sc->sc_disk->d_maxsize = DFLTPHYS;
	sc->sc_disk->d_sectorsize = ident->sectorsize;
	sc->sc_disk->d_mediasize = ident->sectorsize * ident->sectorcount;
	sc->sc_disk->d_unit = device_get_unit(sc->sc_dev);
	sc->sc_disk->d_dump = NULL;		/* NB: no dumps */
	/* Sectorsize for erase operations */
	sc->sc_sectorsize =  ident->sectorsize;
	sc->sc_flags = ident->flags;

        /* NB: use stripesize to hold the erase/region size for RedBoot */
	sc->sc_disk->d_stripesize = ident->sectorsize;

	disk_create(sc->sc_disk, DISK_VERSION);
	bioq_init(&sc->sc_bio_queue);

	kproc_create(&mx25l_task, sc, &sc->sc_p, 0, 0, "task: mx25l flash");
	device_printf(sc->sc_dev, "%s, sector %d bytes, %d sectors\n", 
	    ident->name, ident->sectorsize, ident->sectorcount);

	return (0);
}

static int
mx25l_detach(device_t dev)
{

	return (EIO);
}

static int
mx25l_open(struct disk *dp)
{
	return (0);
}

static int
mx25l_close(struct disk *dp)
{

	return (0);
}

static int
mx25l_ioctl(struct disk *dp, u_long cmd, void *data, int fflag,
	struct thread *td)
{

	return (EINVAL);
}

static void
mx25l_strategy(struct bio *bp)
{
	struct mx25l_softc *sc;

	sc = (struct mx25l_softc *)bp->bio_disk->d_drv1;
	M25PXX_LOCK(sc);
	bioq_disksort(&sc->sc_bio_queue, bp);
	wakeup(sc);
	M25PXX_UNLOCK(sc);
}

static void
mx25l_task(void *arg)
{
	struct mx25l_softc *sc = (struct mx25l_softc*)arg;
	struct bio *bp;
	device_t dev;

	for (;;) {
		dev = sc->sc_dev;
		M25PXX_LOCK(sc);
		do {
			bp = bioq_first(&sc->sc_bio_queue);
			if (bp == NULL)
				msleep(sc, &sc->sc_mtx, PRIBIO, "jobqueue", 0);
		} while (bp == NULL);
		bioq_remove(&sc->sc_bio_queue, bp);
		M25PXX_UNLOCK(sc);

		switch (bp->bio_cmd) {
		case BIO_READ:
			bp->bio_error = mx25l_read(dev, bp->bio_offset, 
			    bp->bio_data, bp->bio_bcount);
			break;
		case BIO_WRITE:
			bp->bio_error = mx25l_write(dev, bp->bio_offset, 
			    bp->bio_data, bp->bio_bcount);
			break;
		default:
			bp->bio_error = EINVAL;
		}


		biodone(bp);
	}
}

static devclass_t mx25l_devclass;

static device_method_t mx25l_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mx25l_probe),
	DEVMETHOD(device_attach,	mx25l_attach),
	DEVMETHOD(device_detach,	mx25l_detach),

	{ 0, 0 }
};

static driver_t mx25l_driver = {
	"mx25l",
	mx25l_methods,
	sizeof(struct mx25l_softc),
};

DRIVER_MODULE(mx25l, spibus, mx25l_driver, mx25l_devclass, 0, 0);
