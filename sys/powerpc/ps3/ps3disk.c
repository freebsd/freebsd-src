/*-
 * Copyright (C) 2011 glevand (geoffrey.levand@mail.ru)
 * All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/disk.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/pio.h>
#include <machine/bus.h>
#include <machine/platform.h>
#include <machine/pmap.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <geom/geom_disk.h>

#include "ps3bus.h"
#include "ps3-hvcall.h"

#define PS3DISK_LOCK_INIT(_sc)		\
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), "ps3disk", MTX_DEF)
#define PS3DISK_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define PS3DISK_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	PS3DISK_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define PS3DISK_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define PS3DISK_ASSERT_UNLOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

#define LV1_STORAGE_ATA_HDDOUT 		0x23

SYSCTL_NODE(_hw, OID_AUTO, ps3disk, CTLFLAG_RD, 0, "PS3 Disk driver parameters");

#ifdef PS3DISK_DEBUG
static int ps3disk_debug = 0;
SYSCTL_INT(_hw_ps3disk, OID_AUTO, debug, CTLFLAG_RW, &ps3disk_debug,
	0, "control debugging printfs");
TUNABLE_INT("hw.ps3disk.debug", &ps3disk_debug);
enum {
	PS3DISK_DEBUG_INTR	= 0x00000001,
	PS3DISK_DEBUG_TASK	= 0x00000002,
	PS3DISK_DEBUG_READ	= 0x00000004,
	PS3DISK_DEBUG_WRITE	= 0x00000008,
	PS3DISK_DEBUG_FLUSH	= 0x00000010,
	PS3DISK_DEBUG_ANY	= 0xffffffff
};
#define	DPRINTF(sc, m, fmt, ...)				\
do {								\
	if (sc->sc_debug & (m))					\
		printf(fmt, __VA_ARGS__);			\
} while (0)
#else
#define	DPRINTF(sc, m, fmt, ...)
#endif

struct ps3disk_region {
	uint64_t r_id;
	uint64_t r_start;
	uint64_t r_size;
	uint64_t r_flags;
};

struct ps3disk_softc {
	device_t sc_dev;

	struct mtx sc_mtx;

	uint64_t sc_blksize;
	uint64_t sc_nblocks;

	uint64_t sc_nregs;
	struct ps3disk_region *sc_reg;

	int sc_irqid;
	struct resource	*sc_irq;
	void *sc_irqctx;

	struct disk **sc_disk;

	struct bio_queue_head sc_bioq;

	struct proc *sc_task;

	int sc_bounce_maxblocks;
	bus_dma_tag_t sc_bounce_dmatag;
	bus_dmamap_t sc_bounce_dmamap;
	bus_addr_t sc_bounce_dmaphys;
	char *sc_bounce;
	uint64_t sc_bounce_lpar;
	int sc_bounce_busy;
	uint64_t sc_bounce_tag;
	uint64_t sc_bounce_status;

	int sc_running;

	int sc_debug;
};

static int ps3disk_open(struct disk *dp);
static int ps3disk_close(struct disk *dp);
static void ps3disk_strategy(struct bio *bp);
static void ps3disk_task(void *arg);

static int ps3disk_intr_filter(void *arg);
static void ps3disk_intr(void *arg);
static void ps3disk_getphys(void *arg, bus_dma_segment_t *segs, int nsegs, int error);
static int ps3disk_get_disk_geometry(struct ps3disk_softc *sc);
static int ps3disk_enum_regions(struct ps3disk_softc *sc);
static int ps3disk_read(struct ps3disk_softc *sc, int regidx,
	uint64_t start_sector, uint64_t sector_count, char *data);
static int ps3disk_write(struct ps3disk_softc *sc, int regidx,
	uint64_t start_sector, uint64_t sector_count, char *data);
static int ps3disk_flush(struct ps3disk_softc *sc);

static void ps3disk_sysctlattach(struct ps3disk_softc *sc);

static MALLOC_DEFINE(M_PS3DISK, "ps3disk", "PS3 Disk");

static int
ps3disk_probe(device_t dev)
{
	if (ps3bus_get_bustype(dev) != PS3_BUSTYPE_STORAGE ||
	    ps3bus_get_devtype(dev) != PS3_DEVTYPE_DISK)
		return (ENXIO);

	device_set_desc(dev, "Playstation 3 Disk");

	return (BUS_PROBE_SPECIFIC);
}

static int
ps3disk_attach(device_t dev)
{
	struct ps3disk_softc *sc;
	struct disk *d;
	intmax_t mb;
	char unit;
	int i, err;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	PS3DISK_LOCK_INIT(sc);

	err = ps3disk_get_disk_geometry(sc);
	if (err) {
		device_printf(dev, "Could not get disk geometry\n");
		err = ENXIO;
		goto fail_destroy_lock;
	}

	device_printf(dev, "block size %lu total blocks %lu\n",
	    sc->sc_blksize, sc->sc_nblocks);

	err = ps3disk_enum_regions(sc);
	if (err) {
		device_printf(dev, "Could not enumerate disk regions\n");
		err = ENXIO;
		goto fail_destroy_lock;
	}

	device_printf(dev, "Found %lu regions\n", sc->sc_nregs);

	if (!sc->sc_nregs) {
		err = ENXIO;
		goto fail_destroy_lock;
	}

	/* Setup interrupt handler */

	sc->sc_irqid = 0;
	sc->sc_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->sc_irqid,
	    RF_ACTIVE);
	if (!sc->sc_irq) {
		device_printf(dev, "Could not allocate IRQ\n");
		err = ENXIO;
		goto fail_free_regions;
	}

	err = bus_setup_intr(dev, sc->sc_irq,
	    INTR_TYPE_BIO | INTR_MPSAFE | INTR_ENTROPY,
	    ps3disk_intr_filter, ps3disk_intr, sc, &sc->sc_irqctx);
	if (err) {
		device_printf(dev, "Could not setup IRQ\n");
		err = ENXIO;
		goto fail_release_intr;
	}

	/* Setup DMA bounce buffer */

	sc->sc_bounce_maxblocks = DFLTPHYS / sc->sc_blksize;

	err = bus_dma_tag_create(bus_get_dma_tag(dev), 4096, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    sc->sc_bounce_maxblocks * sc->sc_blksize, 1,
	    sc->sc_bounce_maxblocks * sc->sc_blksize,
	    0, NULL, NULL, &sc->sc_bounce_dmatag);
	if (err) {
		device_printf(dev, "Could not create DMA tag for bounce buffer\n");
		err = ENXIO;
		goto fail_teardown_intr;
	}

	err = bus_dmamem_alloc(sc->sc_bounce_dmatag, (void **) &sc->sc_bounce,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO,
	    &sc->sc_bounce_dmamap);
	if (err) {
		device_printf(dev, "Could not allocate DMA memory for bounce buffer\n");
		err = ENXIO;
		goto fail_destroy_dmatag;
	}

	err = bus_dmamap_load(sc->sc_bounce_dmatag, sc->sc_bounce_dmamap,
	    sc->sc_bounce, sc->sc_bounce_maxblocks * sc->sc_blksize,
	    ps3disk_getphys, &sc->sc_bounce_dmaphys, 0);
	if (err) {
		device_printf(dev, "Could not load DMA map for bounce buffer\n");
		err = ENXIO;
		goto fail_free_dmamem;
	}

	sc->sc_bounce_lpar = vtophys(sc->sc_bounce);

	if (bootverbose)
		device_printf(dev, "bounce buffer lpar address 0x%016lx\n",
		    sc->sc_bounce_lpar);

	/* Setup disks */

	sc->sc_disk = malloc(sc->sc_nregs * sizeof(struct disk *),
	    M_PS3DISK, M_ZERO | M_WAITOK);
	if (!sc->sc_disk) {
		device_printf(dev, "Could not allocate disk(s)\n");
		err = ENOMEM;
		goto fail_unload_dmamem;
	}

	for (i = 0; i < sc->sc_nregs; i++) {
		d = sc->sc_disk[i] = disk_alloc();
		d->d_open = ps3disk_open;
		d->d_close = ps3disk_close;
		d->d_strategy = ps3disk_strategy;
		d->d_name = "ps3disk";
		d->d_drv1 = sc;
		d->d_maxsize = DFLTPHYS;
		d->d_sectorsize = sc->sc_blksize;
		d->d_unit = i;
		d->d_mediasize = sc->sc_reg[i].r_size * sc->sc_blksize;
		d->d_flags |= DISKFLAG_CANFLUSHCACHE;

		mb = d->d_mediasize >> 20;
		unit = 'M';
		if (mb >= 10240) {
			unit = 'G';
			mb /= 1024;
		}

		device_printf(dev, "region %d %ju%cB\n", i, mb, unit);

		disk_create(d, DISK_VERSION);
	}

	bioq_init(&sc->sc_bioq);

	ps3disk_sysctlattach(sc);

	sc->sc_running = 1;

	kproc_create(&ps3disk_task, sc, &sc->sc_task, 0, 0, "task: ps3disk");

	return (0);

