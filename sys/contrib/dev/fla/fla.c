/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: fla.c,v 1.1 1999/08/06 15:59:07 phk Exp $ 
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
#include <sys/module.h>
#include <sys/conf.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#if __FreeBSD_version > 400000	/* XXX ? */
#include <sys/bus.h>
#include <isa/isareg.h>
#include <isa/isavar.h>

#define dev2ul(foo)	((unsigned long)dev2udev(foo))
#endif

#if __FreeBSD_version < 400000	/* XXX ? */
#include <i386/isa/isa_device.h>

#define dev2ul(foo)	((unsigned long)foo)

static int
physread(dev_t dev, struct uio *uio, int ioflag)
{
	return(physio(cdevsw[major(dev)]->d_strategy, 
	    NULL, dev, 1, minphys, uio));
}

static int
physwrite(dev_t dev, struct uio *uio, int ioflag)
{
	return(physio(cdevsw[major(dev)]->d_strategy,
	    NULL, dev, 0, minphys, uio));
}

#define nopoll	seltrue
#define noparms	NULL
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
} softc[NFLA];

static int
flaopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	int unit;
	struct fla_s *sc;
	int error;
	struct disklabel dk_dd;

	if (fla_debug)
		printf("flaopen(%lx %x %x %p)\n",
			dev2ul(dev), flag, fmt, p);
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

	error = dsopen("fla", dev, fmt, 0, &sc->dk_slices, &dk_dd, 
	    flastrategy, NULL, &fla_cdevsw);
	return (error);
}

static int
flaclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	int unit;
	struct fla_s *sc;

	if (fla_debug)
		printf("flaclose(%lx %x %x %p)\n",
			dev2ul(dev), flags, fmt, p);
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
		printf("flaioctl(%lx %lx %p %x %p)\n",
			dev2ul(dev), cmd, addr, flags, p);
	unit = dkunit(dev);
	sc = &softc[unit];
	error = dsioctl("fla", dev, cmd, addr, flags, &sc->dk_slices,
	    flastrategy, NULL);
	if (error == ENOIOCTL)
		error = ENOTTY;
	return (error);
}

static void
flastrategy(struct buf *bp)
{
	int unit, error;
	u_long ef;
	struct fla_s *sc;
	static int busy;
	enum doc2k_work what;

	if (fla_debug)
		printf("flastrategy(%p) %lx %lx, %d, %ld, %p)\n",
			    bp, dev2ul(bp->b_dev), bp->b_flags, bp->b_blkno, 
			    bp->b_bcount / DEV_BSIZE, bp->b_data);
	unit = dkunit(bp->b_dev);
	sc = &softc[unit];

	if (dscheck(bp, sc->dk_slices) <= 0) {
		biodone(bp);
		return;
	}

	ef = read_eflags();
	disable_intr();

	bufqdisksort(&sc->buf_queue, bp);

	if (busy) {
		write_eflags(ef);
		return;
	}

	busy++;
	
	while (1) {
		bp = bufq_first(&sc->buf_queue);
		if (bp)
			bufq_remove(&sc->buf_queue, bp);

		write_eflags(ef);
		if (!bp)
			break;

		bp->b_resid = bp->b_bcount;
		unit = dkunit(bp->b_dev);

		if (bp->b_flags & B_FREEBUF)
			what = DOC2K_ERASE;
		else if (bp->b_flags & B_READ)
			what = DOC2K_READ;
		else 
			what = DOC2K_WRITE;
			
		error = doc2k_rwe( unit, what, bp->b_pblkno,
		    bp->b_bcount / DEV_BSIZE, bp->b_data);

		if (fla_debug || error) {
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

		ef = read_eflags();
		disable_intr();
	}
	busy = 0;
	return;
}

static int
flapsize(dev_t dev)
{
	int unit;
	struct fla_s *sc;

	if (fla_debug)
		printf("flapsize(%lx)\n", dev2ul(dev));
	unit = dkunit(dev);
	sc = &softc[unit];
	if (!sc->nsect)
		return 0;

	return (dssize(dev, &sc->dk_slices, flaopen, flaclose));
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


#if __FreeBSD_version > 400000	/* XXX ? */

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
#endif


#if __FreeBSD_version < 400000	/* XXX ? */

static int
flaprobe (struct isa_device *dvp)
{
	int unit;
	struct fla_s *sc;
	int i;

	unit = dvp->id_unit;
	sc = &softc[unit];
	i = flarealprobe(unit);
	if (i)
		return (0);

	dvp->id_maddr = (caddr_t)sc->ds.window;
	dvp->id_msize = 8192;

	cdevsw_add_generic(BDEV_MAJOR, CDEV_MAJOR, &fla_cdevsw);
	return (-1);
}


static int
flaattach (struct isa_device *dvp)
{
	int unit, i;

	unit = dvp->id_unit;
	i = flarealattach(unit);
	return (i);
}

struct isa_driver fladriver = {
        flaprobe, flaattach, "fla",
};

#endif
