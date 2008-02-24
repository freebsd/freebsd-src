/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
__FBSDID("$FreeBSD: src/sys/dev/flash/at45d.c,v 1.1 2006/11/29 08:05:55 imp Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/gpio.h>
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

struct at45d_softc 
{
	struct intr_config_hook config_intrhook;
	device_t dev;
	struct mtx sc_mtx;
	struct disk *disk;
	struct proc *p;
	struct bio_queue_head bio_queue;
};

#define AT45D_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	AT45D_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define AT45D_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "at45d", MTX_DEF)
#define AT45D_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define AT45D_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define AT45D_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static void at45d_delayed_attach(void *xsc);

/* disk routines */
static int at45d_open(struct disk *dp);
static int at45d_close(struct disk *dp);
static void at45d_strategy(struct bio *bp);
static void at45d_task(void *arg);

#define CONTINUOUS_ARRAY_READ		0xE8
#define CONTINUOUS_ARRAY_READ_HF	0x0B
#define CONTINUOUS_ARRAY_READ_LF	0x03
#define STATUS_REGISTER_READ		0xD7
#define PROGRAM_THROUGH_BUFFER		0x82
#define MANUFACTURER_ID			0x9F

static uint8_t
at45d_get_status(device_t dev)
{
	uint8_t txBuf[8], rxBuf[8];
	struct spi_command cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));
	memset(txBuf, 0, sizeof(txBuf));
	memset(rxBuf, 0, sizeof(rxBuf));

	txBuf[0] = STATUS_REGISTER_READ;
	cmd.tx_cmd = txBuf;
	cmd.rx_cmd = rxBuf;
	cmd.rx_cmd_sz = 2;
	cmd.tx_cmd_sz = 2;
	err = SPIBUS_TRANSFER(device_get_parent(dev), dev, &cmd);
	return (rxBuf[1]);
}

static void
at45d_wait_for_device_ready(device_t dev)
{
	while (!(at45d_get_status(dev) & 0x80))
		continue;
}

static int
at45d_get_mfg_info(device_t dev, uint8_t *resp)
{
	uint8_t txBuf[8], rxBuf[8];
	struct spi_command cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));
	memset(txBuf, 0, sizeof(txBuf));
	memset(rxBuf, 0, sizeof(rxBuf));

	txBuf[0] = MANUFACTURER_ID;
	cmd.tx_cmd = &txBuf;
	cmd.rx_cmd = &rxBuf;
	cmd.tx_cmd_sz = 5;
	cmd.rx_cmd_sz = 5;
	err = SPIBUS_TRANSFER(device_get_parent(dev), dev, &cmd);
	if (err)
		return (err);
	memcpy(resp, rxBuf + 1, 4);
	// XXX We really should 'decode' the reply into some kind of
	// XXX structure.  To be generic (and not just for atmel parts)
	// XXX we'd have to loop until we got a full reply.
	return (0);
}

static int
at45d_probe(device_t dev)
{
	device_set_desc(dev, "AT45 Flash Family");
	return (0);
}

static int
at45d_attach(device_t dev)
{
	struct at45d_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	AT45D_LOCK_INIT(sc);

	/* We'll see what kind of flash we have later... */
	sc->config_intrhook.ich_func = at45d_delayed_attach;
	sc->config_intrhook.ich_arg = sc;
	if (config_intrhook_establish(&sc->config_intrhook) != 0)
		device_printf(dev, "config_intrhook_establish failed\n");
	return (0);
}

static int
at45d_detach(device_t dev)
{
	return EIO;
}

static void
at45d_delayed_attach(void *xsc)
{
	struct at45d_softc *sc = xsc;
	uint8_t buf[4];
	
	at45d_get_mfg_info(sc->dev, buf);
	at45d_wait_for_device_ready(sc->dev);

	sc->disk = disk_alloc();
	sc->disk->d_open = at45d_open;
	sc->disk->d_close = at45d_close;
	sc->disk->d_strategy = at45d_strategy;
	sc->disk->d_name = "flash/spi";
	sc->disk->d_drv1 = sc;
	sc->disk->d_maxsize = DFLTPHYS;
	sc->disk->d_sectorsize = 1056;		/* XXX */
	sc->disk->d_mediasize = 8192 * 1056;	/* XXX */
	sc->disk->d_unit = device_get_unit(sc->dev);
	disk_create(sc->disk, DISK_VERSION);
	bioq_init(&sc->bio_queue);
	kthread_create(&at45d_task, sc, &sc->p, 0, 0, "task: at45d flash");

	config_intrhook_disestablish(&sc->config_intrhook);
}

static int
at45d_open(struct disk *dp)
{
	return 0;
}

static int
at45d_close(struct disk *dp)
{
	return 0;
}

static void
at45d_strategy(struct bio *bp)
{
	struct at45d_softc *sc;

	sc = (struct at45d_softc *)bp->bio_disk->d_drv1;
	AT45D_LOCK(sc);
	bioq_disksort(&sc->bio_queue, bp);
	wakeup(sc);
	AT45D_UNLOCK(sc);
}

static void
at45d_task(void *arg)
{
	struct at45d_softc *sc = (struct at45d_softc*)arg;
	struct bio *bp;
	uint8_t txBuf[8], rxBuf[8];
	struct spi_command cmd;
	int sz;
	daddr_t block, end;
	device_t dev, pdev;
	int err;

	for (;;) {
		dev = sc->dev;
		pdev = device_get_parent(dev);
		AT45D_LOCK(sc);
		do {
			bp = bioq_first(&sc->bio_queue);
			if (bp == NULL)
				msleep(sc, &sc->sc_mtx, PRIBIO, "jobqueue", 0);
		} while (bp == NULL);
		bioq_remove(&sc->bio_queue, bp);
		AT45D_UNLOCK(sc);
		sz = sc->disk->d_sectorsize;
		end = bp->bio_pblkno + (bp->bio_bcount / sz);
		for (block = bp->bio_pblkno; block < end; block++) {
			char *vaddr = bp->bio_data + (block - bp->bio_pblkno) * sz;
			if (bp->bio_cmd == BIO_READ) {
				txBuf[0] = CONTINUOUS_ARRAY_READ_HF;
				cmd.tx_cmd_sz = 5;
				cmd.rx_cmd_sz = 5;
			} else {
				txBuf[0] = PROGRAM_THROUGH_BUFFER;
				cmd.tx_cmd_sz = 4;
				cmd.rx_cmd_sz = 4;
			}
			// XXX only works on certain devices...  Fixme
			txBuf[1] = ((block >> 5) & 0xFF);
			txBuf[2] = ((block << 3) & 0xF8);
			txBuf[3] = 0;
			txBuf[4] = 0;
			cmd.tx_cmd = txBuf;
			cmd.rx_cmd = rxBuf;
			cmd.tx_data = vaddr;
			cmd.tx_data_sz = sz;
			cmd.rx_data = vaddr;
			cmd.rx_data_sz = sz;
			err = SPIBUS_TRANSFER(pdev, dev, &cmd);
			// XXX err check?
		}
		biodone(bp);
	}
}

static devclass_t at45d_devclass;

static device_method_t at45d_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		at45d_probe),
	DEVMETHOD(device_attach,	at45d_attach),
	DEVMETHOD(device_detach,	at45d_detach),

	{ 0, 0 }
};

static driver_t at45d_driver = {
	"at45d",
	at45d_methods,
	sizeof(struct at45d_softc),
};

DRIVER_MODULE(at45d, spibus, at45d_driver, at45d_devclass, 0, 0);
