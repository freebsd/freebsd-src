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
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <sys/module.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <sys/bus.h>

#define LEAVE()
#define ENTER()

#include <contrib/dev/fla/msysosak.h>

static MALLOC_DEFINE(M_FLA, "fla driver", "fla driver storage");

static int fla_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, fladebug, CTLFLAG_RW, &fla_debug, 0, "");

#define CDEV_MAJOR	102

static d_strategy_t flastrategy;
static d_open_t flaopen;
static d_close_t flaclose;
static d_ioctl_t flaioctl;

static struct cdevsw fla_cdevsw = {
        /* open */      flaopen,
        /* close */     flaclose,
        /* read */      physread,
        /* write */     physwrite,
        /* ioctl */     flaioctl,
        /* poll */      nopoll,
        /* mmap */      nommap,
        /* strategy */  flastrategy,
        /* name */      "fla",
        /* maj */       CDEV_MAJOR,
        /* dump */      nodump,
        /* psize */     nopsize,
        /* flags */     D_DISK | D_CANFREE,
};
static struct cdevsw fladisk_cdevsw;

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
	int busy;
	int unit;
	unsigned nsect;
	struct doc2k_stat ds;
	struct bio_queue_head bio_queue;
	struct devstat stats;
	struct disk disk;
	dev_t dev;
} softc[8];

static int
flaopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct fla_s *sc;
	int error;
	struct disklabel *dl;

	if (fla_debug)
		printf("flaopen(%s %x %x %p)\n",
			devtoname(dev), flag, fmt, p);

	sc = dev->si_drv1;

	error = doc2k_open(sc->unit);

	if (error) {
		printf("doc2k_open(%d) -> err %d\n", sc->unit, error);
		return (EIO);
	}

	dl = &sc->disk.d_label;
	bzero(dl, sizeof(*dl));
	error = doc2k_size(sc->unit, &dl->d_secperunit,
	    &dl->d_ncylinders, &dl->d_ntracks, &dl->d_nsectors);
	dl->d_secsize = DEV_BSIZE;
	dl->d_secpercyl = dl->d_ntracks * dl->d_nsectors; /* XXX */

	return (0);
}

static int
flaclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	int error;
	struct fla_s *sc;

	if (fla_debug)
		printf("flaclose(%s %x %x %p)\n",
			devtoname(dev), flags, fmt, p);

	sc = dev->si_drv1;

	error = doc2k_close(sc->unit);
	if (error) {
		printf("doc2k_close(%d) -> err %d\n", sc->unit, error);
		return (EIO);
	}
	return (0);
}

static int
flaioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{

	if (fla_debug)
		printf("flaioctl(%s %lx %p %x %p)\n",
			devtoname(dev), cmd, addr, flags, p);

	return (ENOIOCTL);
}

static void
flastrategy(struct bio *bp)
{
	int unit, error;
	int s;
	struct fla_s *sc;
	enum doc2k_work what;

	if (fla_debug > 1)
		printf("flastrategy(%p) %s %x, %d, %ld, %p)\n",
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
		bp->bio_resid = bp->bio_bcount;
		unit = dkunit(bp->bio_dev);

		if (bp->bio_cmd == BIO_DELETE)
			what = DOC2K_ERASE;
		else if (bp->bio_cmd == BIO_READ)
			what = DOC2K_READ;
		else 
			what = DOC2K_WRITE;

		LEAVE();

		error = doc2k_rwe( unit, what, bp->bio_pblkno,
		    bp->bio_bcount / DEV_BSIZE, bp->bio_data);

		ENTER();

		if (fla_debug > 1 || error) {
			printf("fla%d: %d = rwe(%p, %d, %d, %d, %ld, %p)\n",
			    unit, error, bp, unit, what, bp->bio_pblkno, 
			    bp->bio_bcount / DEV_BSIZE, bp->bio_data);
		}
		if (error) {
			bp->bio_error = EIO;
			bp->bio_flags |= BIO_ERROR;
		} else {
			bp->bio_resid = 0;
		}
		devstat_end_transaction_bio(&sc->stats, bp);
		biodone(bp);

		s = splbio();
	}
	sc->busy = 0;
	return;
}

static int
flaprobe (device_t dev)
{
	int unit;
	struct fla_s *sc;
	int i;

	unit = device_get_unit(dev);
	if (unit >= 8)
		return (ENXIO);
	sc = &softc[unit];

	/* This is slightly ugly */
	i = doc2k_probe(unit, KERNBASE + 0xc0000, KERNBASE + 0xe0000);
	if (i)
		return (ENXIO);

	i = doc2k_info(unit, &sc->ds);
	if (i)
		return (ENXIO);

	bus_set_resource(dev, SYS_RES_MEMORY, 0, 
		sc->ds.window - KERNBASE, 8192);

	return (0);
}

static int
flaattach (device_t dev)
{
	int unit;
	int i, j, k, l, m, error;
	struct fla_s *sc;

	unit = device_get_unit(dev);
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

	error = doc2k_close(unit);
	if (error) {
		printf("doc2k_close(%d) -> err %d\n", unit, error);
		return (EIO);
	}
	
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

	bioq_init(&sc->bio_queue);

	devstat_add_entry(&softc[unit].stats, "fla", unit, DEV_BSIZE,
		DEVSTAT_NO_ORDERED_TAGS, 
		DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_OTHER,
		DEVSTAT_PRIORITY_DISK);

	sc->dev = disk_create(unit, &sc->disk, 0, &fla_cdevsw, &fladisk_cdevsw);
	sc->dev->si_drv1 = sc;
	sc->unit = unit;

	return (0);
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

DRIVER_MODULE(fla, isa, fladriver, fla_devclass, 0, 0);
