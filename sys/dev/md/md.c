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
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <sys/bus.h>
#include <isa/isareg.h>
#include <isa/isavar.h>

MALLOC_DEFINE(M_MD, "MD disk", "Memory Disk");
MALLOC_DEFINE(M_MDSECT, "MD sectors", "Memory Disk Sectors");

static int md_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, mddebug, CTLFLAG_RW, &md_debug, 0, "");

#define CDEV_MAJOR	95
#define BDEV_MAJOR	22

static d_strategy_t mdstrategy;
static d_open_t mdopen;
static d_ioctl_t mdioctl;

static struct cdevsw md_cdevsw = {
        /* open */      mdopen,
        /* close */     nullclose,
        /* read */      physread,
        /* write */     physwrite,
        /* ioctl */     mdioctl,
        /* stop */      nostop,
        /* reset */     noreset,
        /* devtotty */  nodevtotty,
        /* poll */      nopoll,
        /* mmap */      nommap,
        /* strategy */  mdstrategy,
        /* name */      "md",
        /* parms */     noparms,
        /* maj */       CDEV_MAJOR,
        /* dump */      nodump,
        /* psize */     nopsize,
        /* flags */     D_DISK | D_CANFREE,
        /* maxio */     0,
        /* bmaj */      BDEV_MAJOR
};
static struct cdevsw mddisk_cdevsw;

struct md_s {
	int unit;
	struct devstat stats;
	struct buf_queue_head buf_queue;
	struct disk disk;
	dev_t dev;
	unsigned nsect;
	unsigned nsecp;
	u_char **secp;

	int busy;
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

	dl = &sc->disk.d_label;
	bzero(dl, sizeof(*dl));
	dl->d_secsize = DEV_BSIZE;
	dl->d_nsectors = 1024;
	dl->d_ntracks = 1;
	dl->d_secpercyl = dl->d_nsectors + dl->d_ntracks;
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
mdstrategy(struct buf *bp)
{
	int s, i;
	struct md_s *sc;
	devstat_trans_flags dop;
	u_char *secp, **secpp, *dst;
	unsigned secno, nsec, secval, uc;

	if (md_debug > 1)
		printf("mdstrategy(%p) %s %lx, %d, %ld, %p)\n",
		    bp, devtoname(bp->b_dev), bp->b_flags, bp->b_blkno, 
		    bp->b_bcount / DEV_BSIZE, bp->b_data);

	sc = bp->b_dev->si_drv1;

	s = splbio();

	bufqdisksort(&sc->buf_queue, bp);

	if (sc->busy) {
		splx(s);
		return;
	}

	sc->busy++;
	
	while (1) {
		bp = bufq_first(&sc->buf_queue);
		if (bp)
			bufq_remove(&sc->buf_queue, bp);
		splx(s);
		if (!bp)
			break;

		devstat_start_transaction(&sc->stats);

		if (bp->b_flags & B_FREEBUF) 
			dop = DEVSTAT_NO_DATA;
		else if (bp->b_flags & B_READ)
			dop = DEVSTAT_READ;
		else
			dop = DEVSTAT_WRITE;

		nsec = bp->b_bcount / DEV_BSIZE;
		secno = bp->b_pblkno;
		dst = bp->b_data;
		while (nsec--) {

			if (secno < sc->nsecp) {
				secpp = &sc->secp[secno];
				if ((u_int)secpp > 255) {
					secp = *secpp;
					secval = 0;
				} else {
					secp = 0;
					secval = (u_int) secpp;
				}
			} else {
				secpp = 0;
				secp = 0;
				secval = 0;
			}

			if (bp->b_flags & B_FREEBUF) {
				if (secpp) {
					if (secp)
						FREE(secp, M_MDSECT);
					*secpp = 0;
				}
			} else if (bp->b_flags & B_READ) {
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

		bp->b_resid = 0;
		biodone(bp);
		devstat_end_transaction(&sc->stats, bp->b_bcount,
		    DEVSTAT_TAG_NONE, dop);

		s = splbio();
	}
	sc->busy = 0;
	return;
}

static dev_t
mdcreate(void)
{
	struct md_s *sc;

	MALLOC(sc, struct md_s *,sizeof(*sc), M_MD, M_WAITOK);
	bzero(sc, sizeof(*sc));
	sc->unit = mdunits++;

	bufq_init(&sc->buf_queue);

	devstat_add_entry(&sc->stats, "md", sc->unit, DEV_BSIZE,
		DEVSTAT_NO_ORDERED_TAGS, 
		DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_OTHER, 0x190);

	sc->dev = disk_create(sc->unit, &sc->disk, 0, 
	    &md_cdevsw, &mddisk_cdevsw);

	sc->dev->si_drv1 = sc;
	sc->nsect = 10000 * 2;	/* for now */
	MALLOC(sc->secp, u_char **, sizeof(u_char *), M_MD, M_WAITOK);
	bzero(sc->secp, sizeof(u_char *));
	sc->nsecp = 1;

	return (0);
}

static void
md_drvinit(void *unused)
{

	mdcreate();
}

SYSINIT(ptcdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR, md_drvinit,NULL)

