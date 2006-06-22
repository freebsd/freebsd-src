/*-
 * Copyright (c) 2006 IronPort Systems
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

#include "opt_mfi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/select.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/uio.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/mfi/mfi_compat.h>
#include <machine/md_var.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <dev/mfi/mfireg.h>
#include <dev/mfi/mfi_ioctl.h>
#include <dev/mfi/mfivar.h>

static int	mfi_disk_probe(device_t dev);
static int	mfi_disk_attach(device_t dev);
static int	mfi_disk_detach(device_t dev);

static d_open_t		mfi_disk_open;
static d_close_t	mfi_disk_close;
static d_strategy_t	mfi_disk_strategy;
static d_dump_t		mfi_disk_dump;

static devclass_t	mfi_disk_devclass;

struct mfi_disk {
	device_t	ld_dev;
	dev_t		ld_dev_t;
	int		ld_id;
	int		ld_unit;
	struct mfi_softc *ld_controller;
	struct mfi_ld	*ld_ld;
	struct disk	ld_disk;
	struct devstat	ld_stats;
	int		ld_flags;
#define	MFID_OPEN	(1<<0)		/* drive is open */
};

static struct cdevsw mfid_cdevsw = {
	mfi_disk_open,
	mfi_disk_close,
	physread,
	physwrite,
	noioctl,
	nopoll,
	nommap,
	mfi_disk_strategy,
	"mfid",
	201,
	mfi_disk_dump,
	nopsize,
	D_DISK,
	-1
};
static struct cdevsw    mfiddisk_cdevsw;
static int mfi_disks_registered;


static device_method_t mfi_disk_methods[] = {
	DEVMETHOD(device_probe,		mfi_disk_probe),
	DEVMETHOD(device_attach,	mfi_disk_attach),
	DEVMETHOD(device_detach,	mfi_disk_detach),
	{ 0, 0 }
};

static driver_t mfi_disk_driver = {
	"mfid",
	mfi_disk_methods,
	sizeof(struct mfi_disk)
};

DRIVER_MODULE(mfid, mfi, mfi_disk_driver, mfi_disk_devclass, 0, 0);

static char *
mfi_disk_describe_state(uint8_t state)
{

	switch (state) {
	case MFI_LD_STATE_OFFLINE:
		return ("offline");

	case MFI_LD_STATE_PARTIALLY_DEGRADED:
		return ("partially degraded");

	case MFI_LD_STATE_DEGRADED:
		return ("degraded");

	case MFI_LD_STATE_OPTIMAL:
		return ("optimal");
	}
	return ("unknown");
}

static int
mfi_disk_probe(device_t dev)
{

	return (0);
}

static int
mfi_disk_attach(device_t dev)
{
	struct mfi_disk *sc;
	struct mfi_ld *ld;
	struct disklabel *label;
	uint64_t sectors;
	uint32_t secsize;

	sc = device_get_softc(dev);
	ld = device_get_ivars(dev);

	sc->ld_dev = dev;
	sc->ld_id = ld->ld_id;
	sc->ld_unit = device_get_unit(dev);
	sc->ld_ld = device_get_ivars(dev);
	sc->ld_controller = device_get_softc(device_get_parent(dev));

	sectors = ld->ld_info->size;
	secsize = MFI_SECTOR_LEN;
	TAILQ_INSERT_TAIL(&sc->ld_controller->mfi_ld_tqh, ld, ld_link);

	device_printf(dev, "%lluMB (%llu sectors) RAID %d (%s)\n",
	    sectors / (1024 * 1024 / secsize), sectors,
	    ld->ld_info->ld_config.params.primary_raid_level,
	    mfi_disk_describe_state(ld->ld_info->ld_config.params.state));

	devstat_add_entry(&sc->ld_stats, "mfid", sc->ld_unit, secsize,
			  DEVSTAT_NO_ORDERED_TAGS,
			  DEVSTAT_TYPE_STORARRAY | DEVSTAT_TYPE_IF_OTHER,
			  DEVSTAT_PRIORITY_ARRAY);

	sc->ld_dev_t =
	    disk_create(sc->ld_unit, &sc->ld_disk, 0, &mfid_cdevsw,
			    &mfiddisk_cdevsw);
	sc->ld_dev_t->si_drv1 = sc;
	mfi_disks_registered++;
	sc->ld_dev_t->si_iosize_max = sc->ld_controller->mfi_max_io * secsize;

	label = &sc->ld_disk.d_label;
	bzero(label, sizeof(*label));
	label->d_type		= DTYPE_SCSI;
	label->d_secsize	= secsize;
	if ((sectors * secsize) > (1 * 1024 * 1024)) {
		label->d_ntracks	= 255;
		label->d_nsectors	= 63;
	} else {
		label->d_ntracks	= 64;
		label->d_nsectors	= 32;
	}
	label->d_ncylinders = sectors / (label->d_ntracks * label->d_nsectors);
	label->d_secpercyl = label->d_ntracks * label->d_nsectors;
	label->d_secperunit = sectors;

	return (0);
}

