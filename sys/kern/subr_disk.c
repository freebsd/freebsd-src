/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include "opt_geom.h"
#ifndef GEOM

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <machine/md_var.h>
#include <sys/ctype.h>

static MALLOC_DEFINE(M_DISK, "disk", "disk data");

static d_strategy_t diskstrategy;
static d_open_t diskopen;
static d_close_t diskclose; 
static d_ioctl_t diskioctl;
static d_psize_t diskpsize;

static LIST_HEAD(, disk) disklist = LIST_HEAD_INITIALIZER(&disklist);

void disk_dev_synth(dev_t dev);

void
disk_dev_synth(dev_t dev)
{
	struct disk *dp;
	int u, s, p;
	dev_t pdev;

	if (dksparebits(dev))
		return;
	LIST_FOREACH(dp, &disklist, d_list) {
		if (major(dev) != dp->d_devsw->d_maj)
			continue;
		u = dkunit(dev);
		p = RAW_PART;
		s = WHOLE_DISK_SLICE;
		pdev = makedev(dp->d_devsw->d_maj, dkmakeminor(u, s, p));
		if (pdev->si_devsw == NULL)
			return;		/* Probably a unit we don't have */
		s = dkslice(dev);
		p = dkpart(dev);
		if (s == WHOLE_DISK_SLICE && p == RAW_PART) {
			/* XXX: actually should not happen */
			dev = make_dev(pdev->si_devsw, dkmakeminor(u, s, p), 
			    UID_ROOT, GID_OPERATOR, 0640, "%s%d", 
				dp->d_devsw->d_name, u);
			dev_depends(pdev, dev);
			return;
		}
		if (s == COMPATIBILITY_SLICE) {
			dev = make_dev(pdev->si_devsw, dkmakeminor(u, s, p), 
			    UID_ROOT, GID_OPERATOR, 0640, "%s%d%c", 
				dp->d_devsw->d_name, u, 'a' + p);
			dev_depends(pdev, dev);
			return;
		}
		if (p != RAW_PART) {
			dev = make_dev(pdev->si_devsw, dkmakeminor(u, s, p), 
			    UID_ROOT, GID_OPERATOR, 0640, "%s%ds%d%c", 
				dp->d_devsw->d_name, u, s - BASE_SLICE + 1,
				'a' + p);
		} else {
			dev = make_dev(pdev->si_devsw, dkmakeminor(u, s, p), 
			    UID_ROOT, GID_OPERATOR, 0640, "%s%ds%d", 
				dp->d_devsw->d_name, u, s - BASE_SLICE + 1);
			make_dev_alias(dev, "%s%ds%dc",
			    dp->d_devsw->d_name, u, s - BASE_SLICE + 1);
		}
		dev_depends(pdev, dev);
		return;
	}
}

static void
disk_clone(void *arg, char *name, int namelen, dev_t *dev)
{
	struct disk *dp;
	char const *d;
	char *e;
	int j, u, s, p;
	dev_t pdev;

	if (*dev != NODEV)
		return;

	LIST_FOREACH(dp, &disklist, d_list) {
		d = dp->d_devsw->d_name;
		j = dev_stdclone(name, &e, d, &u);
		if (j == 0)
			continue;
		if (u > DKMAXUNIT)
			continue;
		p = RAW_PART;
		s = WHOLE_DISK_SLICE;
		pdev = makedev(dp->d_devsw->d_maj, dkmakeminor(u, s, p));
		if (pdev->si_disk == NULL)
			continue;
		if (*e != '\0') {
			j = dev_stdclone(e, &e, "s", &s);
			if (j == 0) 
				s = COMPATIBILITY_SLICE;
			else if (j == 1 || j == 2)
				s += BASE_SLICE - 1;
			if (!*e)
				;		/* ad0s1 case */
			else if (e[1] != '\0')
				return;		/* can never be a disk name */
			else if (*e < 'a' || *e > 'h')
				return;		/* can never be a disk name */
			else
				p = *e - 'a';
		}
		if (s == WHOLE_DISK_SLICE && p == RAW_PART) {
			return;
		} else if (s >= BASE_SLICE && p != RAW_PART) {
			*dev = make_dev(pdev->si_devsw, dkmakeminor(u, s, p), 
			    UID_ROOT, GID_OPERATOR, 0640, "%s%ds%d%c",
			    pdev->si_devsw->d_name, u, s - BASE_SLICE + 1, 
			    p + 'a');
		} else if (s >= BASE_SLICE) {
			*dev = make_dev(pdev->si_devsw, dkmakeminor(u, s, p), 
			    UID_ROOT, GID_OPERATOR, 0640, "%s%ds%d",
			    pdev->si_devsw->d_name, u, s - BASE_SLICE + 1);
			make_dev_alias(*dev, "%s%ds%dc", 
			    pdev->si_devsw->d_name, u, s - BASE_SLICE + 1);
		} else {
			*dev = make_dev(pdev->si_devsw, dkmakeminor(u, s, p), 
			    UID_ROOT, GID_OPERATOR, 0640, "%s%d%c",
			    pdev->si_devsw->d_name, u, p + 'a');
		}
		dev_depends(pdev, *dev);
		return;
	}
}

