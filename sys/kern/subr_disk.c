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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stdint.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/diskslice.h>
#include <sys/disklabel.h>
#ifdef NO_GEOM
#include <sys/kernel.h>
#include <sys/sysctl.h>
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
	dp->d_label = malloc(sizeof *dp->d_label, M_DEVBUF, M_WAITOK|M_ZERO);

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
	if (di.mediasize == 0)
		return (EINVAL);
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
	free(dev->si_disk->d_label, M_DEVBUF);
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
 
SYSCTL_PROC(_kern, OID_AUTO, disks, CTLTYPE_STRING | CTLFLAG_RD, 0, 0, 
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
		dp->d_label->d_secsize = dp->d_sectorsize;
		dp->d_label->d_secperunit = dp->d_mediasize / dp->d_sectorsize;
		dp->d_label->d_nsectors = dp->d_fwsectors;
		dp->d_label->d_ntracks = dp->d_fwheads;
	}

	/* Inherit properties from the whole/raw dev_t */
	inherit_raw(pdev, dev);

	if (error)
		goto out;
	
	error = dsopen(dev, devtype, dp->d_dsflags, &dp->d_slice, dp->d_label);

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
	if (cmd == DIOCSKERNELDUMP) {
		u = *(u_int *)data;
		return (diskdumpconf(u, dev, dp));
	}
	if (cmd == DIOCGFRONTSTUFF) {
		*(off_t *)data = 8192;	/* XXX: crude but enough) */
		return (0);
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

#endif /* NO_GEOM */

/*-
 * Disk error is the preface to plaintive error messages
 * about failing disk transfers.  It prints messages of the form
 * 	"hp0g: BLABLABLA cmd=read fsbn 12345 of 12344-12347"
 * blkdone should be -1 if the position of the error is unknown.
 * The message is printed with printf.
 */
void
disk_err(struct bio *bp, const char *what, int blkdone, int nl)
{
	daddr_t sn;

	printf("%s: %s ", devtoname(bp->bio_dev), what);
	switch(bp->bio_cmd) {
	case BIO_READ:		printf("cmd=read "); break;
	case BIO_WRITE:		printf("cmd=write "); break;
	case BIO_DELETE:	printf("cmd=delete "); break;
	case BIO_GETATTR:	printf("cmd=getattr "); break;
	case BIO_SETATTR:	printf("cmd=setattr "); break;
	default:		printf("cmd=%x ", bp->bio_cmd); break;
	}
	sn = bp->bio_blkno;
	if (bp->bio_bcount <= DEV_BSIZE) {
		printf("fsbn %jd%s", (intmax_t)sn, nl ? "\n" : "");
		return;
	}
	if (blkdone >= 0) {
		sn += blkdone;
		printf("fsbn %jd of ", (intmax_t)sn);
	}
	printf("%jd-%jd", (intmax_t)bp->bio_blkno,
	    (intmax_t)(bp->bio_blkno + (bp->bio_bcount - 1) / DEV_BSIZE));
	if (nl)
		printf("\n");
}

#ifdef notquite
/*
 * Mutex to use when delaying niced I/O bound processes in bioq_disksort().
 */
static struct mtx dksort_mtx;
static void
dksort_init(void)
{

	mtx_init(&dksort_mtx, "dksort", NULL, MTX_DEF);
}
SYSINIT(dksort, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, dksort_init, NULL)
#endif

/*
 * Seek sort for disks.
 *
 * The buf_queue keep two queues, sorted in ascending block order.  The first
 * queue holds those requests which are positioned after the current block
 * (in the first request); the second, which starts at queue->switch_point,
 * holds requests which came in after their block number was passed.  Thus
 * we implement a one way scan, retracting after reaching the end of the drive
 * to the first request on the second queue, at which time it becomes the
 * first queue.
 *
 * A one-way scan is natural because of the way UNIX read-ahead blocks are
 * allocated.
 */

void
bioq_disksort(bioq, bp)
	struct bio_queue_head *bioq;
	struct bio *bp;
{
	struct bio *bq;
	struct bio *bn;
	struct bio *be;

#ifdef notquite
	struct thread *td = curthread;
	
	if (td && td->td_ksegrp->kg_nice > 0) {
		TAILQ_FOREACH(bn, &bioq->queue, bio_queue)
			if (BIOTOBUF(bp)->b_vp != BIOTOBUF(bn)->b_vp)
				break;
		if (bn != NULL) {
			mtx_lock(&dksort_mtx);
			msleep(&dksort_mtx, &dksort_mtx,
			    PPAUSE | PCATCH | PDROP, "ioslow",
			    td->td_ksegrp->kg_nice);
		}
	}
#endif
	if (!atomic_cmpset_int(&bioq->busy, 0, 1))
		panic("Recursing in bioq_disksort()");
	be = TAILQ_LAST(&bioq->queue, bio_queue);
	/*
	 * If the queue is empty or we are an
	 * ordered transaction, then it's easy.
	 */
	if ((bq = bioq_first(bioq)) == NULL) {
		bioq_insert_tail(bioq, bp);
		bioq->busy = 0;
		return;
	} else if (bioq->insert_point != NULL) {

		/*
		 * A certain portion of the list is
		 * "locked" to preserve ordering, so
		 * we can only insert after the insert
		 * point.
		 */
		bq = bioq->insert_point;
	} else {

		/*
		 * If we lie before the last removed (currently active)
		 * request, and are not inserting ourselves into the
		 * "locked" portion of the list, then we must add ourselves
		 * to the second request list.
		 */
		if (bp->bio_pblkno < bioq->last_pblkno) {

			bq = bioq->switch_point;
			/*
			 * If we are starting a new secondary list,
			 * then it's easy.
			 */
			if (bq == NULL) {
				bioq->switch_point = bp;
				bioq_insert_tail(bioq, bp);
				bioq->busy = 0;
				return;
			}
			/*
			 * If we lie ahead of the current switch point,
			 * insert us before the switch point and move
			 * the switch point.
			 */
			if (bp->bio_pblkno < bq->bio_pblkno) {
				bioq->switch_point = bp;
				TAILQ_INSERT_BEFORE(bq, bp, bio_queue);
				bioq->busy = 0;
				return;
			}
		} else {
			if (bioq->switch_point != NULL)
				be = TAILQ_PREV(bioq->switch_point,
						bio_queue, bio_queue);
			/*
			 * If we lie between last_pblkno and bq,
			 * insert before bq.
			 */
			if (bp->bio_pblkno < bq->bio_pblkno) {
				TAILQ_INSERT_BEFORE(bq, bp, bio_queue);
				bioq->busy = 0;
				return;
			}
		}
	}

	/*
	 * Request is at/after our current position in the list.
	 * Optimize for sequential I/O by seeing if we go at the tail.
	 */
	if (bp->bio_pblkno > be->bio_pblkno) {
		TAILQ_INSERT_AFTER(&bioq->queue, be, bp, bio_queue);
		bioq->busy = 0;
		return;
	}

	/* Otherwise, insertion sort */
	while ((bn = TAILQ_NEXT(bq, bio_queue)) != NULL) {
		
		/*
		 * We want to go after the current request if it is the end
		 * of the first request list, or if the next request is a
		 * larger cylinder than our request.
		 */
		if (bn == bioq->switch_point
		 || bp->bio_pblkno < bn->bio_pblkno)
			break;
		bq = bn;
	}
	TAILQ_INSERT_AFTER(&bioq->queue, bq, bp, bio_queue);
	bioq->busy = 0;
}


