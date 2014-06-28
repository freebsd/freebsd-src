/*-
 * Copyright (c) 2009 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ada.h"

#include <sys/param.h>

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/cons.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <geom/geom_disk.h>
#endif /* _KERNEL */

#ifndef _KERNEL
#include <stdio.h>
#include <string.h>
#endif /* _KERNEL */

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_sim.h>

#include <cam/ata/ata_all.h>

#include <machine/md_var.h>	/* geometry translation */

#ifdef _KERNEL

#define ATA_MAX_28BIT_LBA               268435455UL

typedef enum {
	ADA_STATE_RAHEAD,
	ADA_STATE_WCACHE,
	ADA_STATE_NORMAL
} ada_state;

typedef enum {
	ADA_FLAG_CAN_48BIT	= 0x0002,
	ADA_FLAG_CAN_FLUSHCACHE	= 0x0004,
	ADA_FLAG_CAN_NCQ	= 0x0008,
	ADA_FLAG_CAN_DMA	= 0x0010,
	ADA_FLAG_NEED_OTAG	= 0x0020,
	ADA_FLAG_WAS_OTAG	= 0x0040,
	ADA_FLAG_CAN_TRIM	= 0x0080,
	ADA_FLAG_OPEN		= 0x0100,
	ADA_FLAG_SCTX_INIT	= 0x0200,
	ADA_FLAG_CAN_CFA        = 0x0400,
	ADA_FLAG_CAN_POWERMGT   = 0x0800,
	ADA_FLAG_CAN_DMA48	= 0x1000,
	ADA_FLAG_DIRTY		= 0x2000
} ada_flags;

typedef enum {
	ADA_Q_NONE		= 0x00,
	ADA_Q_4K		= 0x01,
} ada_quirks;

#define ADA_Q_BIT_STRING	\
	"\020"			\
	"\0014K"

typedef enum {
	ADA_CCB_RAHEAD		= 0x01,
	ADA_CCB_WCACHE		= 0x02,
	ADA_CCB_BUFFER_IO	= 0x03,
	ADA_CCB_DUMP		= 0x05,
	ADA_CCB_TRIM		= 0x06,
	ADA_CCB_TYPE_MASK	= 0x0F,
} ada_ccb_state;

/* Offsets into our private area for storing information */
#define ccb_state	ppriv_field0
#define ccb_bp		ppriv_ptr1

struct disk_params {
	u_int8_t  heads;
	u_int8_t  secs_per_track;
	u_int32_t cylinders;
	u_int32_t secsize;	/* Number of bytes/logical sector */
	u_int64_t sectors;	/* Total number sectors */
};

#define TRIM_MAX_BLOCKS	8
#define TRIM_MAX_RANGES	(TRIM_MAX_BLOCKS * ATA_DSM_BLK_RANGES)
struct trim_request {
	uint8_t		data[TRIM_MAX_RANGES * ATA_DSM_RANGE_SIZE];
	TAILQ_HEAD(, bio) bps;
};

struct ada_softc {
	struct	 bio_queue_head bio_queue;
	struct	 bio_queue_head trim_queue;
	int	 outstanding_cmds;	/* Number of active commands */
	int	 refcount;		/* Active xpt_action() calls */
	ada_state state;
	ada_flags flags;
	ada_quirks quirks;
	int	 sort_io_queue;
	int	 trim_max_ranges;
	int	 trim_running;
	int	 read_ahead;
	int	 write_cache;
#ifdef ADA_TEST_FAILURE
	int      force_read_error;
	int      force_write_error;
	int      periodic_read_error;
	int      periodic_read_count;
#endif
	struct	 disk_params params;
	struct	 disk *disk;
	struct task		sysctl_task;
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
	struct callout		sendordered_c;
	struct trim_request	trim_req;
};

struct ada_quirk_entry {
	struct scsi_inquiry_pattern inq_pat;
	ada_quirks quirks;
};

static struct ada_quirk_entry ada_quirk_table[] =
{
	{
		/* Hitachi Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "Hitachi H??????????E3*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Samsung Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "SAMSUNG HD155UI*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Samsung Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "SAMSUNG HD204UI*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "ST????DL*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Seagate Barracuda Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "ST???DM*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Seagate Barracuda Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "ST????DM*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "ST9500423AS*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "ST9500424AS*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "ST9640423AS*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "ST9640424AS*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "ST9750420AS*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "ST9750422AS*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "ST9750423AS*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Seagate Momentus Thin Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "ST???LT*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "WDC WD????RS*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "WDC WD????RX*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "WDC WD??????RS*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "WDC WD??????RX*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* WDC Scorpio Black Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "WDC WD???PKT*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* WDC Scorpio Black Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "WDC WD?????PKT*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* WDC Scorpio Blue Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "WDC WD???PVT*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* WDC Scorpio Blue Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "WDC WD?????PVT*", "*" },
		/*quirks*/ADA_Q_4K
	},
	/* SSDs */
	{
		/*
		 * Corsair Force 2 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "Corsair CSSD-F*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Corsair Force 3 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "Corsair Force 3*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Corsair Neutron GTX SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "Corsair Neutron GTX*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Corsair Force GT SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "Corsair Force GT*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Crucial M4 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "M4-CT???M4SSD2*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Crucial RealSSD C300 SSDs
		 * 4k optimised
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "C300-CTFDDAC???MAG*",
		"*" }, /*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Intel 320 Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "INTEL SSDSA2CW*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Intel 330 Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "INTEL SSDSC2CT*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Intel 510 Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "INTEL SSDSC2MH*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Intel 520 Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "INTEL SSDSC2BW*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Intel X25-M Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "INTEL SSDSA2M*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Kingston E100 Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "KINGSTON SE100S3*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Kingston HyperX 3k SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "KINGSTON SH103S3*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Marvell SSDs (entry taken from OpenSolaris)
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "MARVELL SD88SA02*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * OCZ Agility 2 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "OCZ-AGILITY2*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * OCZ Agility 3 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "OCZ-AGILITY3*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * OCZ Deneva R Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "DENRSTE251M45*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * OCZ Vertex 2 SSDs (inc pro series)
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "OCZ?VERTEX2*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * OCZ Vertex 3 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "OCZ-VERTEX3*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * OCZ Vertex 4 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "OCZ-VERTEX4*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * Samsung 830 Series SSDs
		 * 4k optimised
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "SAMSUNG SSD 830 Series*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * SuperTalent TeraDrive CT SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "FTM??CT25H*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/*
		 * XceedIOPS SATA SSDs
		 * 4k optimised
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "SG9XCS2D*", "*" },
		/*quirks*/ADA_Q_4K
	},
	{
		/* Default */
		{
		  T_ANY, SIP_MEDIA_REMOVABLE|SIP_MEDIA_FIXED,
		  /*vendor*/"*", /*product*/"*", /*revision*/"*"
		},
		/*quirks*/0
	},
};

static	disk_strategy_t	adastrategy;
static	dumper_t	adadump;
static	periph_init_t	adainit;
static	void		adaasync(void *callback_arg, u_int32_t code,
				struct cam_path *path, void *arg);
static	void		adasysctlinit(void *context, int pending);
static	periph_ctor_t	adaregister;
static	periph_dtor_t	adacleanup;
static	periph_start_t	adastart;
static	periph_oninv_t	adaoninvalidate;
static	void		adadone(struct cam_periph *periph,
			       union ccb *done_ccb);
static  int		adaerror(union ccb *ccb, u_int32_t cam_flags,
				u_int32_t sense_flags);
static void		adagetparams(struct cam_periph *periph,
				struct ccb_getdev *cgd);
static timeout_t	adasendorderedtag;
static void		adashutdown(void *arg, int howto);
static void		adasuspend(void *arg);
static void		adaresume(void *arg);

#ifndef	ADA_DEFAULT_LEGACY_ALIASES
#define	ADA_DEFAULT_LEGACY_ALIASES	1
#endif

#ifndef ADA_DEFAULT_TIMEOUT
#define ADA_DEFAULT_TIMEOUT 30	/* Timeout in seconds */
#endif

#ifndef	ADA_DEFAULT_RETRY
#define	ADA_DEFAULT_RETRY	4
#endif

#ifndef	ADA_DEFAULT_SEND_ORDERED
#define	ADA_DEFAULT_SEND_ORDERED	1
#endif