static int
mfi_disk_detach(device_t dev)
{
	struct mfi_disk *sc;

	sc = device_get_softc(dev);

	if (sc->ld_flags & MFID_OPEN)
		return (EBUSY);

	devstat_remove_entry(&sc->ld_stats);
	if (--mfi_disks_registered == 0)
		cdevsw_remove(&mfiddisk_cdevsw);
	disk_destroy(sc->ld_dev_t);
	return (0);
}

static int
mfi_disk_open(dev_t dev , int flags, int cmd, d_thread_t *td)
{
	struct mfi_disk	*sc;

	sc = (struct mfi_disk *)dev->si_drv1;
	sc->ld_flags |= MFID_OPEN;
	
	return (0);
}

static int
mfi_disk_close(dev_t dev , int flags, int cmd, d_thread_t *td)
{
	struct mfi_disk *sc;

	sc = (struct mfi_disk *)dev->si_drv1;
	if (sc == NULL)
		return (ENXIO);
	sc->ld_flags &= ~MFID_OPEN;

	return (0);
}

static void
mfi_disk_strategy(struct bio *bio)
{
	struct mfi_disk *sc;
	struct mfi_softc *controller;

	sc = bio->bio_disk->d_drv1;

	if (sc == NULL) {
		bio->bio_error = EINVAL;
		bio->bio_flags |= BIO_ERROR;
		bio->bio_resid = bio->bio_bcount;
		biodone(bio);
		return;
	}

	controller = sc->ld_controller;
	bio->bio_driver1 = (void *)(uintptr_t)sc->ld_id;
	devstat_start_transaction(&sc->ld_stats);
	mfi_enqueue_bio(controller, bio);
	mfi_startio(controller);
	return;
}

void
mfi_disk_complete(struct bio *bio)
{
	struct mfi_disk *sc;
	struct mfi_frame_header *hdr;

	sc = bio->bio_disk->d_drv1;
	hdr = bio->bio_driver1;

	if (bio->bio_flags & BIO_ERROR) {
		if (bio->bio_error == 0)
			bio->bio_error = EIO;
		diskerr(bio, "hard error", -1, 1, NULL);
	} else {
		bio->bio_resid = 0;
	}
	devstat_end_transaction_bio(&sc->ld_stats, bio);
	biodone(bio);
}

static int
mfi_disk_dump(dev_t dev)
{
	struct mfi_disk *sc;
	struct mfi_softc *parent_sc;
	u_int count, blkno;
	u_int32_t secsize;
	vm_paddr_t addr = 0;
	long blkcnt;
	int dumppages = MAXDUMPPGS;
	int error;
	int i;

	if ((error = disk_dumpcheck(dev, &count, &blkno, &secsize)))
		return (error);

	sc = dev->d_drv1;
	parent_sc = sc->ld_controller;

	if (!sc || !parent_sc)
		return (ENXIO);

	blkcnt = howmany(PAGE_SIZE, secsize);
	
	while (count > 0) {
		caddr_t va = NULL;

		if ((count / blkcnt) < dumppages)
			dumppages = count / blkcnt;

		for (i = 0; i < dumppages; ++i) {
			vm_paddr_t a = addr + (i * PAGE_SIZE);
			if (is_physical_memory(a))
				va = pmap_kenter_temporary(trunc_page(a), i);
			else
				va = pmap_kenter_temporary(trunc_page(0), i);
		}
		if ((error = mfi_dump_blocks(parent_sc, sc->ld_id, blkno, va,
		    PAGE_SIZE * dumppages)) != 0) {
			return (error);
		}

		if (dumpstatus(addr, (off_t)count * DEV_BSIZE) < 0)
			return (EINTR);

		blkno += blkcnt * dumppages;
		count -= blkcnt * dumppages;
		addr += PAGE_SIZE * dumppages;
	}
	return (0);
}
