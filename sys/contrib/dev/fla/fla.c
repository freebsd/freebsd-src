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

#include "fla.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/devicestat.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <sys/bus.h>
#include <isa/isareg.h>
#include <isa/isavar.h>

#ifdef SMP
#include <machine/smp.h>
#endif

#include <contrib/dev/fla/msysosak.h>

MALLOC_DEFINE(M_FLA, "fla driver", "fla driver storage");

static int fla_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, fladebug, CTLFLAG_RW, &fla_debug, 0, "");

#define CDEV_MAJOR	102
#define BDEV_MAJOR	28

static d_strategy_t flastrategy;
static d_open_t flaopen;
static d_close_t flaclose;
static d_ioctl_t flaioctl;
static d_psize_t flapsize;

static struct cdevsw fla_cdevsw = {
        /* open */      flaopen,
        /* close */     flaclose,
        /* read */      physread,
        /* write */     physwrite,
        /* ioctl */     flaioctl,
        /* stop */      nostop,
        /* reset */     noreset,
        /* devtotty */  nodevtotty,
        /* poll */      nopoll,
        /* mmap */      nommap,
        /* strategy */  flastrategy,
        /* name */      "fla",
        /* parms */     noparms,
        /* maj */       CDEV_MAJOR,
        /* dump */      nodump,
        /* psize */     flapsize,
        /* flags */     D_DISK | D_CANFREE,
        /* maxio */     0,
        /* bmaj */      BDEV_MAJOR
};

void *
doc2k_malloc(int bytes) 
{
	return malloc(bytes, M_FLA, M_WAITOK);
}

void
doc2k_free(void *ptr)
{
	free(ptr, M_FLA);
}

void
doc2k_delay(unsigned msec)
{
	DELAY(1000 * msec);
}

void
doc2k_memcpy(void *dst, const void *src, unsigned len)
{
	bcopy(src, dst, len);
}

int
doc2k_memcmp(const void *dst, const void *src, unsigned len)
{
	return (bcmp(src, dst, len));
}

void
doc2k_memset(void *dst, int c, unsigned len)
{
	u_char *p = dst;
	while (len--)
		*p++ = c;
}

static struct fla_s {
	unsigned nsect;
	struct doc2k_stat ds;
	struct diskslices *dk_slices;
	struct buf_queue_head buf_queue;
	struct devstat stats;
	int busy;
} softc[NFLA];

static int
flaopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	int unit;
	struct fla_s *sc;
	int error;
	struct disklabel dk_dd;

	if (fla_debug)
		printf("flaopen(%x %x %x %p)\n",
			dev2udev(dev), flag, fmt, p);
	unit = dkunit(dev);
	if (unit >= NFLA)
		return (ENXIO);
	sc = &softc[unit];
	if (!sc->nsect)
		return (ENXIO);
	bzero(&dk_dd, sizeof(dk_dd));
	error = doc2k_size(unit, &dk_dd.d_secperunit,
	    &dk_dd.d_ncylinders, &dk_dd.d_ntracks, &dk_dd.d_nsectors);
	dk_dd.d_secsize = DEV_BSIZE;
	dk_dd.d_secpercyl = dk_dd.d_ntracks * dk_dd.d_nsectors;

	error = dsopen(dev, fmt, 0, &sc->dk_slices, &dk_dd);
	if (error)
		return (error);
	return (0);
}

static int
flaclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	int unit;
	struct fla_s *sc;

	if (fla_debug)
		printf("flaclose(%x %x %x %p)\n",
			dev2udev(dev), flags, fmt, p);
	unit = dkunit(dev);
	sc = &softc[unit];
	dsclose(dev, fmt, sc->dk_slices);
	return (0);
}

static int
flaioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	int unit, error;
	struct fla_s *sc;

	if (fla_debug)
		printf("flaioctl(%x %lx %p %x %p)\n",
			dev2udev(dev), cmd, addr, flags, p);
	unit = dkunit(dev);
	sc = &softc[unit];
	error = dsioctl(dev, cmd, addr, flags, &sc->dk_slices);
	if (error == ENOIOCTL)
		error = ENOTTY;
	return (error);
}

