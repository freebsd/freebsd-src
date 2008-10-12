/*-
 * Copyright (c) 2006 Bernd Walter.  All rights reserved.
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
 *
 * Portions of this software may have been developed with reference to
 * the SD Simplified Specification.  The following disclaimer may apply:
 *
 * The following conditions apply to the release of the simplified
 * specification ("Simplified Specification") by the SD Card Association and
 * the SD Group. The Simplified Specification is a subset of the complete SD
 * Specification which is owned by the SD Card Association and the SD
 * Group. This Simplified Specification is provided on a non-confidential
 * basis subject to the disclaimers below. Any implementation of the
 * Simplified Specification may require a license from the SD Card
 * Association, SD Group, SD-3C LLC or other third parties.
 *
 * Disclaimers:
 *
 * The information contained in the Simplified Specification is presented only
 * as a standard specification for SD Cards and SD Host/Ancillary products and
 * is provided "AS-IS" without any representations or warranties of any
 * kind. No responsibility is assumed by the SD Group, SD-3C LLC or the SD
 * Card Association for any damages, any infringements of patents or other
 * right of the SD Group, SD-3C LLC, the SD Card Association or any third
 * parties, which may result from its use. No license is granted by
 * implication, estoppel or otherwise under any patent or other rights of the
 * SD Group, SD-3C LLC, the SD Card Association or any third party. Nothing
 * herein shall be construed as an obligation by the SD Group, the SD-3C LLC
 * or the SD Card Association to disclose or distribute any technical
 * information, know-how or other confidential information to any third party.
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
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <geom/geom_disk.h>

#include <dev/mmc/mmcvar.h>
#include <dev/mmc/mmcreg.h>

#include "mmcbus_if.h"

struct mmcsd_softc {
	device_t dev;
	struct mtx sc_mtx;
	struct disk *disk;
	struct proc *p;
	struct bio_queue_head bio_queue;
	int running;
};

#define	MULTI_BLOCK_BROKEN

/* bus entry points */
static int mmcsd_probe(device_t dev);
static int mmcsd_attach(device_t dev);
static int mmcsd_detach(device_t dev);

/* disk routines */
static int mmcsd_open(struct disk *dp);
static int mmcsd_close(struct disk *dp);
static void mmcsd_strategy(struct bio *bp);
static void mmcsd_task(void *arg);

static const char *mmcsd_card_name(device_t dev);
static int mmcsd_bus_bit_width(device_t dev);

#define MMCSD_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	MMCSD_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)
#define MMCSD_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "mmcsd", MTX_DEF)
#define MMCSD_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define MMCSD_ASSERT_LOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define MMCSD_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static int
mmcsd_probe(device_t dev)
{

	device_quiet(dev);
	device_set_desc(dev, "MMC/SD Memory Card");
	return (0);
}

static int
mmcsd_attach(device_t dev)
{
	struct mmcsd_softc *sc;
	struct disk *d;
	intmax_t mb;
	char unit;

	sc = device_get_softc(dev);
	sc->dev = dev;
	MMCSD_LOCK_INIT(sc);

	d = sc->disk = disk_alloc();
	d->d_open = mmcsd_open;
	d->d_close = mmcsd_close;
	d->d_strategy = mmcsd_strategy;
	// d->d_dump = mmcsd_dump;	Need polling mmc layer
	d->d_name = "mmcsd";
	d->d_drv1 = sc;
	d->d_maxsize = MAXPHYS;		/* Maybe ask bridge? */
	d->d_sectorsize = mmc_get_sector_size(dev);
	d->d_mediasize = mmc_get_media_size(dev) * d->d_sectorsize;
	d->d_unit = device_get_unit(dev);
	/*
	 * Display in most natural units.  There's no cards < 1MB.
	 * The SD standard goes to 2GiB, but the data format supports
	 * up to 4GiB and some card makers push it up to this limit.
	 * The SDHC standard only goes to 32GiB (the data format in
	 * SDHC is good to 2TiB however, which isn't too ugly at
	 * 2048GiBm, so we note it in passing here and don't add the
	 * code to print TiB).
	 */
	mb = d->d_mediasize >> 20;	/* 1MiB == 1 << 20 */
	unit = 'M';
	if (mb >= 10240) {		/* 1GiB = 1024 MiB */
		unit = 'G';
		mb /= 1024;
	}
	device_printf(dev, "%ju%cB <%s Memory Card>%s at %s %dMHz/%dbit\n",
	    mb, unit, mmcsd_card_name(dev),
	    mmc_get_read_only(dev) ? " (read-only)" : "",
	    device_get_nameunit(device_get_parent(dev)),
	    mmc_get_tran_speed(dev) / 1000000, mmcsd_bus_bit_width(dev));
	disk_create(d, DISK_VERSION);
	bioq_init(&sc->bio_queue);

	sc->running = 1;
	kproc_create(&mmcsd_task, sc, &sc->p, 0, 0, "task: mmc/sd card");

	return (0);
}

static int
mmcsd_detach(device_t dev)
{
	struct mmcsd_softc *sc = device_get_softc(dev);

	/* kill thread */
	MMCSD_LOCK(sc);
	sc->running = 0;
	wakeup(sc);
	MMCSD_UNLOCK(sc);

	/* wait for thread to finish.  XXX probably want timeout.  -sorbo */
	MMCSD_LOCK(sc);
	while (sc->running != -1)
		msleep(sc, &sc->sc_mtx, PRIBIO, "detach", 0);
	MMCSD_UNLOCK(sc);

	/* kill disk */
	disk_destroy(sc->disk);
	/* XXX destroy anything in queue */

	MMCSD_LOCK_DESTROY(sc);

	return (0);
}