fail_unload_dmamem:

	bus_dmamap_unload(sc->sc_bounce_dmatag, sc->sc_bounce_dmamap);

fail_free_dmamem:

	bus_dmamem_free(sc->sc_bounce_dmatag, sc->sc_bounce, sc->sc_bounce_dmamap);

fail_destroy_dmatag:

	bus_dma_tag_destroy(sc->sc_bounce_dmatag);

fail_teardown_intr:

	bus_teardown_intr(dev, sc->sc_irq, sc->sc_irqctx);

fail_release_intr:

	bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irqid, sc->sc_irq);

fail_free_regions:

	free(sc->sc_reg, M_PS3DISK);

fail_destroy_lock:

	PS3DISK_LOCK_DESTROY(sc);

	return (err);
}

static int
ps3disk_detach(device_t dev)
{
	struct ps3disk_softc *sc = device_get_softc(dev);
	int i;

	PS3DISK_LOCK(sc);
	sc->sc_running = 0;
	wakeup(sc);
	PS3DISK_UNLOCK(sc);

	PS3DISK_LOCK(sc);
	while (sc->sc_running != -1)
		msleep(sc, &sc->sc_mtx, PRIBIO, "detach", 0);
	PS3DISK_UNLOCK(sc);

	for (i = 0; i < sc->sc_nregs; i++)
		disk_destroy(sc->sc_disk[i]);

	bus_dmamap_unload(sc->sc_bounce_dmatag, sc->sc_bounce_dmamap);
	bus_dmamem_free(sc->sc_bounce_dmatag, sc->sc_bounce, sc->sc_bounce_dmamap);
	bus_dma_tag_destroy(sc->sc_bounce_dmatag);

	bus_teardown_intr(dev, sc->sc_irq, sc->sc_irqctx);
	bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irqid, sc->sc_irq);

	free(sc->sc_disk, M_PS3DISK);

	free(sc->sc_reg, M_PS3DISK);

	PS3DISK_LOCK_DESTROY(sc);

	return (0);
}