#ifndef	ADA_DEFAULT_SPINDOWN_SHUTDOWN
#define	ADA_DEFAULT_SPINDOWN_SHUTDOWN	1
#endif

#ifndef	ADA_DEFAULT_SPINDOWN_SUSPEND
#define	ADA_DEFAULT_SPINDOWN_SUSPEND	1
#endif

#ifndef	ADA_DEFAULT_READ_AHEAD
#define	ADA_DEFAULT_READ_AHEAD	1
#endif

#ifndef	ADA_DEFAULT_WRITE_CACHE
#define	ADA_DEFAULT_WRITE_CACHE	1
#endif

#define	ADA_RA	(softc->read_ahead >= 0 ? \
		 softc->read_ahead : ada_read_ahead)
#define	ADA_WC	(softc->write_cache >= 0 ? \
		 softc->write_cache : ada_write_cache)
#define	ADA_SIO	(softc->sort_io_queue >= 0 ? \
		 softc->sort_io_queue : cam_sort_io_queues)

/*
 * Most platforms map firmware geometry to actual, but some don't.  If
 * not overridden, default to nothing.
 */
#ifndef ata_disk_firmware_geom_adjust
#define	ata_disk_firmware_geom_adjust(disk)
#endif

static int ada_legacy_aliases = ADA_DEFAULT_LEGACY_ALIASES;
static int ada_retry_count = ADA_DEFAULT_RETRY;
static int ada_default_timeout = ADA_DEFAULT_TIMEOUT;
static int ada_send_ordered = ADA_DEFAULT_SEND_ORDERED;
static int ada_spindown_shutdown = ADA_DEFAULT_SPINDOWN_SHUTDOWN;
static int ada_spindown_suspend = ADA_DEFAULT_SPINDOWN_SUSPEND;
static int ada_read_ahead = ADA_DEFAULT_READ_AHEAD;
static int ada_write_cache = ADA_DEFAULT_WRITE_CACHE;

static SYSCTL_NODE(_kern_cam, OID_AUTO, ada, CTLFLAG_RD, 0,
            "CAM Direct Access Disk driver");
SYSCTL_INT(_kern_cam_ada, OID_AUTO, legacy_aliases, CTLFLAG_RWTUN,
           &ada_legacy_aliases, 0, "Create legacy-like device aliases");
SYSCTL_INT(_kern_cam_ada, OID_AUTO, retry_count, CTLFLAG_RWTUN,
           &ada_retry_count, 0, "Normal I/O retry count");
SYSCTL_INT(_kern_cam_ada, OID_AUTO, default_timeout, CTLFLAG_RWTUN,
           &ada_default_timeout, 0, "Normal I/O timeout (in seconds)");
SYSCTL_INT(_kern_cam_ada, OID_AUTO, send_ordered, CTLFLAG_RWTUN,
           &ada_send_ordered, 0, "Send Ordered Tags");
SYSCTL_INT(_kern_cam_ada, OID_AUTO, spindown_shutdown, CTLFLAG_RWTUN,
           &ada_spindown_shutdown, 0, "Spin down upon shutdown");
SYSCTL_INT(_kern_cam_ada, OID_AUTO, spindown_suspend, CTLFLAG_RWTUN,
           &ada_spindown_suspend, 0, "Spin down upon suspend");
SYSCTL_INT(_kern_cam_ada, OID_AUTO, read_ahead, CTLFLAG_RWTUN,
           &ada_read_ahead, 0, "Enable disk read-ahead");
SYSCTL_INT(_kern_cam_ada, OID_AUTO, write_cache, CTLFLAG_RWTUN,
           &ada_write_cache, 0, "Enable disk write cache");

/*
 * ADA_ORDEREDTAG_INTERVAL determines how often, relative
 * to the default timeout, we check to see whether an ordered
 * tagged transaction is appropriate to prevent simple tag
 * starvation.  Since we'd like to ensure that there is at least
 * 1/2 of the timeout length left for a starved transaction to
 * complete after we've sent an ordered tag, we must poll at least
 * four times in every timeout period.  This takes care of the worst
 * case where a starved transaction starts during an interval that
 * meets the requirement "don't send an ordered tag" test so it takes
 * us two intervals to determine that a tag must be sent.
 */
#ifndef ADA_ORDEREDTAG_INTERVAL
#define ADA_ORDEREDTAG_INTERVAL 4
#endif

static struct periph_driver adadriver =
{
	adainit, "ada",
	TAILQ_HEAD_INITIALIZER(adadriver.units), /* generation */ 0
};

PERIPHDRIVER_DECLARE(ada, adadriver);

static MALLOC_DEFINE(M_ATADA, "ata_da", "ata_da buffers");

static int
adaopen(struct disk *dp)
{
	struct cam_periph *periph;
	struct ada_softc *softc;
	int error;

	periph = (struct cam_periph *)dp->d_drv1;
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		return(ENXIO);
	}

	cam_periph_lock(periph);
	if ((error = cam_periph_hold(periph, PRIBIO|PCATCH)) != 0) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return (error);
	}

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE | CAM_DEBUG_PERIPH,
	    ("adaopen\n"));

	softc = (struct ada_softc *)periph->softc;
	softc->flags |= ADA_FLAG_OPEN;

	cam_periph_unhold(periph);
	cam_periph_unlock(periph);
	return (0);
}

static int
adaclose(struct disk *dp)
{
	struct	cam_periph *periph;
	struct	ada_softc *softc;
	union ccb *ccb;
	int error;

	periph = (struct cam_periph *)dp->d_drv1;
	softc = (struct ada_softc *)periph->softc;
	cam_periph_lock(periph);

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE | CAM_DEBUG_PERIPH,
	    ("adaclose\n"));

	/* We only sync the cache if the drive is capable of it. */
	if ((softc->flags & ADA_FLAG_DIRTY) != 0 &&
	    (softc->flags & ADA_FLAG_CAN_FLUSHCACHE) != 0 &&
	    (periph->flags & CAM_PERIPH_INVALID) == 0 &&
	    cam_periph_hold(periph, PRIBIO) == 0) {

		ccb = cam_periph_getccb(periph, CAM_PRIORITY_NORMAL);
		cam_fill_ataio(&ccb->ataio,
				    1,
				    adadone,
				    CAM_DIR_NONE,
				    0,
				    NULL,
				    0,
				    ada_default_timeout*1000);

		if (softc->flags & ADA_FLAG_CAN_48BIT)
			ata_48bit_cmd(&ccb->ataio, ATA_FLUSHCACHE48, 0, 0, 0);
		else
			ata_28bit_cmd(&ccb->ataio, ATA_FLUSHCACHE, 0, 0, 0);
		error = cam_periph_runccb(ccb, adaerror, /*cam_flags*/0,
		    /*sense_flags*/0, softc->disk->d_devstat);

		if (error != 0)
			xpt_print(periph->path, "Synchronize cache failed\n");
		else
			softc->flags &= ~ADA_FLAG_DIRTY;
		xpt_release_ccb(ccb);
		cam_periph_unhold(periph);
	}

	softc->flags &= ~ADA_FLAG_OPEN;

	while (softc->refcount != 0)
		cam_periph_sleep(periph, &softc->refcount, PRIBIO, "adaclose", 1);
	cam_periph_unlock(periph);
	cam_periph_release(periph);
	return (0);	
}

static void
adaschedule(struct cam_periph *periph)
{
	struct ada_softc *softc = (struct ada_softc *)periph->softc;

	if (softc->state != ADA_STATE_NORMAL)
		return;

	/* Check if we have more work to do. */
	if (bioq_first(&softc->bio_queue) ||
	    (!softc->trim_running && bioq_first(&softc->trim_queue))) {
		xpt_schedule(periph, CAM_PRIORITY_NORMAL);
	}
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
static void
adastrategy(struct bio *bp)
{
	struct cam_periph *periph;
	struct ada_softc *softc;
	
	periph = (struct cam_periph *)bp->bio_disk->d_drv1;
	softc = (struct ada_softc *)periph->softc;

	cam_periph_lock(periph);

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("adastrategy(%p)\n", bp));

	/*
	 * If the device has been made invalid, error out
	 */
	if ((periph->flags & CAM_PERIPH_INVALID) != 0) {
		cam_periph_unlock(periph);
		biofinish(bp, NULL, ENXIO);
		return;
	}
	
	/*
	 * Place it in the queue of disk activities for this disk
	 */
	if (bp->bio_cmd == BIO_DELETE &&
	    (softc->flags & ADA_FLAG_CAN_TRIM)) {
		bioq_disksort(&softc->trim_queue, bp);
	} else {
		if (ADA_SIO)
		    bioq_disksort(&softc->bio_queue, bp);
		else
		    bioq_insert_tail(&softc->bio_queue, bp);
	}

	/*
	 * Schedule ourselves for performing the work.
	 */
	adaschedule(periph);
	cam_periph_unlock(periph);

	return;
}

