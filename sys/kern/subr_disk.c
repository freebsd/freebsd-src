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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/malloc.h>

MALLOC_DEFINE(M_DISK, "disk", "disk data");

static d_strategy_t diskstrategy;
static d_open_t diskopen;
static d_close_t diskclose; 
static d_ioctl_t diskioctl;
static d_psize_t diskpsize;
 
static struct cdevsw disk_cdevsw = {
	/* open */      diskopen,
	/* close */     diskclose,
	/* read */      physread,
	/* write */     physwrite,
	/* ioctl */     diskioctl,
	/* stop */      nostop,
	/* reset */     noreset,
	/* devtotty */  nodevtotty,
	/* poll */      nopoll,
	/* mmap */      nommap,
	/* strategy */  diskstrategy,
	/* name */      "disk",
	/* parms */     noparms,
	/* maj */       -1,
	/* dump */      nodump,
	/* psize */     diskpsize,
	/* flags */     D_DISK,
	/* maxio */     0,
	/* bmaj */      -1,
};      

dev_t
disk_create(int unit, struct disk *dp, struct cdevsw *cdevsw)
{
	dev_t dev;
	struct cdevsw *cds;

	dp->d_devsw = cdevsw;
	dev = makedev(cdevsw->d_maj, 0);	
	cds = devsw(dev);
	if (!cds) {
		/* Build the "real" cdevsw */
		MALLOC(cds, struct cdevsw *, sizeof(*cds), M_DISK, M_WAITOK);
		*cds = disk_cdevsw;
		cds->d_name = dp->d_devsw->d_name;
		cds->d_maj = dp->d_devsw->d_maj;
		cds->d_bmaj = dp->d_devsw->d_bmaj;
		cds->d_flags = dp->d_devsw->d_flags;

		cdevsw_add(cds);
	}

	printf("Creating DISK %s%d\n", cds->d_name, unit);
	dev = make_dev(cds, dkmakeminor(unit, WHOLE_DISK_SLICE, RAW_PART),
	    0, 0, 0, "r%s%d", cds->d_name, unit);

	dev->si_disk = dp;
	dp->d_dev = dev;
	return (dev);
}

void
disk_delete(dev_t dev)
{
	return;
}

static int
diskopen(dev_t dev, int oflags, int devtype, struct proc *p)
{
	dev_t pdev;
	struct disk *dp;
	int error;

	pdev = dkmodpart(dkmodslice(dev, WHOLE_DISK_SLICE), RAW_PART);
	dp = pdev->si_disk;
	dev->si_disk = pdev->si_disk;
	dev->si_drv1 = pdev->si_drv1;
	dev->si_drv2 = pdev->si_drv2;
	if (!dp)
		return (ENXIO);
	dev->si_disk = dp;
	if (!dp->d_opencount++) {
		error = dp->d_devsw->d_open(dev, oflags, devtype, p);
		if (error)
			return(error);
	}

	error = dsopen(dev, devtype, 0, &dp->d_slice, &dp->d_label);

	if (error && !--dp->d_opencount)
		dp->d_devsw->d_close(dev, oflags, devtype, p);
	return(error);
}

static int
diskclose(dev_t dev, int fflag, int devtype, struct proc *p)
{
	struct disk *dp;
	int error;

	error = 0;
	dp = dev->si_disk;
	dsclose(dev, devtype, dp->d_slice);
	if (!--dp->d_opencount)
		error = dp->d_devsw->d_close(dev, fflag, devtype, p);
	return (error);
}

static void
diskstrategy(struct buf *bp)
{
	dev_t pdev;
	struct disk *dp;

	dp = bp->b_dev->si_disk;
	if (!dp) {
		pdev = dkmodpart(dkmodslice(bp->b_dev, WHOLE_DISK_SLICE), RAW_PART);
		dp = pdev->si_disk;
		bp->b_dev->si_drv1 = pdev->si_drv1;
		bp->b_dev->si_drv2 = pdev->si_drv2;
		/* XXX: don't set bp->b_dev->si_disk (?) */
	} else {
		pdev = dp->d_dev;
	}

	if (!dp) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}

	if (dscheck(bp, dp->d_slice) < 0) {
		biodone(bp);
		return;
	}

	dp->d_devsw->d_strategy(bp);
	return;
	
}

static int
diskioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc *p)
{
	struct disk *dp;
	int error;

	dp = dev->si_disk;
	error = dsioctl(dev, cmd, data, fflag, &dp->d_slice);
	if (error == ENOIOCTL)
		error = dp->d_devsw->d_ioctl(dev, cmd, data, fflag, p);
	return (error);
}

static int
diskpsize(dev_t dev)
{
	struct disk *dp;

	dp = dev->si_disk;
	return (dssize(dev, &dp->d_slice));
}