static int
ps3disk_open(struct disk *dp)
{
	return (0);
}

static int
ps3disk_close(struct disk *dp)
{
	return (0);
}

static void
ps3disk_strategy(struct bio *bp)
{
	struct ps3disk_softc *sc = (struct ps3disk_softc *) bp->bio_disk->d_drv1;

	if (!sc) {
		bp->bio_flags |= BIO_ERROR;
		bp->bio_error = EINVAL;
		biodone(bp);
		return;
	}

	PS3DISK_LOCK(sc);
	bioq_disksort(&sc->sc_bioq, bp);
	if (!sc->sc_bounce_busy)
		wakeup(sc);
	PS3DISK_UNLOCK(sc);
}

static void
ps3disk_task(void *arg)
{
	struct ps3disk_softc *sc = (struct ps3disk_softc *) arg;
	struct bio *bp;
	daddr_t block, end;
	u_long nblocks;
	char *data;
	int err;

	while (sc->sc_running) {
		PS3DISK_LOCK(sc);
		do {
			bp = bioq_first(&sc->sc_bioq);
			if (bp == NULL)
				msleep(sc, &sc->sc_mtx, PRIBIO, "jobqueue", 0);
		} while (bp == NULL && sc->sc_running);
		if (bp)
			bioq_remove(&sc->sc_bioq, bp);
		PS3DISK_UNLOCK(sc);

		if (!sc->sc_running)
			break;

		DPRINTF(sc, PS3DISK_DEBUG_TASK, "%s: bio_cmd 0x%02x\n",
	 	    __func__, bp->bio_cmd);

		if (bp->bio_cmd == BIO_FLUSH) {
			err = ps3disk_flush(sc);

			if (err) {
				bp->bio_error = EIO;
				bp->bio_flags |= BIO_ERROR;
			} else {
				bp->bio_error = 0;
				bp->bio_flags |= BIO_DONE;
			}
		} else if (bp->bio_cmd == BIO_READ || bp->bio_cmd == BIO_WRITE) {
			end = bp->bio_pblkno + (bp->bio_bcount / sc->sc_blksize);

			DPRINTF(sc, PS3DISK_DEBUG_TASK, "%s: bio_pblkno %ld bio_bcount %ld\n",
	 		    __func__, bp->bio_pblkno, bp->bio_bcount);

			for (block = bp->bio_pblkno; block < end;) {
				data = bp->bio_data + 
				    (block - bp->bio_pblkno) * sc->sc_blksize;

				nblocks = end - block;
				if (nblocks > sc->sc_bounce_maxblocks)
					nblocks = sc->sc_bounce_maxblocks;

				DPRINTF(sc, PS3DISK_DEBUG_TASK, "%s: nblocks %lu\n",
	 			    __func__, nblocks);

				if (bp->bio_cmd == BIO_READ) {
					err = ps3disk_read(sc, bp->bio_disk->d_unit,
					    block, nblocks, data);
				} else {
					err = ps3disk_write(sc, bp->bio_disk->d_unit,
					    block, nblocks, data);
				}
		
				if (err)
					break;

				block += nblocks;
			}

			bp->bio_resid = (end - block) * sc->sc_blksize;
			if (bp->bio_resid) {
				bp->bio_error = EIO;
				bp->bio_flags |= BIO_ERROR;
			} else {
				bp->bio_error = 0;
				bp->bio_flags |= BIO_DONE;
			}

			DPRINTF(sc, PS3DISK_DEBUG_TASK, "%s: bio_resid %ld\n",
	 		    __func__, bp->bio_resid);
		} else {
			bp->bio_error = EINVAL;
			bp->bio_flags |= BIO_ERROR;
		}

		if (bp->bio_flags & BIO_ERROR)
			disk_err(bp, "hard error", -1, 1);

		biodone(bp);
	}

	PS3DISK_LOCK(sc);
	sc->sc_running = -1;
	wakeup(sc);
	PS3DISK_UNLOCK(sc);

	kproc_exit(0);
}