static int
adadump(void *arg, void *virtual, vm_offset_t physical, off_t offset, size_t length)
{
	struct	    cam_periph *periph;
	struct	    ada_softc *softc;
	u_int	    secsize;
	union	    ccb ccb;
	struct	    disk *dp;
	uint64_t    lba;
	uint16_t    count;
	int	    error = 0;

	dp = arg;
	periph = dp->d_drv1;
	softc = (struct ada_softc *)periph->softc;
	cam_periph_lock(periph);
	secsize = softc->params.secsize;
	lba = offset / secsize;
	count = length / secsize;
	
	if ((periph->flags & CAM_PERIPH_INVALID) != 0) {
		cam_periph_unlock(periph);
		return (ENXIO);
	}

	if (length > 0) {
		xpt_setup_ccb(&ccb.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
		ccb.ccb_h.ccb_state = ADA_CCB_DUMP;
		cam_fill_ataio(&ccb.ataio,
		    0,
		    adadone,
		    CAM_DIR_OUT,
		    0,
		    (u_int8_t *) virtual,
		    length,
		    ada_default_timeout*1000);
		if ((softc->flags & ADA_FLAG_CAN_48BIT) &&
		    (lba + count >= ATA_MAX_28BIT_LBA ||
		    count >= 256)) {
			ata_48bit_cmd(&ccb.ataio, ATA_WRITE_DMA48,
			    0, lba, count);
		} else {
			ata_28bit_cmd(&ccb.ataio, ATA_WRITE_DMA,
			    0, lba, count);
		}
		xpt_polled_action(&ccb);

		error = cam_periph_error(&ccb,
		    0, SF_NO_RECOVERY | SF_NO_RETRY, NULL);
		if ((ccb.ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(ccb.ccb_h.path, /*relsim_flags*/0,
			    /*reduction*/0, /*timeout*/0, /*getcount_only*/0);
		if (error != 0)
			printf("Aborting dump due to I/O error.\n");

		cam_periph_unlock(periph);
		return (error);
	}

	if (softc->flags & ADA_FLAG_CAN_FLUSHCACHE) {
		xpt_setup_ccb(&ccb.ccb_h, periph->path, CAM_PRIORITY_NORMAL);

		ccb.ccb_h.ccb_state = ADA_CCB_DUMP;
		cam_fill_ataio(&ccb.ataio,
				    0,
				    adadone,
				    CAM_DIR_NONE,
				    0,
				    NULL,
				    0,
				    ada_default_timeout*1000);

		if (softc->flags & ADA_FLAG_CAN_48BIT)
			ata_48bit_cmd(&ccb.ataio, ATA_FLUSHCACHE48, 0, 0, 0);
		else
			ata_28bit_cmd(&ccb.ataio, ATA_FLUSHCACHE, 0, 0, 0);
		xpt_polled_action(&ccb);

		error = cam_periph_error(&ccb,
		    0, SF_NO_RECOVERY | SF_NO_RETRY, NULL);
		if ((ccb.ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(ccb.ccb_h.path, /*relsim_flags*/0,
			    /*reduction*/0, /*timeout*/0, /*getcount_only*/0);
		if (error != 0)
			xpt_print(periph->path, "Synchronize cache failed\n");
	}
	cam_periph_unlock(periph);
	return (error);
}

static void
adainit(void)
{
	cam_status status;

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, adaasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("ada: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	} else if (ada_send_ordered) {

		/* Register our event handlers */
		if ((EVENTHANDLER_REGISTER(power_suspend, adasuspend,
					   NULL, EVENTHANDLER_PRI_LAST)) == NULL)
		    printf("adainit: power event registration failed!\n");
		if ((EVENTHANDLER_REGISTER(power_resume, adaresume,
					   NULL, EVENTHANDLER_PRI_LAST)) == NULL)
		    printf("adainit: power event registration failed!\n");
		if ((EVENTHANDLER_REGISTER(shutdown_post_sync, adashutdown,
					   NULL, SHUTDOWN_PRI_DEFAULT)) == NULL)
		    printf("adainit: shutdown event registration failed!\n");
	}
}

/*
 * Callback from GEOM, called when it has finished cleaning up its
 * resources.
 */
static void
adadiskgonecb(struct disk *dp)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)dp->d_drv1;

	cam_periph_release(periph);
}

static void
adaoninvalidate(struct cam_periph *periph)
{
	struct ada_softc *softc;

	softc = (struct ada_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_register_async(0, adaasync, periph, periph->path);

	/*
	 * Return all queued I/O with ENXIO.
	 * XXX Handle any transactions queued to the card
	 *     with XPT_ABORT_CCB.
	 */
	bioq_flush(&softc->bio_queue, NULL, ENXIO);
	bioq_flush(&softc->trim_queue, NULL, ENXIO);

	disk_gone(softc->disk);
}

static void
adacleanup(struct cam_periph *periph)
{
	struct ada_softc *softc;

	softc = (struct ada_softc *)periph->softc;

	cam_periph_unlock(periph);

	/*
	 * If we can't free the sysctl tree, oh well...
	 */
	if ((softc->flags & ADA_FLAG_SCTX_INIT) != 0
	    && sysctl_ctx_free(&softc->sysctl_ctx) != 0) {
		xpt_print(periph->path, "can't remove sysctl context\n");
	}

	disk_destroy(softc->disk);
	callout_drain(&softc->sendordered_c);
	free(softc, M_DEVBUF);
	cam_periph_lock(periph);
}

static void
adaasync(void *callback_arg, u_int32_t code,
	struct cam_path *path, void *arg)
{
	struct ccb_getdev cgd;
	struct cam_periph *periph;
	struct ada_softc *softc;

	periph = (struct cam_periph *)callback_arg;
	switch (code) {
	case AC_FOUND_DEVICE:
	{
		struct ccb_getdev *cgd;
		cam_status status;
 
		cgd = (struct ccb_getdev *)arg;
		if (cgd == NULL)
			break;

		if (cgd->protocol != PROTO_ATA)
			break;

		/*
		 * Allocate a peripheral instance for
		 * this device and start the probe
		 * process.
		 */
		status = cam_periph_alloc(adaregister, adaoninvalidate,
					  adacleanup, adastart,
					  "ada", CAM_PERIPH_BIO,
					  path, adaasync,
					  AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG)
			printf("adaasync: Unable to attach to new device "
				"due to status 0x%x\n", status);
		break;
	}
	case AC_GETDEV_CHANGED:
	{
		softc = (struct ada_softc *)periph->softc;
		xpt_setup_ccb(&cgd.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
		cgd.ccb_h.func_code = XPT_GDEV_TYPE;
		xpt_action((union ccb *)&cgd);

		if ((cgd.ident_data.capabilities1 & ATA_SUPPORT_DMA) &&
		    (cgd.inq_flags & SID_DMA))
			softc->flags |= ADA_FLAG_CAN_DMA;
		else
			softc->flags &= ~ADA_FLAG_CAN_DMA;
		if (cgd.ident_data.support.command2 & ATA_SUPPORT_ADDRESS48) {
			softc->flags |= ADA_FLAG_CAN_48BIT;
			if (cgd.inq_flags & SID_DMA48)
				softc->flags |= ADA_FLAG_CAN_DMA48;
			else
				softc->flags &= ~ADA_FLAG_CAN_DMA48;
		} else
			softc->flags &= ~(ADA_FLAG_CAN_48BIT |
			    ADA_FLAG_CAN_DMA48);
		if ((cgd.ident_data.satacapabilities & ATA_SUPPORT_NCQ) &&
		    (cgd.inq_flags & SID_DMA) && (cgd.inq_flags & SID_CmdQue))
			softc->flags |= ADA_FLAG_CAN_NCQ;
		else
			softc->flags &= ~ADA_FLAG_CAN_NCQ;
		if ((cgd.ident_data.support_dsm & ATA_SUPPORT_DSM_TRIM) &&
		    (cgd.inq_flags & SID_DMA))
			softc->flags |= ADA_FLAG_CAN_TRIM;
		else
			softc->flags &= ~ADA_FLAG_CAN_TRIM;

		cam_periph_async(periph, code, path, arg);
		break;
	}
	case AC_ADVINFO_CHANGED:
	{
		uintptr_t buftype;

		buftype = (uintptr_t)arg;
		if (buftype == CDAI_TYPE_PHYS_PATH) {
			struct ada_softc *softc;

			softc = periph->softc;
			disk_attr_changed(softc->disk, "GEOM::physpath",
					  M_NOWAIT);
		}
		break;
	}
	case AC_SENT_BDR:
	case AC_BUS_RESET:
	{
		softc = (struct ada_softc *)periph->softc;
		cam_periph_async(periph, code, path, arg);
		if (softc->state != ADA_STATE_NORMAL)
			break;
		xpt_setup_ccb(&cgd.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
		cgd.ccb_h.func_code = XPT_GDEV_TYPE;
		xpt_action((union ccb *)&cgd);
		if (ADA_RA >= 0 &&
		    cgd.ident_data.support.command1 & ATA_SUPPORT_LOOKAHEAD)
			softc->state = ADA_STATE_RAHEAD;
		else if (ADA_WC >= 0 &&
		    cgd.ident_data.support.command1 & ATA_SUPPORT_WRITECACHE)
			softc->state = ADA_STATE_WCACHE;
		else
		    break;
		if (cam_periph_acquire(periph) != CAM_REQ_CMP)
			softc->state = ADA_STATE_NORMAL;
		else
			xpt_schedule(periph, CAM_PRIORITY_DEV);
	}
	default:
		cam_periph_async(periph, code, path, arg);
		break;
	}
}

static void
adasysctlinit(void *context, int pending)
{
	struct cam_periph *periph;
	struct ada_softc *softc;
	char tmpstr[80], tmpstr2[80];

	periph = (struct cam_periph *)context;

	/* periph was held for us when this task was enqueued */
	if ((periph->flags & CAM_PERIPH_INVALID) != 0) {
		cam_periph_release(periph);
		return;
	}

	softc = (struct ada_softc *)periph->softc;
	snprintf(tmpstr, sizeof(tmpstr), "CAM ADA unit %d", periph->unit_number);
	snprintf(tmpstr2, sizeof(tmpstr2), "%d", periph->unit_number);

	sysctl_ctx_init(&softc->sysctl_ctx);
	softc->flags |= ADA_FLAG_SCTX_INIT;
	softc->sysctl_tree = SYSCTL_ADD_NODE(&softc->sysctl_ctx,
		SYSCTL_STATIC_CHILDREN(_kern_cam_ada), OID_AUTO, tmpstr2,
		CTLFLAG_RD, 0, tmpstr);
	if (softc->sysctl_tree == NULL) {
		printf("adasysctlinit: unable to allocate sysctl tree\n");
		cam_periph_release(periph);
		return;
	}

	SYSCTL_ADD_INT(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "read_ahead", CTLFLAG_RW | CTLFLAG_MPSAFE,
		&softc->read_ahead, 0, "Enable disk read ahead.");
	SYSCTL_ADD_INT(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "write_cache", CTLFLAG_RW | CTLFLAG_MPSAFE,
		&softc->write_cache, 0, "Enable disk write cache.");
	SYSCTL_ADD_INT(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "sort_io_queue", CTLFLAG_RW | CTLFLAG_MPSAFE,
		&softc->sort_io_queue, 0,
		"Sort IO queue to try and optimise disk access patterns");
#ifdef ADA_TEST_FAILURE
	/*
	 * Add a 'door bell' sysctl which allows one to set it from userland
	 * and cause something bad to happen.  For the moment, we only allow
	 * whacking the next read or write.
	 */
	SYSCTL_ADD_INT(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "force_read_error", CTLFLAG_RW | CTLFLAG_MPSAFE,
		&softc->force_read_error, 0,
		"Force a read error for the next N reads.");
	SYSCTL_ADD_INT(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "force_write_error", CTLFLAG_RW | CTLFLAG_MPSAFE,
		&softc->force_write_error, 0,
		"Force a write error for the next N writes.");
	SYSCTL_ADD_INT(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "periodic_read_error", CTLFLAG_RW | CTLFLAG_MPSAFE,
		&softc->periodic_read_error, 0,
		"Force a read error every N reads (don't set too low).");
#endif
	cam_periph_release(periph);
}

static int
adagetattr(struct bio *bp)
{
	int ret;
	struct cam_periph *periph;

	periph = (struct cam_periph *)bp->bio_disk->d_drv1;
	cam_periph_lock(periph);
	ret = xpt_getattr(bp->bio_data, bp->bio_length, bp->bio_attribute,
	    periph->path);
	cam_periph_unlock(periph);
	if (ret == 0)
		bp->bio_completed = bp->bio_length;
	return ret;
}

static cam_status
adaregister(struct cam_periph *periph, void *arg)
{
	struct ada_softc *softc;
	struct ccb_pathinq cpi;
	struct ccb_getdev *cgd;
	char   announce_buf[80], buf1[32];
	struct disk_params *dp;
	caddr_t match;
	u_int maxio;
	int legacy_id, quirks;

	cgd = (struct ccb_getdev *)arg;
	if (cgd == NULL) {
		printf("adaregister: no getdev CCB, can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = (struct ada_softc *)malloc(sizeof(*softc), M_DEVBUF,
	    M_NOWAIT|M_ZERO);

	if (softc == NULL) {
		printf("adaregister: Unable to probe new device. "
		    "Unable to allocate softc\n");
		return(CAM_REQ_CMP_ERR);
	}

	bioq_init(&softc->bio_queue);
	bioq_init(&softc->trim_queue);

	if ((cgd->ident_data.capabilities1 & ATA_SUPPORT_DMA) &&
	    (cgd->inq_flags & SID_DMA))
		softc->flags |= ADA_FLAG_CAN_DMA;
	if (cgd->ident_data.support.command2 & ATA_SUPPORT_ADDRESS48) {
		softc->flags |= ADA_FLAG_CAN_48BIT;
		if (cgd->inq_flags & SID_DMA48)
			softc->flags |= ADA_FLAG_CAN_DMA48;
	}
	if (cgd->ident_data.support.command2 & ATA_SUPPORT_FLUSHCACHE)
		softc->flags |= ADA_FLAG_CAN_FLUSHCACHE;
	if (cgd->ident_data.support.command1 & ATA_SUPPORT_POWERMGT)
		softc->flags |= ADA_FLAG_CAN_POWERMGT;
	if ((cgd->ident_data.satacapabilities & ATA_SUPPORT_NCQ) &&
	    (cgd->inq_flags & SID_DMA) && (cgd->inq_flags & SID_CmdQue))
		softc->flags |= ADA_FLAG_CAN_NCQ;
	if ((cgd->ident_data.support_dsm & ATA_SUPPORT_DSM_TRIM) &&
	    (cgd->inq_flags & SID_DMA)) {
		softc->flags |= ADA_FLAG_CAN_TRIM;
		softc->trim_max_ranges = TRIM_MAX_RANGES;
		if (cgd->ident_data.max_dsm_blocks != 0) {
			softc->trim_max_ranges =
			    min(cgd->ident_data.max_dsm_blocks *
				ATA_DSM_BLK_RANGES, softc->trim_max_ranges);
		}
	}
	if (cgd->ident_data.support.command2 & ATA_SUPPORT_CFA)
		softc->flags |= ADA_FLAG_CAN_CFA;

	periph->softc = softc;

	/*
	 * See if this device has any quirks.
	 */
	match = cam_quirkmatch((caddr_t)&cgd->ident_data,
			       (caddr_t)ada_quirk_table,
			       sizeof(ada_quirk_table)/sizeof(*ada_quirk_table),
			       sizeof(*ada_quirk_table), ata_identify_match);
	if (match != NULL)
		softc->quirks = ((struct ada_quirk_entry *)match)->quirks;
	else
		softc->quirks = ADA_Q_NONE;

	bzero(&cpi, sizeof(cpi));
	xpt_setup_ccb(&cpi.ccb_h, periph->path, CAM_PRIORITY_NONE);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);

	TASK_INIT(&softc->sysctl_task, 0, adasysctlinit, periph);

	/*
	 * Register this media as a disk
	 */
	(void)cam_periph_hold(periph, PRIBIO);
	cam_periph_unlock(periph);
	snprintf(announce_buf, sizeof(announce_buf),
	    "kern.cam.ada.%d.quirks", periph->unit_number);
	quirks = softc->quirks;
	TUNABLE_INT_FETCH(announce_buf, &quirks);
	softc->quirks = quirks;
	softc->read_ahead = -1;
	snprintf(announce_buf, sizeof(announce_buf),
	    "kern.cam.ada.%d.read_ahead", periph->unit_number);
	TUNABLE_INT_FETCH(announce_buf, &softc->read_ahead);
	softc->write_cache = -1;
	snprintf(announce_buf, sizeof(announce_buf),
	    "kern.cam.ada.%d.write_cache", periph->unit_number);
	TUNABLE_INT_FETCH(announce_buf, &softc->write_cache);
	/* Disable queue sorting for non-rotational media by default. */
	if (cgd->ident_data.media_rotation_rate == ATA_RATE_NON_ROTATING)
		softc->sort_io_queue = 0;
	else
		softc->sort_io_queue = -1;
	adagetparams(periph, cgd);
	softc->disk = disk_alloc();
	softc->disk->d_rotation_rate = cgd->ident_data.media_rotation_rate;
	softc->disk->d_devstat = devstat_new_entry(periph->periph_name,
			  periph->unit_number, softc->params.secsize,
			  DEVSTAT_ALL_SUPPORTED,
			  DEVSTAT_TYPE_DIRECT |
			  XPORT_DEVSTAT_TYPE(cpi.transport),
			  DEVSTAT_PRIORITY_DISK);
	softc->disk->d_open = adaopen;
	softc->disk->d_close = adaclose;
	softc->disk->d_strategy = adastrategy;
	softc->disk->d_getattr = adagetattr;
	softc->disk->d_dump = adadump;
	softc->disk->d_gone = adadiskgonecb;
	softc->disk->d_name = "ada";
	softc->disk->d_drv1 = periph;
	maxio = cpi.maxio;		/* Honor max I/O size of SIM */
	if (maxio == 0)
		maxio = DFLTPHYS;	/* traditional default */
	else if (maxio > MAXPHYS)
		maxio = MAXPHYS;	/* for safety */
	if (softc->flags & ADA_FLAG_CAN_48BIT)
		maxio = min(maxio, 65536 * softc->params.secsize);
	else					/* 28bit ATA command limit */
		maxio = min(maxio, 256 * softc->params.secsize);
	softc->disk->d_maxsize = maxio;
	softc->disk->d_unit = periph->unit_number;
	softc->disk->d_flags = DISKFLAG_DIRECT_COMPLETION;
	if (softc->flags & ADA_FLAG_CAN_FLUSHCACHE)
		softc->disk->d_flags |= DISKFLAG_CANFLUSHCACHE;
	if (softc->flags & ADA_FLAG_CAN_TRIM) {
		softc->disk->d_flags |= DISKFLAG_CANDELETE;
		softc->disk->d_delmaxsize = softc->params.secsize *
					    ATA_DSM_RANGE_MAX *
					    softc->trim_max_ranges;
	} else if ((softc->flags & ADA_FLAG_CAN_CFA) &&
	    !(softc->flags & ADA_FLAG_CAN_48BIT)) {
		softc->disk->d_flags |= DISKFLAG_CANDELETE;
		softc->disk->d_delmaxsize = 256 * softc->params.secsize;
	} else
		softc->disk->d_delmaxsize = maxio;
	if ((cpi.hba_misc & PIM_UNMAPPED) != 0)
		softc->disk->d_flags |= DISKFLAG_UNMAPPED_BIO;
	strlcpy(softc->disk->d_descr, cgd->ident_data.model,
	    MIN(sizeof(softc->disk->d_descr), sizeof(cgd->ident_data.model)));
	strlcpy(softc->disk->d_ident, cgd->ident_data.serial,
	    MIN(sizeof(softc->disk->d_ident), sizeof(cgd->ident_data.serial)));
	softc->disk->d_hba_vendor = cpi.hba_vendor;
	softc->disk->d_hba_device = cpi.hba_device;
	softc->disk->d_hba_subvendor = cpi.hba_subvendor;
	softc->disk->d_hba_subdevice = cpi.hba_subdevice;

	softc->disk->d_sectorsize = softc->params.secsize;
	softc->disk->d_mediasize = (off_t)softc->params.sectors *
	    softc->params.secsize;
	if (ata_physical_sector_size(&cgd->ident_data) !=
	    softc->params.secsize) {
		softc->disk->d_stripesize =
		    ata_physical_sector_size(&cgd->ident_data);
		softc->disk->d_stripeoffset = (softc->disk->d_stripesize -
		    ata_logical_sector_offset(&cgd->ident_data)) %
		    softc->disk->d_stripesize;
	} else if (softc->quirks & ADA_Q_4K) {
		softc->disk->d_stripesize = 4096;
		softc->disk->d_stripeoffset = 0;
	}
	softc->disk->d_fwsectors = softc->params.secs_per_track;
	softc->disk->d_fwheads = softc->params.heads;
	ata_disk_firmware_geom_adjust(softc->disk);

	if (ada_legacy_aliases) {
#ifdef ATA_STATIC_ID
		legacy_id = xpt_path_legacy_ata_id(periph->path);
#else
		legacy_id = softc->disk->d_unit;
#endif
		if (legacy_id >= 0) {
			snprintf(announce_buf, sizeof(announce_buf),
			    "kern.devalias.%s%d",
			    softc->disk->d_name, softc->disk->d_unit);
			snprintf(buf1, sizeof(buf1),
			    "ad%d", legacy_id);
			setenv(announce_buf, buf1);
		}
	} else
		legacy_id = -1;
	/*
	 * Acquire a reference to the periph before we register with GEOM.
	 * We'll release this reference once GEOM calls us back (via
	 * adadiskgonecb()) telling us that our provider has been freed.
	 */
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: lost periph during "
			  "registration!\n", __func__);
		cam_periph_lock(periph);
		return (CAM_REQ_CMP_ERR);
	}
	disk_create(softc->disk, DISK_VERSION);
	cam_periph_lock(periph);
	cam_periph_unhold(periph);

	dp = &softc->params;
	snprintf(announce_buf, sizeof(announce_buf),
		"%juMB (%ju %u byte sectors: %dH %dS/T %dC)",
		(uintmax_t)(((uintmax_t)dp->secsize *
		dp->sectors) / (1024*1024)),
		(uintmax_t)dp->sectors,
		dp->secsize, dp->heads,
		dp->secs_per_track, dp->cylinders);
	xpt_announce_periph(periph, announce_buf);
	xpt_announce_quirks(periph, softc->quirks, ADA_Q_BIT_STRING);
	if (legacy_id >= 0)
		printf("%s%d: Previously was known as ad%d\n",
		       periph->periph_name, periph->unit_number, legacy_id);

	/*
	 * Create our sysctl variables, now that we know
	 * we have successfully attached.
	 */
	if (cam_periph_acquire(periph) == CAM_REQ_CMP)
		taskqueue_enqueue(taskqueue_thread, &softc->sysctl_task);

	/*
	 * Add async callbacks for bus reset and
	 * bus device reset calls.  I don't bother
	 * checking if this fails as, in most cases,
	 * the system will function just fine without
	 * them and the only alternative would be to
	 * not attach the device on failure.
	 */
	xpt_register_async(AC_SENT_BDR | AC_BUS_RESET | AC_LOST_DEVICE |
	    AC_GETDEV_CHANGED | AC_ADVINFO_CHANGED,
	    adaasync, periph, periph->path);

	/*
	 * Schedule a periodic event to occasionally send an
	 * ordered tag to a device.
	 */
	callout_init_mtx(&softc->sendordered_c, cam_periph_mtx(periph), 0);
	callout_reset(&softc->sendordered_c,
	    (ada_default_timeout * hz) / ADA_ORDEREDTAG_INTERVAL,
	    adasendorderedtag, softc);

	if (ADA_RA >= 0 &&
	    cgd->ident_data.support.command1 & ATA_SUPPORT_LOOKAHEAD) {
		softc->state = ADA_STATE_RAHEAD;
	} else if (ADA_WC >= 0 &&
	    cgd->ident_data.support.command1 & ATA_SUPPORT_WRITECACHE) {
		softc->state = ADA_STATE_WCACHE;
	} else {
		softc->state = ADA_STATE_NORMAL;
		return(CAM_REQ_CMP);
	}
	if (cam_periph_acquire(periph) != CAM_REQ_CMP)
		softc->state = ADA_STATE_NORMAL;
	else
		xpt_schedule(periph, CAM_PRIORITY_DEV);
	return(CAM_REQ_CMP);
}

static void
adastart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct ada_softc *softc = (struct ada_softc *)periph->softc;
	struct ccb_ataio *ataio = &start_ccb->ataio;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("adastart\n"));

	switch (softc->state) {
	case ADA_STATE_NORMAL:
	{
		struct bio *bp;
		u_int8_t tag_code;

		/* Run TRIM if not running yet. */
		if (!softc->trim_running &&
		    (bp = bioq_first(&softc->trim_queue)) != 0) {
			struct trim_request *req = &softc->trim_req;
			struct bio *bp1;
			uint64_t lastlba = (uint64_t)-1;
			int c, lastcount = 0, off, ranges = 0;

			softc->trim_running = 1;
			bzero(req, sizeof(*req));
			TAILQ_INIT(&req->bps);
			bp1 = bp;
			do {
				uint64_t lba = bp1->bio_pblkno;
				int count = bp1->bio_bcount /
				    softc->params.secsize;

				bioq_remove(&softc->trim_queue, bp1);

				/* Try to extend the previous range. */
				if (lba == lastlba) {
					c = min(count, ATA_DSM_RANGE_MAX - lastcount);
					lastcount += c;
					off = (ranges - 1) * ATA_DSM_RANGE_SIZE;
					req->data[off + 6] = lastcount & 0xff;
					req->data[off + 7] =
					    (lastcount >> 8) & 0xff;
					count -= c;
					lba += c;
				}

				while (count > 0) {
					c = min(count, ATA_DSM_RANGE_MAX);
					off = ranges * ATA_DSM_RANGE_SIZE;
					req->data[off + 0] = lba & 0xff;
					req->data[off + 1] = (lba >> 8) & 0xff;
					req->data[off + 2] = (lba >> 16) & 0xff;
					req->data[off + 3] = (lba >> 24) & 0xff;
					req->data[off + 4] = (lba >> 32) & 0xff;
					req->data[off + 5] = (lba >> 40) & 0xff;
					req->data[off + 6] = c & 0xff;
					req->data[off + 7] = (c >> 8) & 0xff;
					lba += c;
					count -= c;
					lastcount = c;
					ranges++;
					/*
					 * Its the caller's responsibility to ensure the
					 * request will fit so we don't need to check for
					 * overrun here
					 */
				}
				lastlba = lba;
				TAILQ_INSERT_TAIL(&req->bps, bp1, bio_queue);
				bp1 = bioq_first(&softc->trim_queue);
				if (bp1 == NULL ||
				    bp1->bio_bcount / softc->params.secsize >
				    (softc->trim_max_ranges - ranges) *
				    ATA_DSM_RANGE_MAX)
					break;
			} while (1);
			cam_fill_ataio(ataio,
			    ada_retry_count,
			    adadone,
			    CAM_DIR_OUT,
			    0,
			    req->data,
			    ((ranges + ATA_DSM_BLK_RANGES - 1) /
			        ATA_DSM_BLK_RANGES) * ATA_DSM_BLK_SIZE,
			    ada_default_timeout * 1000);
			ata_48bit_cmd(ataio, ATA_DATA_SET_MANAGEMENT,
			    ATA_DSM_TRIM, 0, (ranges + ATA_DSM_BLK_RANGES -
			    1) / ATA_DSM_BLK_RANGES);
			start_ccb->ccb_h.ccb_state = ADA_CCB_TRIM;
			start_ccb->ccb_h.flags |= CAM_UNLOCKED;
			goto out;
		}
		/* Run regular command. */
		bp = bioq_first(&softc->bio_queue);
		if (bp == NULL) {
			xpt_release_ccb(start_ccb);
			break;
		}
		bioq_remove(&softc->bio_queue, bp);

		if ((bp->bio_flags & BIO_ORDERED) != 0
		 || (softc->flags & ADA_FLAG_NEED_OTAG) != 0) {
			softc->flags &= ~ADA_FLAG_NEED_OTAG;
			softc->flags |= ADA_FLAG_WAS_OTAG;
			tag_code = 0;
		} else {
			tag_code = 1;
		}
		switch (bp->bio_cmd) {
		case BIO_WRITE:
			softc->flags |= ADA_FLAG_DIRTY;
			/* FALLTHROUGH */
		case BIO_READ:
		{
			uint64_t lba = bp->bio_pblkno;
			uint16_t count = bp->bio_bcount / softc->params.secsize;
#ifdef ADA_TEST_FAILURE
			int fail = 0;

			/*
			 * Support the failure ioctls.  If the command is a
			 * read, and there are pending forced read errors, or
			 * if a write and pending write errors, then fail this
			 * operation with EIO.  This is useful for testing
			 * purposes.  Also, support having every Nth read fail.
			 *
			 * This is a rather blunt tool.
			 */
			if (bp->bio_cmd == BIO_READ) {
				if (softc->force_read_error) {
					softc->force_read_error--;
					fail = 1;
				}
				if (softc->periodic_read_error > 0) {
					if (++softc->periodic_read_count >=
					    softc->periodic_read_error) {
						softc->periodic_read_count = 0;
						fail = 1;
					}
				}
			} else {
				if (softc->force_write_error) {
					softc->force_write_error--;
					fail = 1;
				}
			}
			if (fail) {
				bp->bio_error = EIO;
				bp->bio_flags |= BIO_ERROR;
				biodone(bp);
				xpt_release_ccb(start_ccb);
				adaschedule(periph);
				return;
			}
#endif
			KASSERT((bp->bio_flags & BIO_UNMAPPED) == 0 ||
			    round_page(bp->bio_bcount + bp->bio_ma_offset) /
			    PAGE_SIZE == bp->bio_ma_n,
			    ("Short bio %p", bp));
			cam_fill_ataio(ataio,
			    ada_retry_count,
			    adadone,
			    (bp->bio_cmd == BIO_READ ? CAM_DIR_IN :
				CAM_DIR_OUT) | ((bp->bio_flags & BIO_UNMAPPED)
				!= 0 ? CAM_DATA_BIO : 0),
			    tag_code,
			    ((bp->bio_flags & BIO_UNMAPPED) != 0) ? (void *)bp :
				bp->bio_data,
			    bp->bio_bcount,
			    ada_default_timeout*1000);

			if ((softc->flags & ADA_FLAG_CAN_NCQ) && tag_code) {
				if (bp->bio_cmd == BIO_READ) {
					ata_ncq_cmd(ataio, ATA_READ_FPDMA_QUEUED,
					    lba, count);
				} else {
					ata_ncq_cmd(ataio, ATA_WRITE_FPDMA_QUEUED,
					    lba, count);
				}
			} else if ((softc->flags & ADA_FLAG_CAN_48BIT) &&
			    (lba + count >= ATA_MAX_28BIT_LBA ||
			    count > 256)) {
				if (softc->flags & ADA_FLAG_CAN_DMA48) {
					if (bp->bio_cmd == BIO_READ) {
						ata_48bit_cmd(ataio, ATA_READ_DMA48,
						    0, lba, count);
					} else {
						ata_48bit_cmd(ataio, ATA_WRITE_DMA48,
						    0, lba, count);
					}
				} else {
					if (bp->bio_cmd == BIO_READ) {
						ata_48bit_cmd(ataio, ATA_READ_MUL48,
						    0, lba, count);
					} else {
						ata_48bit_cmd(ataio, ATA_WRITE_MUL48,
						    0, lba, count);
					}
				}
			} else {
				if (count == 256)
					count = 0;
				if (softc->flags & ADA_FLAG_CAN_DMA) {
					if (bp->bio_cmd == BIO_READ) {
						ata_28bit_cmd(ataio, ATA_READ_DMA,
						    0, lba, count);
					} else {
						ata_28bit_cmd(ataio, ATA_WRITE_DMA,
						    0, lba, count);
					}
				} else {
					if (bp->bio_cmd == BIO_READ) {
						ata_28bit_cmd(ataio, ATA_READ_MUL,
						    0, lba, count);
					} else {
						ata_28bit_cmd(ataio, ATA_WRITE_MUL,
						    0, lba, count);
					}
				}
			}
			break;
		}
		case BIO_DELETE:
		{
			uint64_t lba = bp->bio_pblkno;
			uint16_t count = bp->bio_bcount / softc->params.secsize;

			cam_fill_ataio(ataio,
			    ada_retry_count,
			    adadone,
			    CAM_DIR_NONE,
			    0,
			    NULL,
			    0,
			    ada_default_timeout*1000);

			if (count >= 256)
				count = 0;
			ata_28bit_cmd(ataio, ATA_CFA_ERASE, 0, lba, count);
			break;
		}
		case BIO_FLUSH:
			cam_fill_ataio(ataio,
			    1,
			    adadone,
			    CAM_DIR_NONE,
			    0,
			    NULL,
			    0,
			    ada_default_timeout*1000);

			if (softc->flags & ADA_FLAG_CAN_48BIT)
				ata_48bit_cmd(ataio, ATA_FLUSHCACHE48, 0, 0, 0);
			else
				ata_28bit_cmd(ataio, ATA_FLUSHCACHE, 0, 0, 0);
			break;
		}
		start_ccb->ccb_h.ccb_state = ADA_CCB_BUFFER_IO;
		start_ccb->ccb_h.flags |= CAM_UNLOCKED;
out:
		start_ccb->ccb_h.ccb_bp = bp;
		softc->outstanding_cmds++;
		softc->refcount++;
		cam_periph_unlock(periph);
		xpt_action(start_ccb);
		cam_periph_lock(periph);
		softc->refcount--;

		/* May have more work to do, so ensure we stay scheduled */
		adaschedule(periph);
		break;
	}
	case ADA_STATE_RAHEAD:
	case ADA_STATE_WCACHE:
	{
		cam_fill_ataio(ataio,
		    1,
		    adadone,
		    CAM_DIR_NONE,
		    0,
		    NULL,
		    0,
		    ada_default_timeout*1000);

		if (softc->state == ADA_STATE_RAHEAD) {
			ata_28bit_cmd(ataio, ATA_SETFEATURES, ADA_RA ?
			    ATA_SF_ENAB_RCACHE : ATA_SF_DIS_RCACHE, 0, 0);
			start_ccb->ccb_h.ccb_state = ADA_CCB_RAHEAD;
		} else {
			ata_28bit_cmd(ataio, ATA_SETFEATURES, ADA_WC ?
			    ATA_SF_ENAB_WCACHE : ATA_SF_DIS_WCACHE, 0, 0);
			start_ccb->ccb_h.ccb_state = ADA_CCB_WCACHE;
		}
		start_ccb->ccb_h.flags |= CAM_DEV_QFREEZE;
		xpt_action(start_ccb);
		break;
	}
	}
}

static void
adadone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct ada_softc *softc;
	struct ccb_ataio *ataio;
	struct ccb_getdev *cgd;
	struct cam_path *path;
	int state;

	softc = (struct ada_softc *)periph->softc;
	ataio = &done_ccb->ataio;
	path = done_ccb->ccb_h.path;

	CAM_DEBUG(path, CAM_DEBUG_TRACE, ("adadone\n"));

	state = ataio->ccb_h.ccb_state & ADA_CCB_TYPE_MASK;
	switch (state) {
	case ADA_CCB_BUFFER_IO:
	case ADA_CCB_TRIM:
	{
		struct bio *bp;
		int error;

		cam_periph_lock(periph);
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			error = adaerror(done_ccb, 0, 0);
			if (error == ERESTART) {
				/* A retry was scheduled, so just return. */
				cam_periph_unlock(periph);
				return;
			}
			if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
				cam_release_devq(path,
						 /*relsim_flags*/0,
						 /*reduction*/0,
						 /*timeout*/0,
						 /*getcount_only*/0);
		} else {
			if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
				panic("REQ_CMP with QFRZN");
			error = 0;
		}
		bp = (struct bio *)done_ccb->ccb_h.ccb_bp;
		bp->bio_error = error;
		if (error != 0) {
			bp->bio_resid = bp->bio_bcount;
			bp->bio_flags |= BIO_ERROR;
		} else {
			if (state == ADA_CCB_TRIM)
				bp->bio_resid = 0;
			else
				bp->bio_resid = ataio->resid;
			if (bp->bio_resid > 0)
				bp->bio_flags |= BIO_ERROR;
		}
		softc->outstanding_cmds--;
		if (softc->outstanding_cmds == 0)
			softc->flags |= ADA_FLAG_WAS_OTAG;
		xpt_release_ccb(done_ccb);
		if (state == ADA_CCB_TRIM) {
			TAILQ_HEAD(, bio) queue;
			struct bio *bp1;

			TAILQ_INIT(&queue);
			TAILQ_CONCAT(&queue, &softc->trim_req.bps, bio_queue);
			softc->trim_running = 0;
			adaschedule(periph);
			cam_periph_unlock(periph);
			while ((bp1 = TAILQ_FIRST(&queue)) != NULL) {
				TAILQ_REMOVE(&queue, bp1, bio_queue);
				bp1->bio_error = error;
				if (error != 0) {
					bp1->bio_flags |= BIO_ERROR;
					bp1->bio_resid = bp1->bio_bcount;
				} else
					bp1->bio_resid = 0;
				biodone(bp1);
			}
		} else {
			cam_periph_unlock(periph);
			biodone(bp);
		}
		return;
	}
	case ADA_CCB_RAHEAD:
	{
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			if (adaerror(done_ccb, 0, 0) == ERESTART) {
out:
				/* Drop freeze taken due to CAM_DEV_QFREEZE */
				cam_release_devq(path, 0, 0, 0, FALSE);
				return;
			} else if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
				cam_release_devq(path,
				    /*relsim_flags*/0,
				    /*reduction*/0,
				    /*timeout*/0,
				    /*getcount_only*/0);
			}
		}

		/*
		 * Since our peripheral may be invalidated by an error
		 * above or an external event, we must release our CCB
		 * before releasing the reference on the peripheral.
		 * The peripheral will only go away once the last reference
		 * is removed, and we need it around for the CCB release
		 * operation.
		 */
		cgd = (struct ccb_getdev *)done_ccb;
		xpt_setup_ccb(&cgd->ccb_h, path, CAM_PRIORITY_NORMAL);
		cgd->ccb_h.func_code = XPT_GDEV_TYPE;
		xpt_action((union ccb *)cgd);
		if (ADA_WC >= 0 &&
		    cgd->ident_data.support.command1 & ATA_SUPPORT_WRITECACHE) {
			softc->state = ADA_STATE_WCACHE;
			xpt_release_ccb(done_ccb);
			xpt_schedule(periph, CAM_PRIORITY_DEV);
			goto out;
		}
		softc->state = ADA_STATE_NORMAL;
		xpt_release_ccb(done_ccb);
		/* Drop freeze taken due to CAM_DEV_QFREEZE */
		cam_release_devq(path, 0, 0, 0, FALSE);
		adaschedule(periph);
		cam_periph_release_locked(periph);
		return;
	}
	case ADA_CCB_WCACHE:
	{
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			if (adaerror(done_ccb, 0, 0) == ERESTART) {
				goto out;
			} else if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
				cam_release_devq(path,
				    /*relsim_flags*/0,
				    /*reduction*/0,
				    /*timeout*/0,
				    /*getcount_only*/0);
			}
		}

		softc->state = ADA_STATE_NORMAL;
		/*
		 * Since our peripheral may be invalidated by an error
		 * above or an external event, we must release our CCB
		 * before releasing the reference on the peripheral.
		 * The peripheral will only go away once the last reference
		 * is removed, and we need it around for the CCB release
		 * operation.
		 */
		xpt_release_ccb(done_ccb);
		/* Drop freeze taken due to CAM_DEV_QFREEZE */
		cam_release_devq(path, 0, 0, 0, FALSE);
		adaschedule(periph);
		cam_periph_release_locked(periph);
		return;
	}
	case ADA_CCB_DUMP:
		/* No-op.  We're polling */
		return;
	default:
		break;
	}
	xpt_release_ccb(done_ccb);
}

