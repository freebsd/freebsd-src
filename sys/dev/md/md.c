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

#include "opt_mfs.h"		/* We have adopted some tasks from MFS */
#include "opt_md.h"
#include "opt_devfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/linker.h>
#include <sys/queue.h>

#ifdef DEVFS
#include <sys/eventhandler.h>
#include <fs/devfs/devfs.h>
#endif

#ifndef MD_NSECT
#define MD_NSECT (10000 * 2)
#endif

MALLOC_DEFINE(M_MD, "MD disk", "Memory Disk");
MALLOC_DEFINE(M_MDSECT, "MD sectors", "Memory Disk Sectors");

static int md_debug;
SYSCTL_INT(_debug, OID_AUTO, mddebug, CTLFLAG_RW, &md_debug, 0, "");

#if defined(MFS_ROOT) && !defined(MD_ROOT)
#define MD_ROOT MFS_ROOT
#warning "option MFS_ROOT has been superceeded by MD_ROOT"
#endif

#if defined(MFS_ROOT_SIZE) && !defined(MD_ROOT_SIZE)
#define MD_ROOT_SIZE MFS_ROOT_SIZE
#warning "option MFS_ROOT_SIZE has been superceeded by MD_ROOT_SIZE"
#endif

#if defined(MD_ROOT) && defined(MD_ROOT_SIZE)
/* Image gets put here: */
static u_char mfs_root[MD_ROOT_SIZE*1024] = "MFS Filesystem goes here";
static u_char end_mfs_root[] __unused = "MFS Filesystem had better STOP here";
#endif

static int mdrootready;

static void mdcreate_malloc(int unit);

#define CDEV_MAJOR	95
#define BDEV_MAJOR	22

static d_strategy_t mdstrategy;
static d_strategy_t mdstrategy_preload;
static d_strategy_t mdstrategy_malloc;
static d_open_t mdopen;
static d_ioctl_t mdioctl;

static struct cdevsw md_cdevsw = {
        /* open */      mdopen,
        /* close */     nullclose,
        /* read */      physread,
        /* write */     physwrite,
        /* ioctl */     mdioctl,
        /* poll */      nopoll,
        /* mmap */      nommap,
        /* strategy */  mdstrategy,
        /* name */      "md",
        /* maj */       CDEV_MAJOR,
        /* dump */      nodump,
        /* psize */     nopsize,
        /* flags */     D_DISK | D_CANFREE | D_MEMDISK,
        /* bmaj */      BDEV_MAJOR
};

static struct cdevsw mddisk_cdevsw;

static LIST_HEAD(, md_s) md_softc_list = LIST_HEAD_INITIALIZER(&md_softc_list);

struct md_s {
	int unit;
	LIST_ENTRY(md_s) list;
	struct devstat stats;
	struct bio_queue_head bio_queue;
	struct disk disk;
	dev_t dev;
	int busy;
	enum {MD_MALLOC, MD_PRELOAD} type;
	unsigned nsect;

	/* MD_MALLOC related fields */
	unsigned nsecp;
	u_char **secp;

	/* MD_PRELOAD related fields */
	u_char *pl_ptr;
	unsigned pl_len;
};

static int mdunits;

static int
mdopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct md_s *sc;
	struct disklabel *dl;

	if (md_debug)
		printf("mdopen(%s %x %x %p)\n",
			devtoname(dev), flag, fmt, p);

	sc = dev->si_drv1;
#ifndef DEVFS
	if (sc->unit + 1 == mdunits)
		mdcreate_malloc(-1);
#endif

	dl = &sc->disk.d_label;
	bzero(dl, sizeof(*dl));
	dl->d_secsize = DEV_BSIZE;
	dl->d_nsectors = 1024;
	dl->d_ntracks = 1;
	dl->d_secpercyl = dl->d_nsectors * dl->d_ntracks;
	dl->d_secperunit = sc->nsect;
	dl->d_ncylinders = dl->d_secperunit / dl->d_secpercyl;
	return (0);
}

static int
mdioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{

	if (md_debug)
		printf("mdioctl(%s %lx %p %x %p)\n",
			devtoname(dev), cmd, addr, flags, p);

	return (ENOIOCTL);
}

static void
mdstrategy(struct bio *bp)
{
	struct md_s *sc;

	if (md_debug > 1)
		printf("mdstrategy(%p) %s %x, %d, %ld, %p)\n",
		    bp, devtoname(bp->bio_dev), bp->bio_flags, bp->bio_blkno, 
		    bp->bio_bcount / DEV_BSIZE, bp->bio_data);

	sc = bp->bio_dev->si_drv1;
	if (sc->type == MD_MALLOC) {
		mdstrategy_malloc(bp);
	} else {
		mdstrategy_preload(bp);
	}
	return;
}