static int
ps3disk_intr_filter(void *arg)
{
	return (FILTER_SCHEDULE_THREAD);
}

static void
ps3disk_intr(void *arg)
{
	struct ps3disk_softc *sc = (struct ps3disk_softc *) arg;
	device_t dev = sc->sc_dev;
	uint64_t devid = ps3bus_get_device(dev);
	uint64_t tag, status;
	int err;

	PS3DISK_LOCK(sc);

	err = lv1_storage_get_async_status(devid, &tag, &status);

	DPRINTF(sc, PS3DISK_DEBUG_INTR, "%s: err %d tag 0x%016lx status 0x%016lx\n",
	    __func__, err, tag, status);

	if (err)
		goto out;

	if (!sc->sc_bounce_busy) {
		device_printf(dev, "Got interrupt while no request pending\n");
		goto out;
	}

	if (tag != sc->sc_bounce_tag)
		device_printf(dev, "Tag mismatch, got 0x%016lx expected 0x%016lx\n",
		    tag, sc->sc_bounce_tag);

	if (status)
		device_printf(dev, "Request completed with status 0x%016lx\n", status);

	sc->sc_bounce_status = status;
	sc->sc_bounce_busy = 0;

	wakeup(sc);

out:

	PS3DISK_UNLOCK(sc);
}

static void
ps3disk_getphys(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error != 0)
		return;

	*(bus_addr_t *) arg = segs[0].ds_addr;
}

static int
ps3disk_get_disk_geometry(struct ps3disk_softc *sc)
{
	device_t dev = sc->sc_dev;
	uint64_t bus_index = ps3bus_get_busidx(dev);
	uint64_t dev_index = ps3bus_get_devidx(dev);
	uint64_t junk;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME,
	    (lv1_repository_string("bus") >> 32) | bus_index,
	    lv1_repository_string("dev") | dev_index,
	    lv1_repository_string("blk_size"), 0, &sc->sc_blksize, &junk);
	if (err) {
		device_printf(dev, "Could not get block size (0x%08x)\n", err);
		err = ENXIO;
		goto out;
	}

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME,
	    (lv1_repository_string("bus") >> 32) | bus_index,
	    lv1_repository_string("dev") | dev_index,
	    lv1_repository_string("n_blocks"), 0, &sc->sc_nblocks, &junk);
	if (err) {
		device_printf(dev, "Could not get total number of blocks (0x%08x)\n",
		    err);
		err = ENXIO;
		goto out;
	}

	err = 0;

out:

	return (err);
}