static int
adaerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{

	return(cam_periph_error(ccb, cam_flags, sense_flags, NULL));
}

static void
adagetparams(struct cam_periph *periph, struct ccb_getdev *cgd)
{
	struct ada_softc *softc = (struct ada_softc *)periph->softc;
	struct disk_params *dp = &softc->params;
	u_int64_t lbasize48;
	u_int32_t lbasize;

	dp->secsize = ata_logical_sector_size(&cgd->ident_data);
	if ((cgd->ident_data.atavalid & ATA_FLAG_54_58) &&
		cgd->ident_data.current_heads && cgd->ident_data.current_sectors) {
		dp->heads = cgd->ident_data.current_heads;
		dp->secs_per_track = cgd->ident_data.current_sectors;
		dp->cylinders = cgd->ident_data.cylinders;
		dp->sectors = (u_int32_t)cgd->ident_data.current_size_1 |
			  ((u_int32_t)cgd->ident_data.current_size_2 << 16);
	} else {
		dp->heads = cgd->ident_data.heads;
		dp->secs_per_track = cgd->ident_data.sectors;
		dp->cylinders = cgd->ident_data.cylinders;
		dp->sectors = cgd->ident_data.cylinders * dp->heads * dp->secs_per_track;  
	}
	lbasize = (u_int32_t)cgd->ident_data.lba_size_1 |
		  ((u_int32_t)cgd->ident_data.lba_size_2 << 16);

	/* use the 28bit LBA size if valid or bigger than the CHS mapping */
	if (cgd->ident_data.cylinders == 16383 || dp->sectors < lbasize)
		dp->sectors = lbasize;

	/* use the 48bit LBA size if valid */
	lbasize48 = ((u_int64_t)cgd->ident_data.lba_size48_1) |
		    ((u_int64_t)cgd->ident_data.lba_size48_2 << 16) |
		    ((u_int64_t)cgd->ident_data.lba_size48_3 << 32) |
		    ((u_int64_t)cgd->ident_data.lba_size48_4 << 48);
	if ((cgd->ident_data.support.command2 & ATA_SUPPORT_ADDRESS48) &&
	    lbasize48 > ATA_MAX_28BIT_LBA)
		dp->sectors = lbasize48;
}