static void
inherit_raw(dev_t pdev, dev_t dev)
{
	dev->si_disk = pdev->si_disk;
	dev->si_drv1 = pdev->si_drv1;
	dev->si_drv2 = pdev->si_drv2;
	dev->si_iosize_max = pdev->si_iosize_max;
	dev->si_bsize_phys = pdev->si_bsize_phys;
	dev->si_bsize_best = pdev->si_bsize_best;
}

dev_t
disk_create(int unit, struct disk *dp, int flags, struct cdevsw *cdevsw, struct cdevsw *proto)
{
	static int once;
	dev_t dev;

	if (!once) {
		EVENTHANDLER_REGISTER(dev_clone, disk_clone, 0, 1000);
		once++;
	}

	bzero(dp, sizeof(*dp));

	if (proto->d_open != diskopen) {
		*proto = *cdevsw;
		proto->d_open = diskopen;
		proto->d_close = diskclose;
		proto->d_ioctl = diskioctl;
		proto->d_strategy = diskstrategy;
		proto->d_psize = diskpsize;
	}

	if (bootverbose)
		printf("Creating DISK %s%d\n", cdevsw->d_name, unit);
	dev = make_dev(proto, dkmakeminor(unit, WHOLE_DISK_SLICE, RAW_PART),
	    UID_ROOT, GID_OPERATOR, 0640, "%s%d", cdevsw->d_name, unit);

	dev->si_disk = dp;
	dp->d_dev = dev;
	dp->d_dsflags = flags;
	dp->d_devsw = cdevsw;
	LIST_INSERT_HEAD(&disklist, dp, d_list);

	return (dev);
}

static int
diskdumpconf(u_int onoff, dev_t dev, struct disk *dp)
{
	struct dumperinfo di;
	struct disklabel *dl;

	if (!onoff) 
		return(set_dumper(NULL));
	dl = dsgetlabel(dev, dp->d_slice);
	if (!dl)
		return (ENXIO);
	bzero(&di, sizeof di);
	di.dumper = (dumper_t *)dp->d_devsw->d_dump;
	di.priv = dp->d_dev;
	di.blocksize = dl->d_secsize;
	di.mediaoffset = (off_t)(dl->d_partitions[dkpart(dev)].p_offset +
	    dp->d_slice->dss_slices[dkslice(dev)].ds_offset) * DEV_BSIZE;
	di.mediasize =
	    (off_t)(dl->d_partitions[dkpart(dev)].p_size) * DEV_BSIZE;
	return(set_dumper(&di));
}

void 
disk_invalidate (struct disk *disk)
{
	if (disk->d_slice)
		dsgone(&disk->d_slice);
}

void
disk_destroy(dev_t dev)
{
	LIST_REMOVE(dev->si_disk, d_list);
	bzero(dev->si_disk, sizeof(*dev->si_disk));
    	dev->si_disk = NULL;
	destroy_dev(dev);
	return;
}

struct disk *
disk_enumerate(struct disk *disk)
{
	if (!disk)
		return (LIST_FIRST(&disklist));
	else
		return (LIST_NEXT(disk, d_list));
}

static int
sysctl_disks(SYSCTL_HANDLER_ARGS)
{
	struct disk *disk;
	int error, first;

	disk = NULL;
	first = 1;

	while ((disk = disk_enumerate(disk))) {
		if (!first) {
			error = SYSCTL_OUT(req, " ", 1);
			if (error)
				return error;
		} else {
			first = 0;
		}
		error = SYSCTL_OUT(req, disk->d_dev->si_name, strlen(disk->d_dev->si_name));
		if (error)
			return error;
	}
	error = SYSCTL_OUT(req, "", 1);
	return error;
}
 
SYSCTL_PROC(_kern, OID_AUTO, disks, CTLTYPE_STRING | CTLFLAG_RD, 0, NULL, 
    sysctl_disks, "A", "names of available disks");

/*
 * The cdevsw functions
 */