static int
ps3disk_enum_regions(struct ps3disk_softc *sc)
{
	device_t dev = sc->sc_dev;
	uint64_t bus_index = ps3bus_get_busidx(dev);
	uint64_t dev_index = ps3bus_get_devidx(dev);
	uint64_t junk;
	int i, err;

	/* Read number of regions */

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME,
	    (lv1_repository_string("bus") >> 32) | bus_index,
	    lv1_repository_string("dev") | dev_index,
	    lv1_repository_string("n_regs"), 0, &sc->sc_nregs, &junk);
	if (err) {
		device_printf(dev, "Could not get number of regions (0x%08x)\n",
		    err);
		err = ENXIO;
		goto fail;
	}

	if (!sc->sc_nregs)
		return 0;

	sc->sc_reg = malloc(sc->sc_nregs * sizeof(struct ps3disk_region),
	    M_PS3DISK, M_ZERO | M_WAITOK);
	if (!sc->sc_reg) {
		err = ENOMEM;
		goto fail;
	}

	/* Setup regions */

	for (i = 0; i < sc->sc_nregs; i++) {
		err = lv1_get_repository_node_value(PS3_LPAR_ID_PME,
		    (lv1_repository_string("bus") >> 32) | bus_index,
		    lv1_repository_string("dev") | dev_index,
		    lv1_repository_string("region") | i,
		    lv1_repository_string("id"), &sc->sc_reg[i].r_id, &junk);
		if (err) {
			device_printf(dev, "Could not get region id (0x%08x)\n",
			    err);
			err = ENXIO;
			goto fail;
		}

		err = lv1_get_repository_node_value(PS3_LPAR_ID_PME,
		    (lv1_repository_string("bus") >> 32) | bus_index,
		    lv1_repository_string("dev") | dev_index,
		    lv1_repository_string("region") | i,
		    lv1_repository_string("start"), &sc->sc_reg[i].r_start, &junk);
		if (err) {
			device_printf(dev, "Could not get region start (0x%08x)\n",
			    err);
			err = ENXIO;
			goto fail;
		}

		err = lv1_get_repository_node_value(PS3_LPAR_ID_PME,
		    (lv1_repository_string("bus") >> 32) | bus_index,
		    lv1_repository_string("dev") | dev_index,
		    lv1_repository_string("region") | i,
		    lv1_repository_string("size"), &sc->sc_reg[i].r_size, &junk);
		if (err) {
			device_printf(dev, "Could not get region size (0x%08x)\n",
			    err);
			err = ENXIO;
			goto fail;
		}

		if (i == 0)
			/* disables HV access control and grants access to whole disk */
			sc->sc_reg[i].r_flags = 0x2;
		else
			sc->sc_reg[i].r_flags = 0;
	}

	return (0);

fail:

	sc->sc_nregs = 0;
	if (sc->sc_reg)
		free(sc->sc_reg, M_PS3DISK);

	return (err);
}

static int
ps3disk_read(struct ps3disk_softc *sc, int regidx,
	uint64_t start_sector, uint64_t sector_count, char *data)
{
	device_t dev = sc->sc_dev;
	struct ps3disk_region *rp = &sc->sc_reg[regidx];
	uint64_t devid = ps3bus_get_device(dev);
	int err;

	PS3DISK_LOCK(sc);

	if (sc->sc_bounce_busy) {
		device_printf(dev, "busy\n");
		PS3DISK_UNLOCK(sc);
		return EIO;
	}

	sc->sc_bounce_busy = 1;

	err = lv1_storage_read(devid, rp->r_id,
	    start_sector, sector_count, rp->r_flags,
	    sc->sc_bounce_lpar, &sc->sc_bounce_tag);
	if (err) {
		device_printf(dev, "Could not read sectors (0x%08x)\n", err);
		err = EIO;
		goto out;
	}

	DPRINTF(sc, PS3DISK_DEBUG_READ, "%s: tag 0x%016lx\n",
	    __func__, sc->sc_bounce_tag);

	err = msleep(sc, &sc->sc_mtx, PRIBIO, "read", hz);
	if (err) {
		device_printf(dev, "Read request timed out\n");
		err = EIO;
		goto out;
	}

	if (sc->sc_bounce_busy || sc->sc_bounce_status) {
		err = EIO;
	} else {
		bus_dmamap_sync(sc->sc_bounce_dmatag, sc->sc_bounce_dmamap,
		    BUS_DMASYNC_POSTREAD);
		memcpy(data, sc->sc_bounce, sector_count * sc->sc_blksize);
		err = 0;
	}

out:

	sc->sc_bounce_busy = 0;

	PS3DISK_UNLOCK(sc);

	return (err);
}