static void
adasendorderedtag(void *arg)
{
	struct ada_softc *softc = arg;

	if (ada_send_ordered) {
		if (softc->outstanding_cmds > 0) {
			if ((softc->flags & ADA_FLAG_WAS_OTAG) == 0)
				softc->flags |= ADA_FLAG_NEED_OTAG;
			softc->flags &= ~ADA_FLAG_WAS_OTAG;
		}
	}
	/* Queue us up again */
	callout_reset(&softc->sendordered_c,
	    (ada_default_timeout * hz) / ADA_ORDEREDTAG_INTERVAL,
	    adasendorderedtag, softc);
}

/*
 * Step through all ADA peripheral drivers, and if the device is still open,
 * sync the disk cache to physical media.
 */
static void
adaflush(void)
{
	struct cam_periph *periph;
	struct ada_softc *softc;
	union ccb *ccb;
	int error;

	CAM_PERIPH_FOREACH(periph, &adadriver) {
		softc = (struct ada_softc *)periph->softc;
		if (SCHEDULER_STOPPED()) {
			/* If we paniced with the lock held, do not recurse. */
			if (!cam_periph_owned(periph) &&
			    (softc->flags & ADA_FLAG_OPEN)) {
				adadump(softc->disk, NULL, 0, 0, 0);
			}
			continue;
		}
		cam_periph_lock(periph);
		/*
		 * We only sync the cache if the drive is still open, and
		 * if the drive is capable of it..
		 */
		if (((softc->flags & ADA_FLAG_OPEN) == 0) ||
		    (softc->flags & ADA_FLAG_CAN_FLUSHCACHE) == 0) {
			cam_periph_unlock(periph);
			continue;
		}

		ccb = cam_periph_getccb(periph, CAM_PRIORITY_NORMAL);
		cam_fill_ataio(&ccb->ataio,
				    0,
				    adadone,
				    CAM_DIR_NONE,
				    0,
				    NULL,
				    0,
				    ada_default_timeout*1000);
		if (softc->flags & ADA_FLAG_CAN_48BIT)
			ata_48bit_cmd(&ccb->ataio, ATA_FLUSHCACHE48, 0, 0, 0);
		else
			ata_28bit_cmd(&ccb->ataio, ATA_FLUSHCACHE, 0, 0, 0);

		error = cam_periph_runccb(ccb, adaerror, /*cam_flags*/0,
		    /*sense_flags*/ SF_NO_RECOVERY | SF_NO_RETRY,
		    softc->disk->d_devstat);
		if (error != 0)
			xpt_print(periph->path, "Synchronize cache failed\n");
		xpt_release_ccb(ccb);
		cam_periph_unlock(periph);
	}
}

