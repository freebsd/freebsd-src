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

#include "opt_md.h"
#include "opt_ski.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
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

MALLOC_DEFINE(M_SSC, "SSC disk", "Memory Disk");
MALLOC_DEFINE(M_SSCSECT, "SSC sectors", "Memory Disk Sectors");

static int ssc_debug;
SYSCTL_INT(_debug, OID_AUTO, sscdebug, CTLFLAG_RW, &ssc_debug, 0, "");

static int sscrootready;

#define CDEV_MAJOR	157

static d_strategy_t sscstrategy;
static d_open_t sscopen;
static d_ioctl_t sscioctl;

static struct cdevsw ssc_cdevsw = {
        /* open */      sscopen,
        /* close */     nullclose,
        /* read */      physread,
        /* write */     physwrite,
        /* ioctl */     sscioctl,
        /* poll */      nopoll,
        /* mmap */      nommap,
        /* strategy */  sscstrategy,
        /* name */      "sscdisk",
        /* maj */       CDEV_MAJOR,
        /* dump */      nodump,
        /* psize */     nopsize,
        /* flags */     D_DISK | D_CANFREE,
};

static struct cdevsw sscdisk_cdevsw;

static LIST_HEAD(, ssc_s) ssc_softc_list = LIST_HEAD_INITIALIZER(&ssc_softc_list);

struct ssc_s {
	int unit;
	LIST_ENTRY(ssc_s) list;
	struct devstat stats;
	struct bio_queue_head bio_queue;
	struct disk disk;
	dev_t dev;
	int busy;
	unsigned nsect;
	int fd;
};

static int sscunits;

static int
sscopen(dev_t dev, int flag, int fmt, struct thread *td)
{
	struct ssc_s *sc;

	if (ssc_debug)
		printf("sscopen(%s %x %x %p)\n",
			devtoname(dev), flag, fmt, td);

	sc = dev->si_drv1;

	sc->disk.d_sectorsize = DEV_BSIZE;
	sc->disk.d_mediasize = (off_t)sc->nsect * DEV_BSIZE;
	sc->disk.d_fwsectors = 0;
	sc->disk.d_fwheads = 0;
	return (0);
}

static int
sscioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{

	if (ssc_debug)
		printf("sscioctl(%s %lx %p %x %p)\n",
			devtoname(dev), cmd, addr, flags, td);

	return (ENOIOCTL);
}

static void
sscstrategy(struct bio *bp)
{
	struct ssc_s *sc;
	int s;
	devstat_trans_flags dop;
	unsigned sscop = 0;
	struct disk_req req;
	struct disk_stat stat;
	u_long len, va, off;

	if (ssc_debug > 1)
		printf("sscstrategy(%p) %s %x, %ld, %ld, %p)\n",
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
			sscop = SSC_READ;
		} else {
			dop = DEVSTAT_WRITE;
			sscop = SSC_WRITE;
		}
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
			if (ssc_debug > 1)
				printf("sscstrategy: reading %d bytes from 0x%ld into 0x%lx\n",
				       req.len, off, req.addr);
			ssc(sc->fd, 1, ia64_tpa((long) &req), off, sscop);
			stat.fd = sc->fd;
			ssc(ia64_tpa((long)&stat), 0, 0, 0,
			    SSC_WAIT_COMPLETION);
			va += t;
			len -= t;
			off += t;
		}
		bp->bio_resid = 0;
		biofinish(bp, &sc->stats, 0);
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
	MALLOC(sc, struct ssc_s *,sizeof(*sc), M_SSC, M_ZERO);
	LIST_INSERT_HEAD(&ssc_softc_list, sc, list);
	sc->unit = unit;
	bioq_init(&sc->bio_queue);
	devstat_add_entry(&sc->stats, "sscdisk", sc->unit, DEV_BSIZE,
		DEVSTAT_NO_ORDERED_TAGS, 
		DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_OTHER,
		DEVSTAT_PRIORITY_OTHER);
	sc->dev = disk_create(sc->unit, &sc->disk, 0,
			      &ssc_cdevsw, &sscdisk_cdevsw);
	sc->dev->si_drv1 = sc;
	sc->nsect = SSC_NSECT;
	sc->fd = fd;
	if (sc->unit == 0) 
		sscrootready = 1;
	return (sc);
}

#if 0
static void
ssc_clone (void *arg, char *name, int namelen, dev_t *dev)
{
	int i, u;

	if (*dev != NODEV)
		return;
	i = dev_stdclone(name, NULL, "ssc", &u);
	if (i == 0)
		return;
	/* XXX: should check that next char is [\0sa-h] */
	/*
	 * Now we cheat: We just create the disk, but don't match.
	 * Since we run before it, subr_disk.c::disk_clone() will
	 * find our disk and match the sought for device.
	 */
	ssccreate(u);
	return;
}
#endif

static void
ssc_drvinit(void *unused)
{
	if (!ia64_running_in_simulator())
		return;

	ssccreate(-1);
}

SYSINIT(sscdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR, ssc_drvinit,NULL)

static void
ssc_takeroot(void *junk)
{
	if (sscrootready)
		rootdevnames[0] = "ufs:/dev/sscdisk0c";
}

SYSINIT(ssc_root, SI_SUB_MOUNT_ROOT, SI_ORDER_FIRST, ssc_takeroot, NULL);