static int
diskopen(dev_t dev, int oflags, int devtype, struct thread *td)
{
	dev_t pdev;
	struct disk *dp;
	int error;

	error = 0;
	pdev = dkmodpart(dkmodslice(dev, WHOLE_DISK_SLICE), RAW_PART);

	dp = pdev->si_disk;
	if (!dp)
		return (ENXIO);

	while (dp->d_flags & DISKFLAG_LOCK) {
		dp->d_flags |= DISKFLAG_WANTED;
		error = tsleep(dp, PRIBIO | PCATCH, "diskopen", hz);
		if (error)
			return (error);
	}
	dp->d_flags |= DISKFLAG_LOCK;

	if (!dsisopen(dp->d_slice)) {
		if (!pdev->si_iosize_max)
			pdev->si_iosize_max = dev->si_iosize_max;
		error = dp->d_devsw->d_open(pdev, oflags, devtype, td);
	}

	/* Inherit properties from the whole/raw dev_t */
	inherit_raw(pdev, dev);

	if (error)
		goto out;
	
	error = dsopen(dev, devtype, dp->d_dsflags, &dp->d_slice, &dp->d_label);

	if (!dsisopen(dp->d_slice)) 
		dp->d_devsw->d_close(pdev, oflags, devtype, td);
out:	
	dp->d_flags &= ~DISKFLAG_LOCK;
	if (dp->d_flags & DISKFLAG_WANTED) {
		dp->d_flags &= ~DISKFLAG_WANTED;
		wakeup(dp);
	}
	
	return(error);
}

static int
diskclose(dev_t dev, int fflag, int devtype, struct thread *td)
{
	struct disk *dp;
	int error;
	dev_t pdev;

	error = 0;
	pdev = dkmodpart(dkmodslice(dev, WHOLE_DISK_SLICE), RAW_PART);
	dp = pdev->si_disk;
	if (!dp)
		return (ENXIO);
	dsclose(dev, devtype, dp->d_slice);
	if (!dsisopen(dp->d_slice))
		error = dp->d_devsw->d_close(dp->d_dev, fflag, devtype, td);
	return (error);
}

static void
diskstrategy(struct bio *bp)
{
	dev_t pdev;
	struct disk *dp;

	pdev = dkmodpart(dkmodslice(bp->bio_dev, WHOLE_DISK_SLICE), RAW_PART);
	dp = pdev->si_disk;
	bp->bio_resid = bp->bio_bcount;
	if (dp != bp->bio_dev->si_disk)
		inherit_raw(pdev, bp->bio_dev);

	if (!dp) {
		biofinish(bp, NULL, ENXIO);
		return;
	}

	if (dscheck(bp, dp->d_slice) <= 0) {
		biodone(bp);
		return;
	}

	if (bp->bio_bcount == 0) {
		biodone(bp);
		return;
	}

	KASSERT(dp->d_devsw != NULL, ("NULL devsw"));
	KASSERT(dp->d_devsw->d_strategy != NULL, ("NULL d_strategy"));
	dp->d_devsw->d_strategy(bp);
	return;
	
}

static int
diskioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct disk *dp;
	int error;
	u_int u;
	dev_t pdev;

	pdev = dkmodpart(dkmodslice(dev, WHOLE_DISK_SLICE), RAW_PART);
	dp = pdev->si_disk;
	if (!dp)
		return (ENXIO);
	if (cmd == DIOCGKERNELDUMP) {
		u = *(u_int *)data;
		return (diskdumpconf(u, dev, dp));
	}
	error = dsioctl(dev, cmd, data, fflag, &dp->d_slice);
	if (error == ENOIOCTL)
		error = dp->d_devsw->d_ioctl(dev, cmd, data, fflag, td);
	return (error);
}

static int
diskpsize(dev_t dev)
{
	struct disk *dp;
	dev_t pdev;

	pdev = dkmodpart(dkmodslice(dev, WHOLE_DISK_SLICE), RAW_PART);
	dp = pdev->si_disk;
	if (!dp)
		return (-1);
	if (dp != dev->si_disk) {
		dev->si_drv1 = pdev->si_drv1;
		dev->si_drv2 = pdev->si_drv2;
		/* XXX: don't set bp->b_dev->si_disk (?) */
	}
	return (dssize(dev, &dp->d_slice));
}

SYSCTL_INT(_debug_sizeof, OID_AUTO, disklabel, CTLFLAG_RD, 
    0, sizeof(struct disklabel), "sizeof(struct disklabel)");

SYSCTL_INT(_debug_sizeof, OID_AUTO, diskslices, CTLFLAG_RD, 
    0, sizeof(struct diskslices), "sizeof(struct diskslices)");

SYSCTL_INT(_debug_sizeof, OID_AUTO, disk, CTLFLAG_RD, 
    0, sizeof(struct disk), "sizeof(struct disk)");

#endif