static void
adaspindown(uint8_t cmd, int flags)
{
	struct cam_periph *periph;
	struct ada_softc *softc;
	union ccb *ccb;
	int error;

	CAM_PERIPH_FOREACH(periph, &adadriver) {
		/* If we paniced with lock held - not recurse here. */
		if (cam_periph_owned(periph))
			continue;
		cam_periph_lock(periph);
		softc = (struct ada_softc *)periph->softc;
		/*
		 * We only spin-down the drive if it is capable of it..
		 */
		if ((softc->flags & ADA_FLAG_CAN_POWERMGT) == 0) {
			cam_periph_unlock(periph);
			continue;
		}

		if (bootverbose)
			xpt_print(periph->path, "spin-down\n");

		ccb = cam_periph_getccb(periph, CAM_PRIORITY_NORMAL);
		cam_fill_ataio(&ccb->ataio,
				    0,
				    adadone,
				    CAM_DIR_NONE | flags,
				    0,
				    NULL,
				    0,
				    ada_default_timeout*1000);
		ata_28bit_cmd(&ccb->ataio, cmd, 0, 0, 0);

		error = cam_periph_runccb(ccb, adaerror, /*cam_flags*/0,
		    /*sense_flags*/ SF_NO_RECOVERY | SF_NO_RETRY,
		    softc->disk->d_devstat);
		if (error != 0)
			xpt_print(periph->path, "Spin-down disk failed\n");
		xpt_release_ccb(ccb);
		cam_periph_unlock(periph);
	}
}