static void
mdstrategy_malloc(struct bio *bp)
{
	int s, i;
	struct md_s *sc;
	devstat_trans_flags dop;
	u_char *secp, **secpp, *dst;
	unsigned secno, nsec, secval, uc;

	if (md_debug > 1)
		printf("mdstrategy_malloc(%p) %s %x, %d, %ld, %p)\n",
		    bp, devtoname(bp->bio_dev), bp->bio_flags, bp->bio_blkno, 
		    bp->bio_bcount / DEV_BSIZE, bp->bio_data);

	sc = bp->bio_dev->si_drv1;

	s = splbio();

	bioqdisksort(&sc->bio_queue, bp);

	if (sc->busy) {
		splx(s);
		return;
	}

	sc->busy++;
	
	while (1) {
		bp = bioq_first(&sc->bio_queue);
		if (bp)
			bioq_remove(&sc->bio_queue, bp);
		splx(s);
		if (!bp)
			break;

		devstat_start_transaction(&sc->stats);

		if (bp->bio_cmd == BIO_DELETE)
			dop = DEVSTAT_NO_DATA;
		else if (bp->bio_cmd == BIO_READ)
			dop = DEVSTAT_READ;
		else
			dop = DEVSTAT_WRITE;

		nsec = bp->bio_bcount / DEV_BSIZE;
		secno = bp->bio_pblkno;
		dst = bp->bio_data;
		while (nsec--) {

			if (secno < sc->nsecp) {
				secpp = &sc->secp[secno];
				if ((u_int)*secpp > 255) {
					secp = *secpp;
					secval = 0;
				} else {
					secp = 0;
					secval = (u_int) *secpp;
				}
			} else {
				secpp = 0;
				secp = 0;
				secval = 0;
			}
			if (md_debug > 2)
				printf("%x %p %p %d\n", 
				    bp->bio_flags, secpp, secp, secval);

			if (bp->bio_cmd == BIO_DELETE) {
				if (secpp) {
					if (secp)
						FREE(secp, M_MDSECT);
					*secpp = 0;
				}
			} else if (bp->bio_cmd == BIO_READ) {
				if (secp) {
					bcopy(secp, dst, DEV_BSIZE);
				} else if (secval) {
					for (i = 0; i < DEV_BSIZE; i++)
						dst[i] = secval;
				} else {
					bzero(dst, DEV_BSIZE);
				}
			} else {
				uc = dst[0];
				for (i = 1; i < DEV_BSIZE; i++) 
					if (dst[i] != uc)
						break;
				if (i == DEV_BSIZE && !uc) {
					if (secp)
						FREE(secp, M_MDSECT);
					if (secpp)
						*secpp = (u_char *)uc;
				} else {
					if (!secpp) {
						MALLOC(secpp, u_char **, (secno + nsec + 1) * sizeof(u_char *), M_MD, M_WAITOK);
						bzero(secpp, (secno + nsec + 1) * sizeof(u_char *));
						bcopy(sc->secp, secpp, sc->nsecp * sizeof(u_char *));
						FREE(sc->secp, M_MD);
						sc->secp = secpp;
						sc->nsecp = secno + nsec + 1;
						secpp = &sc->secp[secno];
					}
					if (i == DEV_BSIZE) {
						if (secp)
							FREE(secp, M_MDSECT);
						*secpp = (u_char *)uc;
					} else {
						if (!secp) 
							MALLOC(secp, u_char *, DEV_BSIZE, M_MDSECT, M_WAITOK);
						bcopy(dst, secp, DEV_BSIZE);

						*secpp = secp;
					}
				}
			}
			secno++;
			dst += DEV_BSIZE;
		}
		bp->bio_resid = 0;
		devstat_end_transaction_bio(&sc->stats, bp);
		biodone(bp);
		s = splbio();
	}
	sc->busy = 0;
	return;
}


static void
mdstrategy_preload(struct bio *bp)
{
	int s;
	struct md_s *sc;
	devstat_trans_flags dop;

	if (md_debug > 1)
		printf("mdstrategy_preload(%p) %s %x, %d, %ld, %p)\n",
		    bp, devtoname(bp->bio_dev), bp->bio_flags, bp->bio_blkno, 
		    bp->bio_bcount / DEV_BSIZE, bp->bio_data);

	sc = bp->bio_dev->si_drv1;

	s = splbio();

	bioqdisksort(&sc->bio_queue, bp);

	if (sc->busy) {
		splx(s);
		return;
	}

	sc->busy++;
	
	while (1) {
		bp = bioq_first(&sc->bio_queue);
		if (bp)
			bioq_remove(&sc->bio_queue, bp);
		splx(s);
		if (!bp)
			break;

		devstat_start_transaction(&sc->stats);

		if (bp->bio_cmd == BIO_DELETE) {
			dop = DEVSTAT_NO_DATA;
		} else if (bp->bio_cmd == BIO_READ) {
			dop = DEVSTAT_READ;
			bcopy(sc->pl_ptr + (bp->bio_pblkno << DEV_BSHIFT), bp->bio_data, bp->bio_bcount);
		} else {
			dop = DEVSTAT_WRITE;
			bcopy(bp->bio_data, sc->pl_ptr + (bp->bio_pblkno << DEV_BSHIFT), bp->bio_bcount);
		}
		bp->bio_resid = 0;
		devstat_end_transaction_bio(&sc->stats, bp);
		biodone(bp);
		s = splbio();
	}
	sc->busy = 0;
	return;
}

