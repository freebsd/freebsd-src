/*-
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

#include "opt_md.h"
#include "opt_ski.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <machine/md_var.h>
#include <geom/geom_disk.h>

#ifndef SKI_ROOT_FILESYSTEM
#define SKI_ROOT_FILESYSTEM	"ia64-root.fs"
#endif

#define SSC_OPEN			50
#define SSC_CLOSE			51
#define SSC_READ			52
#define SSC_WRITE			53
#define SSC_GET_COMPLETION		54
#define SSC_WAIT_COMPLETION		55

struct disk_req {
	unsigned long addr;
	unsigned len;
};

struct disk_stat {
	int fd;
	unsigned count;
};

static u_int64_t
ssc(u_int64_t in0, u_int64_t in1, u_int64_t in2, u_int64_t in3, int which)
{
	register u_int64_t ret0 __asm("r8");

	__asm __volatile("mov r15=%1\n\t"
			 "break 0x80001"
			 : "=r"(ret0)
			 : "r"(which), "r"(in0), "r"(in1), "r"(in2), "r"(in3));
	return ret0;
}

#ifndef SSC_NSECT
#define SSC_NSECT 409600
#endif

MALLOC_DEFINE(M_SSC, "ssc_disk", "Simulator Disk");

static int sscrootready;

static d_strategy_t sscstrategy;

static LIST_HEAD(, ssc_s) ssc_softc_list = LIST_HEAD_INITIALIZER(&ssc_softc_list);

struct ssc_s {
	int unit;
	LIST_ENTRY(ssc_s) list;
	struct bio_queue_head bio_queue;
	struct disk *disk;
	struct cdev *dev;
	int busy;
	int fd;
};

static int sscunits;

static void
sscstrategy(struct bio *bp)
{
	struct ssc_s *sc;
	int s;
	struct disk_req req;
	struct disk_stat stat;
	u_long len, va, off;

	sc = bp->bio_disk->d_drv1;

	s = splbio();

	bioq_disksort(&sc->bio_queue, bp);

	if (sc->busy) {
		splx(s);
		return;
	}

	sc->busy++;
	
	while (1) {
		bp = bioq_takefirst(&sc->bio_queue);
		splx(s);
		if (!bp)
			break;

		va = (u_long) bp->bio_data;
		len = bp->bio_bcount;
		off = bp->bio_pblkno << DEV_BSHIFT;
		while (len > 0) {
			u_int t;
			if ((va & PAGE_MASK) + len > PAGE_SIZE)
				t = PAGE_SIZE - (va & PAGE_MASK);
			else
				t = len;
			req.len = t;
			req.addr = ia64_tpa(va);
			ssc(sc->fd, 1, ia64_tpa((long) &req), off,
			    (bp->bio_cmd == BIO_READ) ? SSC_READ : SSC_WRITE);
			stat.fd = sc->fd;
			ssc(ia64_tpa((long)&stat), 0, 0, 0,
			    SSC_WAIT_COMPLETION);
			va += t;
			len -= t;
			off += t;
		}
		bp->bio_resid = 0;
		biodone(bp);
		s = splbio();
	}

	sc->busy = 0;
	return;
}

static struct ssc_s *
ssccreate(int unit)
{
	struct ssc_s *sc;
	int fd;

	fd = ssc(ia64_tpa((u_int64_t) SKI_ROOT_FILESYSTEM),
		 1, 0, 0, SSC_OPEN);
	if (fd == -1)
		return (NULL);

	if (unit == -1)
		unit = sscunits++;
	/* Make sure this unit isn't already in action */
	LIST_FOREACH(sc, &ssc_softc_list, list) {
		if (sc->unit == unit)
			return (NULL);
	}
	MALLOC(sc, struct ssc_s *,sizeof(*sc), M_SSC, M_WAITOK | M_ZERO);
	LIST_INSERT_HEAD(&ssc_softc_list, sc, list);
	sc->unit = unit;
	bioq_init(&sc->bio_queue);

	sc->disk = disk_alloc();
	sc->disk->d_drv1 = sc;
	sc->disk->d_fwheads = 0;
	sc->disk->d_fwsectors = 0;
	sc->disk->d_maxsize = DFLTPHYS;
	sc->disk->d_mediasize = (off_t)SSC_NSECT * DEV_BSIZE;
	sc->disk->d_name = "sscdisk";
	sc->disk->d_sectorsize = DEV_BSIZE;
	sc->disk->d_strategy = sscstrategy;
	sc->disk->d_unit = sc->unit;
	sc->disk->d_flags = DISKFLAG_NEEDSGIANT;
	disk_create(sc->disk, DISK_VERSION);
	sc->fd = fd;
	if (sc->unit == 0) 
		sscrootready = 1;
	return (sc);
}

static void
ssc_drvinit(void *unused)
{
	ssccreate(-1);
}

SYSINIT(sscdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE, ssc_drvinit,NULL)

static void
ssc_takeroot(void *junk)
{
	if (sscrootready)
		rootdevnames[0] = "ufs:/dev/sscdisk0";
}

SYSINIT(ssc_root, SI_SUB_MOUNT_ROOT, SI_ORDER_FIRST, ssc_takeroot, NULL);