static void
adashutdown(void *arg, int howto)
{

	adaflush();
	if (ada_spindown_shutdown != 0 &&
	    (howto & (RB_HALT | RB_POWEROFF)) != 0)
		adaspindown(ATA_STANDBY_IMMEDIATE, 0);
}

static void
adasuspend(void *arg)
{

	adaflush();
	if (ada_spindown_suspend != 0)
		adaspindown(ATA_SLEEP, CAM_DEV_QFREEZE);
}

static void
adaresume(void *arg)
{
	struct cam_periph *periph;
	struct ada_softc *softc;

	if (ada_spindown_suspend == 0)
		return;

	CAM_PERIPH_FOREACH(periph, &adadriver) {
		cam_periph_lock(periph);
		softc = (struct ada_softc *)periph->softc;
		/*
		 * We only spin-down the drive if it is capable of it..
		 */
		if ((softc->flags & ADA_FLAG_CAN_POWERMGT) == 0) {
			cam_periph_unlock(periph);
			continue;
		}

		if (bootverbose)
			xpt_print(periph->path, "resume\n");

		/*
		 * Drop freeze taken due to CAM_DEV_QFREEZE flag set on
		 * sleep request.
		 */
		cam_release_devq(periph->path,
			 /*relsim_flags*/0,
			 /*openings*/0,
			 /*timeout*/0,
			 /*getcount_only*/0);
		
		cam_periph_unlock(periph);
	}
}

#endif /* _KERNEL */
