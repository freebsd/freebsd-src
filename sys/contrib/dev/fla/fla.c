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
#include <sys/stdint.h>
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
flaopen(struct disk *dp)
{
	struct fla_s *sc;
	int error;
	u_int spu, ncyl, nt, ns;

	sc = dp->d_drv1;

	error = doc2k_open(sc->unit);

	if (error) {
		printf("doc2k_open(%d) -> err %d\n", sc->unit, error);
		return (EIO);
	}

	error = doc2k_size(sc->unit, &spu, &ncyl, &nt, &ns);
	sc->disk.d_sectorsize = DEV_BSIZE;
	sc->disk.d_mediasize = (off_t)spu * DEV_BSIZE;
	sc->disk.d_fwsectors = ns;
	sc->disk.d_fwheads = nt;

	return (0);
}

static int
flaclose(struct disk *dp)
{
	int error;
	struct fla_s *sc;

	sc = dp->d_drv1;

	error = doc2k_close(sc->unit);
	if (error) {
		printf("doc2k_close(%d) -> err %d\n", sc->unit, error);
		return (EIO);
	}
	return (0);
}

static void
flastrategy(struct bio *bp)
{
	int unit, error;
	struct fla_s *sc;
	enum doc2k_work what;

	sc = bp->bio_disk->d_drv1;


	bioqdisksort(&sc->bio_queue, bp);

	if (sc->busy) {
		return;
	}

	sc->busy++;
	
	while (1) {
		bp = bioq_first(&sc->bio_queue);
		if (bp)
			bioq_remove(&sc->bio_queue, bp);
		if (!bp)
			break;

		devstat_start_transaction(&sc->stats);
		bp->bio_resid = bp->bio_bcount;
		unit = sc->unit;

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

		if (error) {
			printf("fla%d: %d = rwe(%p, %d, %d, %jd, %ld, %p)\n",
			    unit, error, bp, unit, what,
			    (intmax_t)bp->bio_pblkno, 
			    bp->bio_bcount / DEV_BSIZE, bp->bio_data);
		}
		if (error) {
			bp->bio_error = EIO;
			bp->bio_flags |= BIO_ERROR;
		} else {
			bp->bio_resid = 0;
		}
		biofinish(bp, &sc->stats, 0);

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

	sc->disk.d_open = flaopen;
	sc->disk.d_close = flaclose;
	sc->disk.d_strategy = flastrategy;
	sc->disk.d_drv1 = sc;
	sc->disk.d_name = "fla";
	sc->disk.d_maxsize = MAXPHYS;
	sc->unit = unit;
	disk_create(unit, &sc->disk, DISKFLAG_CANDELETE, NULL, NULL);

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