static int
mmcsd_open(struct disk *dp)
{
	return (0);
}

static int
mmcsd_close(struct disk *dp)
{
	return (0);
}

static void
mmcsd_strategy(struct bio *bp)
{
	struct mmcsd_softc *sc;

	sc = (struct mmcsd_softc *)bp->bio_disk->d_drv1;
	MMCSD_LOCK(sc);
	bioq_disksort(&sc->bio_queue, bp);
	wakeup(sc);
	MMCSD_UNLOCK(sc);
}

static void
mmcsd_task(void *arg)
{
	struct mmcsd_softc *sc = (struct mmcsd_softc*)arg;
	struct bio *bp;
	int sz;
	daddr_t block, end;
	struct mmc_command cmd;
	struct mmc_command stop;
	struct mmc_request req;
	struct mmc_data data;
	device_t dev;

	dev = sc->dev;
	while (sc->running) {
		MMCSD_LOCK(sc);
		do {
			bp = bioq_first(&sc->bio_queue);
			if (bp == NULL)
				msleep(sc, &sc->sc_mtx, PRIBIO, "jobqueue", 0);
		} while (bp == NULL && sc->running);
		if (bp)
			bioq_remove(&sc->bio_queue, bp);
		MMCSD_UNLOCK(sc);
		if (!sc->running)
			break;
//		printf("mmc_task: request %p for block %ju\n", bp, bp->bio_pblkno);
		if (bp->bio_cmd != BIO_READ && mmc_get_read_only(dev)) {
			bp->bio_error = EROFS;
			bp->bio_resid = bp->bio_bcount;
			bp->bio_flags |= BIO_ERROR;
			biodone(bp);
			continue;
		}
		MMCBUS_ACQUIRE_BUS(device_get_parent(dev), dev);
		sz = sc->disk->d_sectorsize;
		end = bp->bio_pblkno + (bp->bio_bcount / sz);
		for (block = bp->bio_pblkno; block < end;) {
			char *vaddr = bp->bio_data + (block - bp->bio_pblkno) * sz;
			int numblocks;
#ifdef MULTI_BLOCK
			numblocks = end - block;
#else
			numblocks = 1;
#endif
			memset(&req, 0, sizeof(req));
			memset(&cmd, 0, sizeof(cmd));
			memset(&stop, 0, sizeof(stop));
			req.cmd = &cmd;
			cmd.data = &data;
			if (bp->bio_cmd == BIO_READ) {
				if (numblocks > 1)
					cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
				else
					cmd.opcode = MMC_READ_SINGLE_BLOCK;
			} else {
				if (numblocks > 1)
					cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
				else
					cmd.opcode = MMC_WRITE_BLOCK;
			}
			cmd.arg = block;
			if (!mmc_get_high_cap(dev))
				cmd.arg <<= 9;
			cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
			data.data = vaddr;
			data.mrq = &req;
			if (bp->bio_cmd == BIO_READ)
				data.flags = MMC_DATA_READ;
			else
				data.flags = MMC_DATA_WRITE;
			data.len = numblocks * sz;
			if (numblocks > 1) {
				data.flags |= MMC_DATA_MULTI;
				stop.opcode = MMC_STOP_TRANSMISSION;
				stop.arg = 0;
				stop.flags = MMC_RSP_R1B | MMC_CMD_AC;
				req.stop = &stop;
			}
//			printf("Len %d  %lld-%lld flags %#x sz %d\n",
//			    (int)data.len, (long long)block, (long long)end, data.flags, sz);
			MMCBUS_WAIT_FOR_REQUEST(device_get_parent(dev), dev,
			    &req);
			if (req.cmd->error != MMC_ERR_NONE)
				break;
			block += numblocks;
		}
		MMCBUS_RELEASE_BUS(device_get_parent(dev), dev);
		if (block < end) {
			bp->bio_error = EIO;
			bp->bio_resid = (end - block) * sz;
			bp->bio_flags |= BIO_ERROR;
		}
		biodone(bp);
	}

	/* tell parent we're done */
	MMCSD_LOCK(sc);
	sc->running = -1;
	wakeup(sc);
	MMCSD_UNLOCK(sc);

	kproc_exit(0);
}

static const char *
mmcsd_card_name(device_t dev)
{
	if (mmc_get_card_type(dev) == mode_mmc)
		return ("MMC");
	if (mmc_get_high_cap(dev))
		return ("SDHC");
	return ("SD");
}

static int
mmcsd_bus_bit_width(device_t dev)
{
	if (mmc_get_bus_width(dev) == bus_width_1)
		return (1);
	if (mmc_get_bus_width(dev) == bus_width_4)
		return (4);
	return (8);
}

static device_method_t mmcsd_methods[] = {
	DEVMETHOD(device_probe, mmcsd_probe),
	DEVMETHOD(device_attach, mmcsd_attach),
	DEVMETHOD(device_detach, mmcsd_detach),
	{0, 0},
};

static driver_t mmcsd_driver = {
	"mmcsd",
	mmcsd_methods,
	sizeof(struct mmcsd_softc),
};
static devclass_t mmcsd_devclass;

DRIVER_MODULE(mmcsd, mmc, mmcsd_driver, mmcsd_devclass, 0, 0);