static int
ps3disk_write(struct ps3disk_softc *sc, int regidx,
	uint64_t start_sector, uint64_t sector_count, char *data)
{
	device_t dev = sc->sc_dev;
	struct ps3disk_region *rp = &sc->sc_reg[regidx];
	uint64_t devid = ps3bus_get_device(dev);
	int err;

	PS3DISK_LOCK(sc);

	if (sc->sc_bounce_busy) {
		device_printf(dev, "busy\n");
		PS3DISK_UNLOCK(sc);
		return EIO;
	}

	memcpy(sc->sc_bounce, data, sector_count * sc->sc_blksize);

	bus_dmamap_sync(sc->sc_bounce_dmatag, sc->sc_bounce_dmamap,
	    BUS_DMASYNC_PREWRITE);

	sc->sc_bounce_busy = 1;

	err = lv1_storage_write(devid, rp->r_id,
	    start_sector, sector_count, rp->r_flags,
	    sc->sc_bounce_lpar, &sc->sc_bounce_tag);
	if (err) {
		device_printf(dev, "Could not write sectors (0x%08x)\n", err);
		err = EIO;
		goto out;
	}

	DPRINTF(sc, PS3DISK_DEBUG_WRITE, "%s: tag 0x%016lx\n",
	    __func__, sc->sc_bounce_tag);

	err = msleep(sc, &sc->sc_mtx, PRIBIO, "write", hz);
	if (err) {
		device_printf(dev, "Write request timed out\n");
		err = EIO;
		goto out;
	}

	err = (sc->sc_bounce_busy || sc->sc_bounce_status) ? EIO : 0;

out:

	sc->sc_bounce_busy = 0;

	PS3DISK_UNLOCK(sc);

	return (err);
}

static int
ps3disk_flush(struct ps3disk_softc *sc)
{
	device_t dev = sc->sc_dev;
	uint64_t devid = ps3bus_get_device(dev);
	int err;

	PS3DISK_LOCK(sc);

	if (sc->sc_bounce_busy) {
		device_printf(dev, "busy\n");
		PS3DISK_UNLOCK(sc);
		return EIO;
	}

	sc->sc_bounce_busy = 1;

	err = lv1_storage_send_device_command(devid, LV1_STORAGE_ATA_HDDOUT,
	    0, 0, 0, 0, &sc->sc_bounce_tag);
	if (err) {
		device_printf(dev, "Could not flush (0x%08x)\n", err);
		err = EIO;
		goto out;
	}

	DPRINTF(sc, PS3DISK_DEBUG_FLUSH, "%s: tag 0x%016lx\n",
	    __func__, sc->sc_bounce_tag);

	err = msleep(sc, &sc->sc_mtx, PRIBIO, "flush", hz);
	if (err) {
		device_printf(dev, "Flush request timed out\n");
		err = EIO;
		goto out;
	}

	err = (sc->sc_bounce_busy || sc->sc_bounce_status) ? EIO : 0;

out:

	sc->sc_bounce_busy = 0;

	PS3DISK_UNLOCK(sc);

	return (err);
}

#ifdef PS3DISK_DEBUG
static int
ps3disk_sysctl_debug(SYSCTL_HANDLER_ARGS)
{
	struct ps3disk_softc *sc = arg1;
	int debug, error;

	debug = sc->sc_debug;

	error = sysctl_handle_int(oidp, &debug, 0, req);
	if (error || !req->newptr)
		return error;

	sc->sc_debug = debug;

	return 0;
}
#endif

static void
ps3disk_sysctlattach(struct ps3disk_softc *sc)
{
#ifdef PS3DISK_DEBUG
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

	sc->sc_debug = ps3disk_debug;

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		"debug", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
		ps3disk_sysctl_debug, "I", "control debugging printfs");
#endif
}

static device_method_t ps3disk_methods[] = {
	DEVMETHOD(device_probe,		ps3disk_probe),
	DEVMETHOD(device_attach,	ps3disk_attach),
	DEVMETHOD(device_detach,	ps3disk_detach),
	{0, 0},
};

static driver_t ps3disk_driver = {
	"ps3disk",
	ps3disk_methods,
	sizeof(struct ps3disk_softc),
};

static devclass_t ps3disk_devclass;

DRIVER_MODULE(ps3disk, ps3bus, ps3disk_driver, ps3disk_devclass, 0, 0);