static struct md_s *
mdcreate(int unit)
{
	struct md_s *sc;

	if (unit == -1)
		unit = mdunits++;
	/* Make sure this unit isn't already in action */
	LIST_FOREACH(sc, &md_softc_list, list) {
		if (sc->unit == unit)
			return (NULL);
	}
	MALLOC(sc, struct md_s *,sizeof(*sc), M_MD, M_WAITOK);
	bzero(sc, sizeof(*sc));
	LIST_INSERT_HEAD(&md_softc_list, sc, list);
	sc->unit = unit;
	bioq_init(&sc->bio_queue);
	devstat_add_entry(&sc->stats, "md", sc->unit, DEV_BSIZE,
		DEVSTAT_NO_ORDERED_TAGS, 
		DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_OTHER,
		DEVSTAT_PRIORITY_OTHER);
	sc->dev = disk_create(sc->unit, &sc->disk, 0, &md_cdevsw, &mddisk_cdevsw);
	sc->dev->si_drv1 = sc;
	return (sc);
}

static void
mdcreate_preload(u_char *image, unsigned length)
{
	struct md_s *sc;

	sc = mdcreate(-1);
	sc->type = MD_PRELOAD;
	sc->nsect = length / DEV_BSIZE;
	sc->pl_ptr = image;
	sc->pl_len = length;

	if (sc->unit == 0) 
		mdrootready = 1;
}

static void
mdcreate_malloc(int unit)
{
	struct md_s *sc;

	sc = mdcreate(unit);
	if (sc == NULL)
		return;

	sc->type = MD_MALLOC;

	sc->nsect = MD_NSECT;	/* for now */
	MALLOC(sc->secp, u_char **, sizeof(u_char *), M_MD, M_WAITOK);
	bzero(sc->secp, sizeof(u_char *));
	sc->nsecp = 1;
	printf("md%d: Malloc disk\n", sc->unit);
}

#ifdef DEVFS
static void
md_clone (void *arg, char *name, int namelen, dev_t *dev)
{
	int i, u;

	if (*dev != NODEV)
		return;
	i = devfs_stdclone(name, NULL, "md", &u);
	if (i == 0)
		return;
	/* XXX: should check that next char is [\0sa-h] */
	/*
	 * Now we cheat: We just create the disk, but don't match.
	 * Since we run before it, subr_disk.c::disk_clone() will
	 * find our disk and match the sought for device.
	 */
	mdcreate_malloc(u);
	return;
}
#endif

static void
md_drvinit(void *unused)
{

	caddr_t mod;
	caddr_t c;
	u_char *ptr, *name, *type;
	unsigned len;

#ifdef MD_ROOT_SIZE
	mdcreate_preload(mfs_root, MD_ROOT_SIZE*1024);
#endif
	mod = NULL;
	while ((mod = preload_search_next_name(mod)) != NULL) {
		name = (char *)preload_search_info(mod, MODINFO_NAME);
		type = (char *)preload_search_info(mod, MODINFO_TYPE);
		if (name == NULL)
			continue;
		if (type == NULL)
			continue;
		if (strcmp(type, "md_image") && strcmp(type, "mfs_root"))
			continue;
		c = preload_search_info(mod, MODINFO_ADDR);
		ptr = *(u_char **)c;
		c = preload_search_info(mod, MODINFO_SIZE);
		len = *(unsigned *)c;
		printf("md%d: Preloaded image <%s> %d bytes at %p\n",
		   mdunits, name, len, ptr);
		mdcreate_preload(ptr, len);
	} 
#ifdef DEVFS
	EVENTHANDLER_REGISTER(devfs_clone, md_clone, 0, 999);
#else
	mdcreate_malloc(-1);
#endif
}

SYSINIT(mddev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR, md_drvinit,NULL)

#ifdef MD_ROOT
static void
md_takeroot(void *junk)
{
	if (mdrootready)
		rootdevnames[0] = "ufs:/dev/md0c";
}

SYSINIT(md_root, SI_SUB_MOUNT_ROOT, SI_ORDER_FIRST, md_takeroot, NULL);
#endif