static void
flastrategy(struct buf *bp)
{
	int unit, error;
	int s;
	struct fla_s *sc;
	enum doc2k_work what;
	devstat_trans_flags dop;

	if (fla_debug > 1)
		printf("flastrategy(%p) %x %lx, %d, %ld, %p)\n",
			    bp, dev2udev(bp->b_dev), bp->b_flags, bp->b_blkno, 
			    bp->b_bcount / DEV_BSIZE, bp->b_data);
	unit = dkunit(bp->b_dev);
	sc = &softc[unit];

	if (dscheck(bp, sc->dk_slices) <= 0) {
		biodone(bp);
		return;
	}

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
		bp->b_resid = bp->b_bcount;
		unit = dkunit(bp->b_dev);

		if (bp->b_flags & B_FREEBUF)
			what = DOC2K_ERASE, dop = DEVSTAT_NO_DATA;
		else if (bp->b_flags & B_READ)
			what = DOC2K_READ, dop = DEVSTAT_READ;
		else 
			what = DOC2K_WRITE, dop = DEVSTAT_WRITE;
#ifdef SMP
		rel_mplock();			
#endif
		error = doc2k_rwe( unit, what, bp->b_pblkno,
		    bp->b_bcount / DEV_BSIZE, bp->b_data);
#ifdef SMP
		get_mplock();			
#endif

		if (fla_debug > 1 || error) {
			printf("fla%d: %d = rwe(%p, %d, %d, %d, %ld, %p)\n",
			    unit, error, bp, unit, what, bp->b_pblkno, 
			    bp->b_bcount / DEV_BSIZE, bp->b_data);
		}
		if (error) {
			bp->b_error = EIO;
			bp->b_flags |= B_ERROR;
		} else {
			bp->b_resid = 0;
		}
		biodone(bp);
		devstat_end_transaction(&sc->stats, bp->b_bcount,
		    DEVSTAT_TAG_NONE, dop);


		s = splbio();
	}
	sc->busy = 0;
	return;
}

static int
flapsize(dev_t dev)
{
	int unit;
	struct fla_s *sc;

	if (fla_debug)
		printf("flapsize(%x)\n", dev2udev(dev));
	unit = dkunit(dev);
	sc = &softc[unit];
	if (!sc->nsect)
		return 0;

	return (dssize(dev, &sc->dk_slices));
}

static int
flarealprobe(int unit)
{
	struct fla_s *sc;
	int i;

	sc = &softc[unit];

	/* This is slightly ugly */
	i = doc2k_probe(unit, KERNBASE + 0xc0000, KERNBASE + 0xe0000);
	if (i)
		return (ENXIO);

	i = doc2k_info(unit, &sc->ds);
	if (i)
		return (ENXIO);

	return (0);
}


static int
flarealattach(int unit)
{
	int i, j, k, l, m, error;
	struct fla_s *sc;

	sc = &softc[unit];

	error = doc2k_open(unit);
	if (error) {
		printf("doc2k_open(%d) -> err %d\n", unit, error);
		return (EIO);
	}

	error = doc2k_size(unit, &sc->nsect, &i, &j, &k );
	if (error) {
		printf("doc2k_size(%d) -> err %d\n", unit, error);
		return (EIO);
	}

	printf("fla%d: <%s %s>\n", unit, sc->ds.product, sc->ds.model);
	
	m = 1024L * 1024L / DEV_BSIZE;
	l = (sc->nsect * 10 + m/2) / m;
	printf("fla%d: %d.%01dMB (%u sectors),"
	    " %d cyls, %d heads, %d S/T, 512 B/S\n",
	    unit, l / 10, l % 10, sc->nsect, i, j, k);

	if (bootverbose)
		printf("fla%d: JEDEC=0x%x unitsize=%ld mediasize=%ld"
		       " chipsize=%ld interleave=%d window=%lx\n", 
		    unit, sc->ds.type, sc->ds.unitSize, sc->ds.mediaSize, 
		    sc->ds.chipSize, sc->ds.interleaving, sc->ds.window);

	bufq_init(&sc->buf_queue);

	return (0);
}

static int
flaprobe (device_t dev)
{
	int unit;
	struct fla_s *sc;
	device_t bus;
	int i;

	unit = device_get_unit(dev);
	sc = &softc[unit];
	i = flarealprobe(unit);
	if (i)
		return (i);

	bus = device_get_parent(dev);
	ISA_SET_RESOURCE(bus, dev, SYS_RES_MEMORY, 0, 
		sc->ds.window - KERNBASE, 8192);

	return (0);
}


static int
flaattach (device_t dev)
{
	int unit, i;

	unit = device_get_unit(dev);
	i = flarealattach(unit);
	devstat_add_entry(&softc[unit].stats, "fla", unit, DEV_BSIZE,
		DEVSTAT_NO_ORDERED_TAGS, 
		DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_OTHER, 0x190);
	return (i);
}

static device_method_t fla_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		flaprobe),
	DEVMETHOD(device_attach,	flaattach),
	{0, 0}
};

static driver_t fladriver = {
	"fla",
	fla_methods,
	sizeof(struct fla_s),
};

static devclass_t	fla_devclass;

DEV_DRIVER_MODULE(fla, isa, fladriver, fla_devclass, fla_cdevsw, 0, 0);
