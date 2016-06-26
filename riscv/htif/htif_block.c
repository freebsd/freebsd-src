/*-
 * Copyright (c) 2015-2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/selinfo.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <geom/geom_disk.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/md_var.h>
#include <machine/bus.h>
#include <machine/trap.h>
#include <sys/rman.h>

#include "htif.h"

#define	SECTOR_SIZE_SHIFT	(9)
#define	SECTOR_SIZE		(1 << SECTOR_SIZE_SHIFT)

#define	HTIF_BLK_LOCK(_sc)	mtx_lock(&(_sc)->sc_mtx)
#define	HTIF_BLK_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)
#define	HTIF_BLK_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "htif_blk", MTX_DEF)
#define	HTIF_BLK_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define	HTIF_BLK_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define	HTIF_BLK_ASSERT_UNLOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static void htif_blk_task(void *arg);

static disk_open_t	htif_blk_open;
static disk_close_t	htif_blk_close;
static disk_strategy_t	htif_blk_strategy;

struct htif_blk_softc {
	device_t	dev;
	struct disk	*disk;
	struct mtx	htif_io_mtx;
	struct mtx	sc_mtx;
	struct proc	*p;
	struct bio_queue_head bio_queue;
	int		running;
	int		intr_chan;
	int		cmd_done;
	int		index;
	uint16_t	curtag;
};

struct htif_blk_request {
	uint64_t addr;
	uint64_t offset;	/* offset in bytes */
	uint64_t size;		/* length in bytes */
	uint64_t tag;
};

static void
htif_blk_intr(void *arg, uint64_t entry)
{
	struct htif_blk_softc *sc;
	uint64_t devcmd;
	uint64_t data;

	sc = arg;

	devcmd = HTIF_DEV_CMD(entry);
	data = HTIF_DEV_DATA(entry);

	if (sc->curtag == data) {
		wmb();
		sc->cmd_done = 1;
		wakeup(&sc->intr_chan);
	} else {
		device_printf(sc->dev, "Unexpected tag %d (should be %d)\n",
		    data, sc->curtag);
	}
}

static int
htif_blk_probe(device_t dev)
{

	return (0);
}

static int
htif_blk_attach(device_t dev)
{
	struct htif_blk_softc *sc;
	char prefix[] = " size=";
	char *str;
	long size;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->htif_io_mtx, device_get_nameunit(dev), "htif_blk", MTX_DEF);
	HTIF_BLK_LOCK_INIT(sc);

	str = strstr(htif_get_id(dev), prefix);

	size = strtol((str + 6), NULL, 10);
	if (size == 0) {
		return (ENXIO);
	}

	sc->index = htif_get_index(dev);
	if (sc->index < 0)
		return (EINVAL);
	htif_setup_intr(sc->index, htif_blk_intr, sc);

	sc->disk = disk_alloc();
	sc->disk->d_drv1 = sc;

	sc->disk->d_maxsize = 4096; /* Max transfer */
	sc->disk->d_name = "htif_blk";
	sc->disk->d_open = htif_blk_open;
	sc->disk->d_close = htif_blk_close;
	sc->disk->d_strategy = htif_blk_strategy;
	sc->disk->d_unit = 0;
	sc->disk->d_sectorsize = SECTOR_SIZE;
	sc->disk->d_mediasize = size;
	disk_create(sc->disk, DISK_VERSION);

	bioq_init(&sc->bio_queue);

	sc->running = 1;

	kproc_create(&htif_blk_task, sc, &sc->p, 0, 0, "%s: transfer", 
	    device_get_nameunit(dev));

	return (0);
}

static int
htif_blk_open(struct disk *dp)
{

	return (0);
}

static int
htif_blk_close(struct disk *dp)
{

	return (0);
}

static void
htif_blk_task(void *arg)
{
	struct htif_blk_request req __aligned(HTIF_ALIGN);
	struct htif_blk_softc *sc;
	uint64_t req_paddr;
	struct bio *bp;
	uint64_t paddr;
	uint64_t cmd;
	int i;

	sc = (struct htif_blk_softc *)arg;

	while (1) {
		HTIF_BLK_LOCK(sc);
		do {
			bp = bioq_takefirst(&sc->bio_queue);
			if (bp == NULL)
				msleep(sc, &sc->sc_mtx, PRIBIO, "jobqueue", 0);
		} while (bp == NULL);
		HTIF_BLK_UNLOCK(sc);

		if (bp->bio_cmd == BIO_READ || bp->bio_cmd == BIO_WRITE) {
			HTIF_BLK_LOCK(sc);

			rmb();
			req.offset = (bp->bio_pblkno * sc->disk->d_sectorsize);
			req.size = bp->bio_bcount;
			paddr = vtophys(bp->bio_data);
			KASSERT(paddr != 0, ("paddr is 0"));
			req.addr = paddr;
			sc->curtag++;
			req.tag = sc->curtag;

			cmd = sc->index;
			cmd <<= HTIF_DEV_ID_SHIFT;
			if (bp->bio_cmd == BIO_READ)
				cmd |= (HTIF_CMD_READ << HTIF_CMD_SHIFT);
			else
				cmd |= (HTIF_CMD_WRITE << HTIF_CMD_SHIFT);
			req_paddr = vtophys(&req);
			KASSERT(req_paddr != 0, ("req_paddr is 0"));
			cmd |= req_paddr;

			sc->cmd_done = 0;
			htif_command(cmd);

			/* Wait for interrupt */
			i = 0;
			while (sc->cmd_done == 0) {
				msleep(&sc->intr_chan, &sc->sc_mtx, PRIBIO, "intr", hz/2);

				if (i++ > 2) {
					/* TODO: try to re-issue operation on timeout ? */
					bp->bio_error = EIO;
					bp->bio_flags |= BIO_ERROR;
					disk_err(bp, "hard error", -1, 1);
					break;
				}
			}
			HTIF_BLK_UNLOCK(sc);

			biodone(bp);
		} else {
			printf("unknown op %d\n", bp->bio_cmd);
		}
	}
}

static void
htif_blk_strategy(struct bio *bp)
{
	struct htif_blk_softc *sc;

	sc = bp->bio_disk->d_drv1;

	HTIF_BLK_LOCK(sc);
	if (sc->running > 0) {
		bioq_disksort(&sc->bio_queue, bp);
		HTIF_BLK_UNLOCK(sc);
		wakeup(sc);
	} else {
		HTIF_BLK_UNLOCK(sc);
		biofinish(bp, NULL, ENXIO);
	}
}

static device_method_t htif_blk_methods[] = {
	DEVMETHOD(device_probe,		htif_blk_probe),
	DEVMETHOD(device_attach,	htif_blk_attach),
};

static driver_t htif_blk_driver = {
	"htif_blk",
	htif_blk_methods,
	sizeof(struct htif_blk_softc)
};

static devclass_t	htif_blk_devclass;

DRIVER_MODULE(htif_blk, htif, htif_blk_driver, htif_blk_devclass, 0, 0);
