/*
 * Implementation of SCSI Direct Access Peripheral driver for CAM.
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include "opt_da.h"
#include "opt_hw_wdog.h"
#endif /* _KERNEL */

#include <sys/param.h>

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#endif /* _KERNEL */

#include <sys/devicestat.h>
#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/cons.h>

#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <geom/geom_disk.h>

#ifndef _KERNEL
#include <stdio.h>
#include <string.h>
#endif /* _KERNEL */

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>

#include <cam/scsi/scsi_message.h>

#ifndef _KERNEL 
#include <cam/scsi/scsi_da.h>
#endif /* !_KERNEL */

#ifdef _KERNEL
typedef enum {
	DA_STATE_PROBE,
	DA_STATE_PROBE2,
	DA_STATE_NORMAL
} da_state;

typedef enum {
	DA_FLAG_PACK_INVALID	= 0x001,
	DA_FLAG_NEW_PACK	= 0x002,
	DA_FLAG_PACK_LOCKED	= 0x004,
	DA_FLAG_PACK_REMOVABLE	= 0x008,
	DA_FLAG_TAGGED_QUEUING	= 0x010,
	DA_FLAG_NEED_OTAG	= 0x020,
	DA_FLAG_WENT_IDLE	= 0x040,
	DA_FLAG_RETRY_UA	= 0x080,
	DA_FLAG_OPEN		= 0x100
} da_flags;

typedef enum {
	DA_Q_NONE		= 0x00,
	DA_Q_NO_SYNC_CACHE	= 0x01,
	DA_Q_NO_6_BYTE		= 0x02
} da_quirks;

typedef enum {
	DA_CCB_PROBE		= 0x01,
	DA_CCB_PROBE2		= 0x02,
	DA_CCB_BUFFER_IO	= 0x03,
	DA_CCB_WAITING		= 0x04,
	DA_CCB_DUMP		= 0x05,
	DA_CCB_TYPE_MASK	= 0x0F,
	DA_CCB_RETRY_UA		= 0x10
} da_ccb_state;

/* Offsets into our private area for storing information */
#define ccb_state	ppriv_field0
#define ccb_bp		ppriv_ptr1

struct disk_params {
	u_int8_t  heads;
	u_int32_t cylinders;
	u_int8_t  secs_per_track;
	u_int32_t secsize;	/* Number of bytes/sector */
	u_int64_t sectors;	/* total number sectors */
};

struct da_softc {
	struct	 bio_queue_head bio_queue;
	SLIST_ENTRY(da_softc) links;
	LIST_HEAD(, ccb_hdr) pending_ccbs;
	da_state state;
	da_flags flags;	
	da_quirks quirks;
	int	 minimum_cmd_size;
	int	 ordered_tag_count;
	int	 outstanding_cmds;
	struct	 disk_params params;
	struct	 disk disk;
	union	 ccb saved_ccb;
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
};

struct da_quirk_entry {
	struct scsi_inquiry_pattern inq_pat;
	da_quirks quirks;
};

static const char quantum[] = "QUANTUM";
static const char microp[] = "MICROP";

static struct da_quirk_entry da_quirk_table[] =
{
#ifdef DA_OLD_QUIRKS
	/*
	 * Logitec USB/Firewire LHD-P30FU
	 */
	{
		/* USB part */
		{T_DIRECT, SIP_MEDIA_FIXED, "HITACHI_", "DK23DA*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/* Firewire part */
		{T_DIRECT, SIP_MEDIA_FIXED, "LSILogic", "SYM13FW*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
#endif /* DA_OLD_QUIRKS */
	{
		/*
		 * Fujitsu M2513A MO drives.
		 * Tested devices: M2513A2 firmware versions 1200 & 1300.
		 * (dip switch selects whether T_DIRECT or T_OPTICAL device)
		 * Reported by: W.Scholten <whs@xs4all.nl>
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "FUJITSU", "M2513A", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/* See above. */
		{T_OPTICAL, SIP_MEDIA_REMOVABLE, "FUJITSU", "M2513A", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * This particular Fujitsu drive doesn't like the
		 * synchronize cache command.
		 * Reported by: Tom Jackson <toj@gorilla.net>
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "FUJITSU", "M2954*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	
	},
	{
		/*
		 * This drive doesn't like the synchronize cache command
		 * either.  Reported by: Matthew Jacob <mjacob@feral.com>
		 * in NetBSD PR kern/6027, August 24, 1998.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, microp, "2217*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * This drive doesn't like the synchronize cache command
		 * either.  Reported by: Hellmuth Michaelis (hm@kts.org)
		 * (PR 8882).
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, microp, "2112*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 * Reported by: Blaz Zupan <blaz@gold.amis.net>
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "NEC", "D3847*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "MAVERICK 540S", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "LPS525S", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't work correctly with 6 byte reads/writes.
		 * Returns illegal request, and points to byte 9 of the
		 * 6-byte CDB.
		 * Reported by:  Adam McDougall <bsdx@spawnet.com>
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "VIKING 4*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * See above.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "VIKING 2*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},

#ifdef DA_OLD_QUIRKS
	/* Below a list of quirks for USB devices supported by umass. */
	{
		/*
		 * This USB floppy drive uses the UFI command set. This
		 * command set is a derivative of the ATAPI command set and
		 * does not support READ_6 commands only READ_10. It also does
		 * not support sync cache (0x35).
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Y-E DATA", "USB-FDU", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/* Another USB floppy */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "MATSHITA", "FDD CF-VFDU*","*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Sony Memory Stick adapter MSAC-US1 and
		 * Sony PCG-C1VJ Internal Memory Stick Slot (MSC-U01).
		 * Make all sony MS* products use this quirk.
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Sony", "MS*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Sony Memory Stick adapter for the CLIE series
		 * of PalmOS PDA's
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Sony", "CLIE*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Intelligent Stick USB disk-on-key
		 * PR: kern/53005
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "USB Card",
		 "IntelligentStick*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Sony DSC cameras (DSC-S30, DSC-S50, DSC-S70)
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Sony", "Sony DSC", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Maxtor 3000LE USB Drive
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "MAXTOR*", "K040H2*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * LaCie USB drive, among others
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "Maxtor*", "D080H4*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		{T_OPTICAL, SIP_MEDIA_REMOVABLE, "FUJITSU", "MCF3064AP", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Microtech USB CameraMate
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "eUSB    Compact*",
		 "Compact Flash*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * The vendor, product and version strings coming from the
		 * controller are null terminated instead of being padded with
		 * spaces. The trailing wildcard character '*' is required.
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "SMSC*", "USB FDC*","*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Olympus digital cameras (C-3040ZOOM, C-2040ZOOM, C-1)
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "OLYMPUS", "C-*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Olympus digital cameras (D-370)
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "OLYMPUS", "D-*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Olympus digital cameras (E-100RS, E-10).
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "OLYMPUS", "E-*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * KingByte Pen Drives
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "NO BRAND", "PEN DRIVE", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
 	},
 	{
		/*
		 * FujiFilm Camera
		 */
 		{T_DIRECT, SIP_MEDIA_REMOVABLE, "FUJIFILMUSB-DRIVEUNIT",
		 "USB-DRIVEUNIT", "*"},
 		/*quirks*/ DA_Q_NO_SYNC_CACHE
 	},
	{
		/*
		 * Nikon Coolpix E775/E995 Cameras 
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "NIKON", "NIKON DSC E*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Nikon Coolpix E885 Camera
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Nikon", "Digital Camera", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * SimpleTech FlashLink UCF-100
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "OEI-USB", "CompactFlash", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Minolta Dimage 2330
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "MINOLTA", "DIMAGE 2330*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Minolta Dimage E203
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "MINOLTA", "DiMAGE E203", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * DIVA USB Mp3 Player.
		 * PR: kern/33638
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "DIVA USB", "Media Reader","*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Daisy Technology PhotoClip USB Camera
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Digital", "World   DMC","*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Apacer HandyDrive
		 * PR: kern/43627
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Apacer", "HandyDrive", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Daisy Technology PhotoClip on Zoran chip
		 * PR: kern/43580
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "ZORAN", "COACH", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * HP 315 Digital Camera
		 * PR: kern/41010
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "HP", "USB CAMERA", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Fujitsu-Siemens Memorybird pen drive
		 * PR: kern/34712
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Fujitsu", "Memorybird", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Sony USB Key-Storage
		 * PR: kern/46386
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Sony", "Storage Media", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Lexar Media Jumpdrive
		 * PR: kern/47006
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "LEXAR", "DIGITAL FILM", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Pentax USB Optio 230 camera
		 * PR: kern/46369
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE,
		 "PENTAX", "DIGITAL_CAMERA", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Casio QV-R3 USB camera (uses Pentax chip as above)
		 * PR: kern/46545
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE,
		 "CASIO", "DIGITAL_CAMERA", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * M-Systems DiskOnKey USB flash key
		 * PR: kern/47793
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "M-Sys", "DiskOnKey", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * SanDisk ImageMate (I, II, ...) compact flash
		 * PR: kern/47877
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "SanDisk", "ImageMate*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Feiya "slider" dual-slot flash reader. The vendor field
		 * is blank so this may match other devices.
		 * PR: kern/50020
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "", "USB CARD READER", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * SmartDisk (Mitsumi) USB floppy drive
		 * PR: kern/50226
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "MITSUMI", "USB FDD", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * OTi USB Flash Key
		 * PR: kern/51825
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "OTi", "Flash Disk", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	}
#endif /* DA_OLD_QUIRKS */
};

static	disk_strategy_t	dastrategy;
static	dumper_t	dadump;
static	periph_init_t	dainit;
static	void		daasync(void *callback_arg, u_int32_t code,
				struct cam_path *path, void *arg);
static	int		dacmdsizesysctl(SYSCTL_HANDLER_ARGS);
static	periph_ctor_t	daregister;
static	periph_dtor_t	dacleanup;
static	periph_start_t	dastart;
static	periph_oninv_t	daoninvalidate;
static	void		dadone(struct cam_periph *periph,
			       union ccb *done_ccb);
static  int		daerror(union ccb *ccb, u_int32_t cam_flags,
				u_int32_t sense_flags);
static void		daprevent(struct cam_periph *periph, int action);
static int		dagetcapacity(struct cam_periph *periph);
static void		dasetgeom(struct cam_periph *periph, uint32_t block_len,
				  uint64_t maxsector);
static timeout_t	dasendorderedtag;
static void		dashutdown(void *arg, int howto);

#ifndef DA_DEFAULT_TIMEOUT
#define DA_DEFAULT_TIMEOUT 60	/* Timeout in seconds */
#endif

#ifndef	DA_DEFAULT_RETRY
#define	DA_DEFAULT_RETRY	4
#endif

static int da_retry_count = DA_DEFAULT_RETRY;
static int da_default_timeout = DA_DEFAULT_TIMEOUT;

SYSCTL_NODE(_kern_cam, OID_AUTO, da, CTLFLAG_RD, 0,
            "CAM Direct Access Disk driver");
SYSCTL_INT(_kern_cam_da, OID_AUTO, retry_count, CTLFLAG_RW,
           &da_retry_count, 0, "Normal I/O retry count");
TUNABLE_INT("kern.cam.da.retry_count", &da_retry_count);
SYSCTL_INT(_kern_cam_da, OID_AUTO, default_timeout, CTLFLAG_RW,
           &da_default_timeout, 0, "Normal I/O timeout (in seconds)");
TUNABLE_INT("kern.cam.da.default_timeout", &da_default_timeout);

/*
 * DA_ORDEREDTAG_INTERVAL determines how often, relative
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
#ifndef DA_ORDEREDTAG_INTERVAL
#define DA_ORDEREDTAG_INTERVAL 4
#endif

static struct periph_driver dadriver =
{
	dainit, "da",
	TAILQ_HEAD_INITIALIZER(dadriver.units), /* generation */ 0
};

PERIPHDRIVER_DECLARE(da, dadriver);

static SLIST_HEAD(,da_softc) softc_list;

static int
daopen(struct disk *dp)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	int unit;
	int error;
	int s;

	s = splsoftcam();
	periph = (struct cam_periph *)dp->d_drv1;
	unit = periph->unit_number;
	if (periph == NULL) {
		splx(s);
		return (ENXIO);	
	}

	softc = (struct da_softc *)periph->softc;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE,
	    ("daopen: disk=%s%d (unit %d)\n", dp->d_name, dp->d_unit,
	     unit));

	if ((error = cam_periph_lock(periph, PRIBIO|PCATCH)) != 0)
		return (error); /* error code from tsleep */

	if (cam_periph_acquire(periph) != CAM_REQ_CMP)
		return(ENXIO);
	softc->flags |= DA_FLAG_OPEN;

	if ((softc->flags & DA_FLAG_PACK_INVALID) != 0) {
		/* Invalidate our pack information. */
		softc->flags &= ~DA_FLAG_PACK_INVALID;
	}
	splx(s);

	error = dagetcapacity(periph);

	if (error == 0) {

		softc->disk.d_sectorsize = softc->params.secsize;
		softc->disk.d_mediasize = softc->params.secsize * (off_t)softc->params.sectors;
		/* XXX: these are not actually "firmware" values, so they may be wrong */
		softc->disk.d_fwsectors = softc->params.secs_per_track;
		softc->disk.d_fwheads = softc->params.heads;
		softc->disk.d_devstat->block_size = softc->params.secsize;
		softc->disk.d_devstat->flags &= ~DEVSTAT_BS_UNAVAILABLE;
	}
	
	if (error == 0) {
		if ((softc->flags & DA_FLAG_PACK_REMOVABLE) != 0)
			daprevent(periph, PR_PREVENT);
	} else {
		softc->flags &= ~DA_FLAG_OPEN;
		cam_periph_release(periph);
	}
	cam_periph_unlock(periph);
	return (error);
}

static int
daclose(struct disk *dp)
{
	struct	cam_periph *periph;
	struct	da_softc *softc;
	int	error;

	periph = (struct cam_periph *)dp->d_drv1;
	if (periph == NULL)
		return (ENXIO);	

	softc = (struct da_softc *)periph->softc;

	if ((error = cam_periph_lock(periph, PRIBIO)) != 0) {
		return (error); /* error code from tsleep */
	}

	if ((softc->quirks & DA_Q_NO_SYNC_CACHE) == 0) {
		union	ccb *ccb;

		ccb = cam_periph_getccb(periph, /*priority*/1);

		scsi_synchronize_cache(&ccb->csio,
				       /*retries*/1,
				       /*cbfcnp*/dadone,
				       MSG_SIMPLE_Q_TAG,
				       /*begin_lba*/0,/* Cover the whole disk */
				       /*lb_count*/0,
				       SSD_FULL_SIZE,
				       5 * 60 * 1000);

		cam_periph_runccb(ccb, /*error_routine*/NULL, /*cam_flags*/0,
				  /*sense_flags*/SF_RETRY_UA,
				  softc->disk.d_devstat);

		if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			     CAM_SCSI_STATUS_ERROR) {
				int asc, ascq;
				int sense_key, error_code;

				scsi_extract_sense(&ccb->csio.sense_data,
						   &error_code,
						   &sense_key, 
						   &asc, &ascq);
				if (sense_key != SSD_KEY_ILLEGAL_REQUEST)
					scsi_sense_print(&ccb->csio);
			} else {
				xpt_print_path(periph->path);
				printf("Synchronize cache failed, status "
				       "== 0x%x, scsi status == 0x%x\n",
				       ccb->csio.ccb_h.status,
				       ccb->csio.scsi_status);
			}
		}

		if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(ccb->ccb_h.path,
					 /*relsim_flags*/0,
					 /*reduction*/0,
					 /*timeout*/0,
					 /*getcount_only*/0);

		xpt_release_ccb(ccb);

	}

	if ((softc->flags & DA_FLAG_PACK_REMOVABLE) != 0) {
		daprevent(periph, PR_ALLOW);
		/*
		 * If we've got removeable media, mark the blocksize as
		 * unavailable, since it could change when new media is
		 * inserted.
		 */
		softc->disk.d_devstat->flags |= DEVSTAT_BS_UNAVAILABLE;
	}

	softc->flags &= ~DA_FLAG_OPEN;
	cam_periph_unlock(periph);
	cam_periph_release(periph);
	return (0);	
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
static void
dastrategy(struct bio *bp)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	int    s;
	
	periph = (struct cam_periph *)bp->bio_disk->d_drv1;
	if (periph == NULL) {
		biofinish(bp, NULL, ENXIO);
		return;
	}
	softc = (struct da_softc *)periph->softc;
#if 0
	/*
	 * check it's not too big a transfer for our adapter
	 */
	scsi_minphys(bp,&sd_switch);
#endif

	/*
	 * Mask interrupts so that the pack cannot be invalidated until
	 * after we are in the queue.  Otherwise, we might not properly
	 * clean up one of the buffers.
	 */
	s = splbio();
	
	/*
	 * If the device has been made invalid, error out
	 */
	if ((softc->flags & DA_FLAG_PACK_INVALID)) {
		splx(s);
		biofinish(bp, NULL, ENXIO);
		return;
	}
	
	/*
	 * Place it in the queue of disk activities for this disk
	 */
	bioq_disksort(&softc->bio_queue, bp);

	splx(s);
	
	/*
	 * Schedule ourselves for performing the work.
	 */
	xpt_schedule(periph, /* XXX priority */1);

	return;
}

static int
dadump(void *arg, void *virtual, vm_offset_t physical, off_t offset, size_t length)
{
	struct	    cam_periph *periph;
	struct	    da_softc *softc;
	u_int	    secsize;
	struct	    ccb_scsiio csio;
	struct	    disk *dp;

	dp = arg;
	periph = dp->d_drv1;
	if (periph == NULL)
		return (ENXIO);
	softc = (struct da_softc *)periph->softc;
	secsize = softc->params.secsize;
	
	if ((softc->flags & DA_FLAG_PACK_INVALID) != 0)
		return (ENXIO);

	if (length > 0) {
		xpt_setup_ccb(&csio.ccb_h, periph->path, /*priority*/1);
		csio.ccb_h.ccb_state = DA_CCB_DUMP;
		scsi_read_write(&csio,
				/*retries*/1,
				dadone,
				MSG_ORDERED_Q_TAG,
				/*read*/FALSE,
				/*byte2*/0,
				/*minimum_cmd_size*/ softc->minimum_cmd_size,
				offset / secsize,
				length / secsize,
				/*data_ptr*/(u_int8_t *) virtual,
				/*dxfer_len*/length,
				/*sense_len*/SSD_FULL_SIZE,
				DA_DEFAULT_TIMEOUT * 1000);		
		xpt_polled_action((union ccb *)&csio);

		if ((csio.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			printf("Aborting dump due to I/O error.\n");
			if ((csio.ccb_h.status & CAM_STATUS_MASK) ==
			     CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(&csio);
			else
				printf("status == 0x%x, scsi status == 0x%x\n",
				       csio.ccb_h.status, csio.scsi_status);
			return(EIO);
		}
		return(0);
	} 
		
	/*
	 * Sync the disk cache contents to the physical media.
	 */
	if ((softc->quirks & DA_Q_NO_SYNC_CACHE) == 0) {

		xpt_setup_ccb(&csio.ccb_h, periph->path, /*priority*/1);
		csio.ccb_h.ccb_state = DA_CCB_DUMP;
		scsi_synchronize_cache(&csio,
				       /*retries*/1,
				       /*cbfcnp*/dadone,
				       MSG_SIMPLE_Q_TAG,
				       /*begin_lba*/0,/* Cover the whole disk */
				       /*lb_count*/0,
				       SSD_FULL_SIZE,
				       5 * 60 * 1000);
		xpt_polled_action((union ccb *)&csio);

		if ((csio.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			if ((csio.ccb_h.status & CAM_STATUS_MASK) ==
			     CAM_SCSI_STATUS_ERROR) {
				int asc, ascq;
				int sense_key, error_code;

				scsi_extract_sense(&csio.sense_data,
						   &error_code,
						   &sense_key, 
						   &asc, &ascq);
				if (sense_key != SSD_KEY_ILLEGAL_REQUEST)
					scsi_sense_print(&csio);
			} else {
				xpt_print_path(periph->path);
				printf("Synchronize cache failed, status "
				       "== 0x%x, scsi status == 0x%x\n",
				       csio.ccb_h.status, csio.scsi_status);
			}
		}
	}
	return (0);
}

static void
dainit(void)
{
	cam_status status;
	struct cam_path *path;

	SLIST_INIT(&softc_list);
	
	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_create_path(&path, /*periph*/NULL, CAM_XPT_PATH_ID,
				 CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);

	if (status == CAM_REQ_CMP) {
		struct ccb_setasync csa;

                xpt_setup_ccb(&csa.ccb_h, path, /*priority*/5);
                csa.ccb_h.func_code = XPT_SASYNC_CB;
                csa.event_enable = AC_FOUND_DEVICE;
                csa.callback = daasync;
                csa.callback_arg = NULL;
                xpt_action((union ccb *)&csa);
		status = csa.ccb_h.status;
                xpt_free_path(path);
        }

	if (status != CAM_REQ_CMP) {
		printf("da: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	} else {

		/*
		 * Schedule a periodic event to occasionally send an
		 * ordered tag to a device.
		 */
		timeout(dasendorderedtag, NULL,
			(DA_DEFAULT_TIMEOUT * hz) / DA_ORDEREDTAG_INTERVAL);

		/* Register our shutdown event handler */
		if ((EVENTHANDLER_REGISTER(shutdown_post_sync, dashutdown, 
					   NULL, SHUTDOWN_PRI_DEFAULT)) == NULL)
		    printf("dainit: shutdown event registration failed!\n");
	}
}

static void
daoninvalidate(struct cam_periph *periph)
{
	int s;
	struct da_softc *softc;
	struct ccb_setasync csa;

	softc = (struct da_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_setup_ccb(&csa.ccb_h, periph->path,
		      /* priority */ 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = 0;
	csa.callback = daasync;
	csa.callback_arg = periph;
	xpt_action((union ccb *)&csa);

	softc->flags |= DA_FLAG_PACK_INVALID;

	/*
	 * Although the oninvalidate() routines are always called at
	 * splsoftcam, we need to be at splbio() here to keep the buffer
	 * queue from being modified while we traverse it.
	 */
	s = splbio();

	/*
	 * Return all queued I/O with ENXIO.
	 * XXX Handle any transactions queued to the card
	 *     with XPT_ABORT_CCB.
	 */
	bioq_flush(&softc->bio_queue, NULL, ENXIO);
	splx(s);

	SLIST_REMOVE(&softc_list, softc, da_softc, links);

	xpt_print_path(periph->path);
	printf("lost device\n");
}

static void
dacleanup(struct cam_periph *periph)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	xpt_print_path(periph->path);
	printf("removing device entry\n");
	/*
	 * If we can't free the sysctl tree, oh well...
	 */
	if (sysctl_ctx_free(&softc->sysctl_ctx) != 0) {
		xpt_print_path(periph->path);
		printf("can't remove sysctl context\n");
	}
	disk_destroy(&softc->disk);
	free(softc, M_DEVBUF);
}

static void
daasync(void *callback_arg, u_int32_t code,
	struct cam_path *path, void *arg)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)callback_arg;
	switch (code) {
	case AC_FOUND_DEVICE:
	{
		struct ccb_getdev *cgd;
		cam_status status;
 
		cgd = (struct ccb_getdev *)arg;
		if (cgd == NULL)
			break;

		if (SID_TYPE(&cgd->inq_data) != T_DIRECT
		    && SID_TYPE(&cgd->inq_data) != T_RBC
		    && SID_TYPE(&cgd->inq_data) != T_OPTICAL)
			break;

		/*
		 * Allocate a peripheral instance for
		 * this device and start the probe
		 * process.
		 */
		status = cam_periph_alloc(daregister, daoninvalidate,
					  dacleanup, dastart,
					  "da", CAM_PERIPH_BIO,
					  cgd->ccb_h.path, daasync,
					  AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG)
			printf("daasync: Unable to attach to new device "
				"due to status 0x%x\n", status);
		break;
	}
	case AC_SENT_BDR:
	case AC_BUS_RESET:
	{
		struct da_softc *softc;
		struct ccb_hdr *ccbh;
		int s;

		softc = (struct da_softc *)periph->softc;
		s = splsoftcam();
		/*
		 * Don't fail on the expected unit attention
		 * that will occur.
		 */
		softc->flags |= DA_FLAG_RETRY_UA;
		LIST_FOREACH(ccbh, &softc->pending_ccbs, periph_links.le)
			ccbh->ccb_state |= DA_CCB_RETRY_UA;
		splx(s);
		/* FALLTHROUGH*/
	}
	default:
		cam_periph_async(periph, code, path, arg);
		break;
	}
}

static int
dacmdsizesysctl(SYSCTL_HANDLER_ARGS)
{
	int error, value;

	value = *(int *)arg1;

	error = sysctl_handle_int(oidp, &value, 0, req);

	if ((error != 0)
	 || (req->newptr == NULL))
		return (error);

	/*
	 * Acceptable values here are 6, 10, 12 or 16.
	 */
	if (value < 6)
		value = 6;
	else if ((value > 6)
	      && (value <= 10))
		value = 10;
	else if ((value > 10)
	      && (value <= 12))
		value = 12;
	else if (value > 12)
		value = 16;

	*(int *)arg1 = value;

	return (0);
}

static cam_status
daregister(struct cam_periph *periph, void *arg)
{
	int s;
	struct da_softc *softc;
	struct ccb_setasync csa;
	struct ccb_pathinq cpi;
	struct ccb_getdev *cgd;
	char tmpstr[80], tmpstr2[80];
	caddr_t match;

	cgd = (struct ccb_getdev *)arg;
	if (periph == NULL) {
		printf("daregister: periph was NULL!!\n");
		return(CAM_REQ_CMP_ERR);
	}

	if (cgd == NULL) {
		printf("daregister: no getdev CCB, can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = (struct da_softc *)malloc(sizeof(*softc), M_DEVBUF,
	    M_NOWAIT|M_ZERO);

	if (softc == NULL) {
		printf("daregister: Unable to probe new device. "
		       "Unable to allocate softc\n");				
		return(CAM_REQ_CMP_ERR);
	}

	LIST_INIT(&softc->pending_ccbs);
	softc->state = DA_STATE_PROBE;
	bioq_init(&softc->bio_queue);
	if (SID_IS_REMOVABLE(&cgd->inq_data))
		softc->flags |= DA_FLAG_PACK_REMOVABLE;
	if ((cgd->inq_data.flags & SID_CmdQue) != 0)
		softc->flags |= DA_FLAG_TAGGED_QUEUING;

	periph->softc = softc;

	/*
	 * See if this device has any quirks.
	 */
	match = cam_quirkmatch((caddr_t)&cgd->inq_data,
			       (caddr_t)da_quirk_table,
			       sizeof(da_quirk_table)/sizeof(*da_quirk_table),
			       sizeof(*da_quirk_table), scsi_inquiry_match);

	if (match != NULL)
		softc->quirks = ((struct da_quirk_entry *)match)->quirks;
	else
		softc->quirks = DA_Q_NONE;

	/* Check if the SIM does not want 6 byte commands */
	xpt_setup_ccb(&cpi.ccb_h, periph->path, /*priority*/1);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);
	if (cpi.ccb_h.status == CAM_REQ_CMP && (cpi.hba_misc & PIM_NO_6_BYTE))
		softc->quirks |= DA_Q_NO_6_BYTE;

	snprintf(tmpstr, sizeof(tmpstr), "CAM DA unit %d", periph->unit_number);
	snprintf(tmpstr2, sizeof(tmpstr2), "%d", periph->unit_number);
	sysctl_ctx_init(&softc->sysctl_ctx);
	softc->sysctl_tree = SYSCTL_ADD_NODE(&softc->sysctl_ctx,
		SYSCTL_STATIC_CHILDREN(_kern_cam_da), OID_AUTO, tmpstr2,
		CTLFLAG_RD, 0, tmpstr);
	if (softc->sysctl_tree == NULL) {
		printf("daregister: unable to allocate sysctl tree\n");
		free(softc, M_DEVBUF);
		return (CAM_REQ_CMP_ERR);
	}

	/*
	 * RBC devices don't have to support READ(6), only READ(10).
	 */
	if (softc->quirks & DA_Q_NO_6_BYTE || SID_TYPE(&cgd->inq_data) == T_RBC)
		softc->minimum_cmd_size = 10;
	else
		softc->minimum_cmd_size = 6;

	/*
	 * Load the user's default, if any.
	 */
	snprintf(tmpstr, sizeof(tmpstr), "kern.cam.da.%d.minimum_cmd_size",
		 periph->unit_number);
	TUNABLE_INT_FETCH(tmpstr, &softc->minimum_cmd_size);

	/*
	 * 6, 10, 12 and 16 are the currently permissible values.
	 */
	if (softc->minimum_cmd_size < 6)
		softc->minimum_cmd_size = 6;
	else if ((softc->minimum_cmd_size > 6)
	      && (softc->minimum_cmd_size <= 10))
		softc->minimum_cmd_size = 10;
	else if ((softc->minimum_cmd_size > 10)
	      && (softc->minimum_cmd_size <= 12))
		softc->minimum_cmd_size = 12;
	else if (softc->minimum_cmd_size > 12)
		softc->minimum_cmd_size = 16;

	/*
	 * Now register the sysctl handler, so the user can the value on
	 * the fly.
	 */
	SYSCTL_ADD_PROC(&softc->sysctl_ctx,SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "minimum_cmd_size", CTLTYPE_INT | CTLFLAG_RW,
		&softc->minimum_cmd_size, 0, dacmdsizesysctl, "I",
		"Minimum CDB size");

	/*
	 * Block our timeout handler while we
	 * add this softc to the dev list.
	 */
	s = splsoftclock();
	SLIST_INSERT_HEAD(&softc_list, softc, links);
	splx(s);

	/*
	 * Register this media as a disk
	 */

	softc->disk.d_open = daopen;
	softc->disk.d_close = daclose;
	softc->disk.d_strategy = dastrategy;
	softc->disk.d_dump = dadump;
	softc->disk.d_name = "da";
	softc->disk.d_drv1 = periph;
	softc->disk.d_maxsize = DFLTPHYS; /* XXX: probably not arbitrary */
	disk_create(periph->unit_number, &softc->disk, 0, NULL, NULL);

	/*
	 * Add async callbacks for bus reset and
	 * bus device reset calls.  I don't bother
	 * checking if this fails as, in most cases,
	 * the system will function just fine without
	 * them and the only alternative would be to
	 * not attach the device on failure.
	 */
	xpt_setup_ccb(&csa.ccb_h, periph->path, /*priority*/5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_SENT_BDR | AC_BUS_RESET | AC_LOST_DEVICE;
	csa.callback = daasync;
	csa.callback_arg = periph;
	xpt_action((union ccb *)&csa);
	/*
	 * Lock this peripheral until we are setup.
	 * This first call can't block
	 */
	(void)cam_periph_lock(periph, PRIBIO);
	xpt_schedule(periph, /*priority*/5);

	return(CAM_REQ_CMP);
}

static void
dastart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	
	switch (softc->state) {
	case DA_STATE_NORMAL:
	{
		/* Pull a buffer from the queue and get going on it */		
		struct bio *bp;
		int s;

		/*
		 * See if there is a buf with work for us to do..
		 */
		s = splbio();
		bp = bioq_first(&softc->bio_queue);
		if (periph->immediate_priority <= periph->pinfo.priority) {
			CAM_DEBUG_PRINT(CAM_DEBUG_SUBTRACE,
					("queuing for immediate ccb\n"));
			start_ccb->ccb_h.ccb_state = DA_CCB_WAITING;
			SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
					  periph_links.sle);
			periph->immediate_priority = CAM_PRIORITY_NONE;
			splx(s);
			wakeup(&periph->ccb_list);
		} else if (bp == NULL) {
			splx(s);
			xpt_release_ccb(start_ccb);
		} else {
			int oldspl;
			u_int8_t tag_code;

			bioq_remove(&softc->bio_queue, bp);

			if ((softc->flags & DA_FLAG_NEED_OTAG) != 0) {
				softc->flags &= ~DA_FLAG_NEED_OTAG;
				softc->ordered_tag_count++;
				tag_code = MSG_ORDERED_Q_TAG;
			} else {
				tag_code = MSG_SIMPLE_Q_TAG;
			}
			scsi_read_write(&start_ccb->csio,
					/*retries*/da_retry_count,
					/*cbfcnp*/dadone,
					/*tag_action*/tag_code,
					/*read_op*/bp->bio_cmd == BIO_READ,
					/*byte2*/0,
					softc->minimum_cmd_size,
					/*lba*/bp->bio_pblkno,
					/*block_count*/bp->bio_bcount /
					softc->params.secsize,
					/*data_ptr*/ bp->bio_data,
					/*dxfer_len*/ bp->bio_bcount,
					/*sense_len*/SSD_FULL_SIZE,
					/*timeout*/da_default_timeout*1000);
			start_ccb->ccb_h.ccb_state = DA_CCB_BUFFER_IO;

			/*
			 * Block out any asyncronous callbacks
			 * while we touch the pending ccb list.
			 */
			oldspl = splcam();
			LIST_INSERT_HEAD(&softc->pending_ccbs,
					 &start_ccb->ccb_h, periph_links.le);
			softc->outstanding_cmds++;
			splx(oldspl);

			/* We expect a unit attention from this device */
			if ((softc->flags & DA_FLAG_RETRY_UA) != 0) {
				start_ccb->ccb_h.ccb_state |= DA_CCB_RETRY_UA;
				softc->flags &= ~DA_FLAG_RETRY_UA;
			}

			start_ccb->ccb_h.ccb_bp = bp;
			bp = bioq_first(&softc->bio_queue);
			splx(s);

			xpt_action(start_ccb);
		}
		
		if (bp != NULL) {
			/* Have more work to do, so ensure we stay scheduled */
			xpt_schedule(periph, /* XXX priority */1);
		}
		break;
	}
	case DA_STATE_PROBE:
	{
		struct ccb_scsiio *csio;
		struct scsi_read_capacity_data *rcap;

		rcap = (struct scsi_read_capacity_data *)malloc(sizeof(*rcap),
								M_TEMP,
								M_NOWAIT);
		if (rcap == NULL) {
			printf("dastart: Couldn't malloc read_capacity data\n");
			/* da_free_periph??? */
			break;
		}
		csio = &start_ccb->csio;
		scsi_read_capacity(csio,
				   /*retries*/4,
				   dadone,
				   MSG_SIMPLE_Q_TAG,
				   rcap,
				   SSD_FULL_SIZE,
				   /*timeout*/5000);
		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = DA_CCB_PROBE;
		xpt_action(start_ccb);
		break;
	}
	case DA_STATE_PROBE2:
	{
		struct ccb_scsiio *csio;
		struct scsi_read_capacity_data_long *rcaplong;

		rcaplong = (struct scsi_read_capacity_data_long *)
			malloc(sizeof(*rcaplong), M_TEMP, M_NOWAIT);
		if (rcaplong == NULL) {
			printf("dastart: Couldn't malloc read_capacity data\n");
			/* da_free_periph??? */
			break;
		}
		csio = &start_ccb->csio;
		scsi_read_capacity_16(csio,
				      /*retries*/ 4,
				      /*cbfcnp*/ dadone,
				      /*tag_action*/ MSG_SIMPLE_Q_TAG,
				      /*lba*/ 0,
				      /*reladr*/ 0,
				      /*pmi*/ 0,
				      rcaplong,
				      /*sense_len*/ SSD_FULL_SIZE,
				      /*timeout*/ 60000);
		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = DA_CCB_PROBE2;
		xpt_action(start_ccb);	
		break;
	}
	}
}

static int
cmd6workaround(union ccb *ccb)
{
	struct scsi_rw_6 cmd6;
	struct scsi_rw_10 *cmd10;
	struct da_softc *softc;
	u_int8_t *cdb;
	int frozen;

	cdb = ccb->csio.cdb_io.cdb_bytes;

	/* Translation only possible if CDB is an array and cmd is R/W6 */
	if ((ccb->ccb_h.flags & CAM_CDB_POINTER) != 0 ||
	    (*cdb != READ_6 && *cdb != WRITE_6))
		return 0;

	xpt_print_path(ccb->ccb_h.path);
 	printf("READ(6)/WRITE(6) not supported, "
	       "increasing minimum_cmd_size to 10.\n");
 	softc = (struct da_softc *)xpt_path_periph(ccb->ccb_h.path)->softc;
	softc->minimum_cmd_size = 10;

	bcopy(cdb, &cmd6, sizeof(struct scsi_rw_6));
	cmd10 = (struct scsi_rw_10 *)cdb;
	cmd10->opcode = (cmd6.opcode == READ_6) ? READ_10 : WRITE_10;
	cmd10->byte2 = 0;
	scsi_ulto4b(scsi_3btoul(cmd6.addr), cmd10->addr);
	cmd10->reserved = 0;
	scsi_ulto2b(cmd6.length, cmd10->length);
	cmd10->control = cmd6.control;
	ccb->csio.cdb_len = sizeof(*cmd10);

	/* Requeue request, unfreezing queue if necessary */
	frozen = (ccb->ccb_h.status & CAM_DEV_QFRZN) != 0;
 	ccb->ccb_h.status = CAM_REQUEUE_REQ;
	xpt_action(ccb);
	if (frozen) {
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0,
				 /*timeout*/0,
				 /*getcount_only*/0);
	}
	return (ERESTART);
}

static void
dadone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct da_softc *softc;
	struct ccb_scsiio *csio;

	softc = (struct da_softc *)periph->softc;
	csio = &done_ccb->csio;
	switch (csio->ccb_h.ccb_state & DA_CCB_TYPE_MASK) {
	case DA_CCB_BUFFER_IO:
	{
		struct bio *bp;
		int    oldspl;

		bp = (struct bio *)done_ccb->ccb_h.ccb_bp;
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			int error;
			int s;
			int sf;
			
			if ((csio->ccb_h.ccb_state & DA_CCB_RETRY_UA) != 0)
				sf = SF_RETRY_UA;
			else
				sf = 0;

			error = daerror(done_ccb, CAM_RETRY_SELTO, sf);
			if (error == ERESTART) {
				/*
				 * A retry was scheuled, so
				 * just return.
				 */
				return;
			}
			if (error != 0) {

				s = splbio();

				if (error == ENXIO) {
					/*
					 * Catastrophic error.  Mark our pack as
					 * invalid.
					 */
					/* XXX See if this is really a media
					 *     change first.
					 */
					xpt_print_path(periph->path);
					printf("Invalidating pack\n");
					softc->flags |= DA_FLAG_PACK_INVALID;
				}

				/*
				 * return all queued I/O with EIO, so that
				 * the client can retry these I/Os in the
				 * proper order should it attempt to recover.
				 */
				bioq_flush(&softc->bio_queue, NULL, EIO);
				splx(s);
				bp->bio_error = error;
				bp->bio_resid = bp->bio_bcount;
				bp->bio_flags |= BIO_ERROR;
			} else {
				bp->bio_resid = csio->resid;
				bp->bio_error = 0;
				if (bp->bio_resid != 0)
					bp->bio_flags |= BIO_ERROR;
			}
			if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
				cam_release_devq(done_ccb->ccb_h.path,
						 /*relsim_flags*/0,
						 /*reduction*/0,
						 /*timeout*/0,
						 /*getcount_only*/0);
		} else {
			if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
				panic("REQ_CMP with QFRZN");
			bp->bio_resid = csio->resid;
			if (csio->resid > 0)
				bp->bio_flags |= BIO_ERROR;
		}

		/*
		 * Block out any asyncronous callbacks
		 * while we touch the pending ccb list.
		 */
		oldspl = splcam();
		LIST_REMOVE(&done_ccb->ccb_h, periph_links.le);
		softc->outstanding_cmds--;
		if (softc->outstanding_cmds == 0)
			softc->flags |= DA_FLAG_WENT_IDLE;
		splx(oldspl);

		biodone(bp);
		break;
	}
	case DA_CCB_PROBE:
	case DA_CCB_PROBE2:
	{
		struct	   scsi_read_capacity_data *rdcap;
		struct     scsi_read_capacity_data_long *rcaplong;
		char	   announce_buf[80];

		rdcap = NULL;
		rcaplong = NULL;
		if (softc->state == DA_STATE_PROBE)
			rdcap =(struct scsi_read_capacity_data *)csio->data_ptr;
		else
			rcaplong = (struct scsi_read_capacity_data_long *)
				csio->data_ptr;

		if ((csio->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			struct disk_params *dp;
			uint32_t block_size;
			uint64_t maxsector;

			if (softc->state == DA_STATE_PROBE) {
				block_size = scsi_4btoul(rdcap->length);
				maxsector = scsi_4btoul(rdcap->addr);

				/*
				 * According to SBC-2, if the standard 10
				 * byte READ CAPACITY command returns 2^32,
				 * we should issue the 16 byte version of
				 * the command, since the device in question
				 * has more sectors than can be represented
				 * with the short version of the command.
				 */
				if (maxsector == 0xffffffff) {
					softc->state = DA_STATE_PROBE2;
					free(rdcap, M_TEMP);
					xpt_release_ccb(done_ccb);
					xpt_schedule(periph, /*priority*/5);
					return;
				}
			} else {
				block_size = scsi_4btoul(rcaplong->length);
				maxsector = scsi_8btou64(rcaplong->addr);
			}
			dasetgeom(periph, block_size, maxsector);
			dp = &softc->params;
			snprintf(announce_buf, sizeof(announce_buf),
			        "%juMB (%ju %u byte sectors: %dH %dS/T %dC)",
				(uintmax_t) (((uintmax_t)dp->secsize *
				dp->sectors) / (1024*1024)),
			        (uintmax_t)dp->sectors,
				dp->secsize, dp->heads, dp->secs_per_track,
				dp->cylinders);
		} else {
			int	error;

			announce_buf[0] = '\0';

			/*
			 * Retry any UNIT ATTENTION type errors.  They
			 * are expected at boot.
			 */
			error = daerror(done_ccb, CAM_RETRY_SELTO,
					SF_RETRY_UA|SF_NO_PRINT);
			if (error == ERESTART) {
				/*
				 * A retry was scheuled, so
				 * just return.
				 */
				return;
			} else if (error != 0) {
				struct scsi_sense_data *sense;
				int asc, ascq;
				int sense_key, error_code;
				int have_sense;
				cam_status status;
				struct ccb_getdev cgd;

				/* Don't wedge this device's queue */
				status = done_ccb->ccb_h.status;
				if ((status & CAM_DEV_QFRZN) != 0)
					cam_release_devq(done_ccb->ccb_h.path,
							 /*relsim_flags*/0,
							 /*reduction*/0,
							 /*timeout*/0,
							 /*getcount_only*/0);


				xpt_setup_ccb(&cgd.ccb_h, 
					      done_ccb->ccb_h.path,
					      /* priority */ 1);
				cgd.ccb_h.func_code = XPT_GDEV_TYPE;
				xpt_action((union ccb *)&cgd);

				if (((csio->ccb_h.flags & CAM_SENSE_PHYS) != 0)
				 || ((csio->ccb_h.flags & CAM_SENSE_PTR) != 0)
				 || ((status & CAM_AUTOSNS_VALID) == 0))
					have_sense = FALSE;
				else
					have_sense = TRUE;

				if (have_sense) {
					sense = &csio->sense_data;
					scsi_extract_sense(sense, &error_code,
							   &sense_key, 
							   &asc, &ascq);
				}
				/*
				 * Attach to anything that claims to be a
				 * direct access or optical disk device,
				 * as long as it doesn't return a "Logical
				 * unit not supported" (0x25) error.
				 */
				if ((have_sense) && (asc != 0x25)
				 && (error_code == SSD_CURRENT_ERROR)) {
					const char *sense_key_desc;
					const char *asc_desc;

					scsi_sense_desc(sense_key, asc, ascq,
							&cgd.inq_data,
							&sense_key_desc,
							&asc_desc);
					snprintf(announce_buf,
					    sizeof(announce_buf),
						"Attempt to query device "
						"size failed: %s, %s",
						sense_key_desc,
						asc_desc);
				} else { 
					if (have_sense)
						scsi_sense_print(
							&done_ccb->csio);
					else {
						xpt_print_path(periph->path);
						printf("got CAM status %#x\n",
						       done_ccb->ccb_h.status);
					}

					xpt_print_path(periph->path);
					printf("fatal error, failed" 
					       " to attach to device\n");

					/*
					 * Free up resources.
					 */
					cam_periph_invalidate(periph);
				} 
			}
		}
		free(csio->data_ptr, M_TEMP);
		if (announce_buf[0] != '\0')
			xpt_announce_periph(periph, announce_buf);
		softc->state = DA_STATE_NORMAL;	
		/*
		 * Since our peripheral may be invalidated by an error
		 * above or an external event, we must release our CCB
		 * before releasing the probe lock on the peripheral.
		 * The peripheral will only go away once the last lock
		 * is removed, and we need it around for the CCB release
		 * operation.
		 */
		xpt_release_ccb(done_ccb);
		cam_periph_unlock(periph);
		return;
	}
	case DA_CCB_WAITING:
	{
		/* Caller will release the CCB */
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}
	case DA_CCB_DUMP:
		/* No-op.  We're polling */
		return;
	default:
		break;
	}
	xpt_release_ccb(done_ccb);
}

static int
daerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	struct da_softc	  *softc;
	struct cam_periph *periph;
	int error;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct da_softc *)periph->softc;

 	/*
	 * Automatically detect devices that do not support
 	 * READ(6)/WRITE(6) and upgrade to using 10 byte cdbs.
 	 */
	error = 0;
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INVALID) {
		error = cmd6workaround(ccb);
	} else if (((ccb->ccb_h.status & CAM_STATUS_MASK) ==
		   CAM_SCSI_STATUS_ERROR)
	 && (ccb->ccb_h.status & CAM_AUTOSNS_VALID)
	 && (ccb->csio.scsi_status == SCSI_STATUS_CHECK_COND)
	 && ((ccb->ccb_h.flags & CAM_SENSE_PHYS) == 0)
	 && ((ccb->ccb_h.flags & CAM_SENSE_PTR) == 0)) {
		int sense_key, error_code, asc, ascq;

 		scsi_extract_sense(&ccb->csio.sense_data,
				   &error_code, &sense_key, &asc, &ascq);
		if (sense_key == SSD_KEY_ILLEGAL_REQUEST)
 			error = cmd6workaround(ccb);
	}
	if (error == ERESTART)
		return (ERESTART);

	/*
	 * XXX
	 * Until we have a better way of doing pack validation,
	 * don't treat UAs as errors.
	 */
	sense_flags |= SF_RETRY_UA;
	return(cam_periph_error(ccb, cam_flags, sense_flags,
				&softc->saved_ccb));
}

static void
daprevent(struct cam_periph *periph, int action)
{
	struct	da_softc *softc;
	union	ccb *ccb;		
	int	error;
		
	softc = (struct da_softc *)periph->softc;

	if (((action == PR_ALLOW)
	  && (softc->flags & DA_FLAG_PACK_LOCKED) == 0)
	 || ((action == PR_PREVENT)
	  && (softc->flags & DA_FLAG_PACK_LOCKED) != 0)) {
		return;
	}

	ccb = cam_periph_getccb(periph, /*priority*/1);

	scsi_prevent(&ccb->csio,
		     /*retries*/1,
		     /*cbcfp*/dadone,
		     MSG_SIMPLE_Q_TAG,
		     action,
		     SSD_FULL_SIZE,
		     5000);

	error = cam_periph_runccb(ccb, /*error_routine*/NULL, CAM_RETRY_SELTO,
				  SF_RETRY_UA, softc->disk.d_devstat);

	if (error == 0) {
		if (action == PR_ALLOW)
			softc->flags &= ~DA_FLAG_PACK_LOCKED;
		else
			softc->flags |= DA_FLAG_PACK_LOCKED;
	}

	xpt_release_ccb(ccb);
}

static int
dagetcapacity(struct cam_periph *periph)
{
	struct da_softc *softc;
	union ccb *ccb;
	struct scsi_read_capacity_data *rcap;
	struct scsi_read_capacity_data_long *rcaplong;
	uint32_t block_len;
	uint64_t maxsector;
	int error;

	softc = (struct da_softc *)periph->softc;
	block_len = 0;
	maxsector = 0;
	error = 0;

	/* Do a read capacity */
	rcap = (struct scsi_read_capacity_data *)malloc(sizeof(*rcaplong),
							M_TEMP,
							M_WAITOK);
		
	ccb = cam_periph_getccb(periph, /*priority*/1);
	scsi_read_capacity(&ccb->csio,
			   /*retries*/4,
			   /*cbfncp*/dadone,
			   MSG_SIMPLE_Q_TAG,
			   rcap,
			   SSD_FULL_SIZE,
			   /*timeout*/60000);
	ccb->ccb_h.ccb_bp = NULL;

	error = cam_periph_runccb(ccb, daerror,
				  /*cam_flags*/CAM_RETRY_SELTO,
				  /*sense_flags*/SF_RETRY_UA,
				  softc->disk.d_devstat);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0,
				 /*timeout*/0,
				 /*getcount_only*/0);

	if (error == 0) {
		block_len = scsi_4btoul(rcap->length);
		maxsector = scsi_4btoul(rcap->addr);

		if (maxsector != 0xffffffff)
			goto done;
	} else
		goto done;

	rcaplong = (struct scsi_read_capacity_data_long *)rcap;

	scsi_read_capacity_16(&ccb->csio,
			      /*retries*/ 4,
			      /*cbfcnp*/ dadone,
			      /*tag_action*/ MSG_SIMPLE_Q_TAG,
			      /*lba*/ 0,
			      /*reladr*/ 0,
			      /*pmi*/ 0,
			      rcaplong,
			      /*sense_len*/ SSD_FULL_SIZE,
			      /*timeout*/ 60000);
	ccb->ccb_h.ccb_bp = NULL;

	error = cam_periph_runccb(ccb, daerror,
				  /*cam_flags*/CAM_RETRY_SELTO,
				  /*sense_flags*/SF_RETRY_UA,
				  softc->disk.d_devstat);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0,
				 /*timeout*/0,
				 /*getcount_only*/0);

	if (error == 0) {
		block_len = scsi_4btoul(rcaplong->length);
		maxsector = scsi_8btou64(rcaplong->addr);
	}

done:

	if (error == 0)
		dasetgeom(periph, block_len, maxsector);

	xpt_release_ccb(ccb);

	free(rcap, M_TEMP);

	return (error);
}

static void
dasetgeom(struct cam_periph *periph, uint32_t block_len, uint64_t maxsector)
{
	struct ccb_calc_geometry ccg;
	struct da_softc *softc;
	struct disk_params *dp;

	softc = (struct da_softc *)periph->softc;

	dp = &softc->params;
	dp->secsize = block_len;
	dp->sectors = maxsector + 1;
	/*
	 * Have the controller provide us with a geometry
	 * for this disk.  The only time the geometry
	 * matters is when we boot and the controller
	 * is the only one knowledgeable enough to come
	 * up with something that will make this a bootable
	 * device.
	 */
	xpt_setup_ccb(&ccg.ccb_h, periph->path, /*priority*/1);
	ccg.ccb_h.func_code = XPT_CALC_GEOMETRY;
	ccg.block_size = dp->secsize;
	ccg.volume_size = dp->sectors;
	ccg.heads = 0;
	ccg.secs_per_track = 0;
	ccg.cylinders = 0;
	xpt_action((union ccb*)&ccg);
	dp->heads = ccg.heads;
	dp->secs_per_track = ccg.secs_per_track;
	dp->cylinders = ccg.cylinders;
}

static void
dasendorderedtag(void *arg)
{
	struct da_softc *softc;
	int s;

	for (softc = SLIST_FIRST(&softc_list);
	     softc != NULL;
	     softc = SLIST_NEXT(softc, links)) {
		s = splsoftcam();
		if ((softc->ordered_tag_count == 0) 
		 && ((softc->flags & DA_FLAG_WENT_IDLE) == 0)) {
			softc->flags |= DA_FLAG_NEED_OTAG;
		}
		if (softc->outstanding_cmds > 0)
			softc->flags &= ~DA_FLAG_WENT_IDLE;

		softc->ordered_tag_count = 0;
		splx(s);
	}
	/* Queue us up again */
	timeout(dasendorderedtag, NULL,
		(da_default_timeout * hz) / DA_ORDEREDTAG_INTERVAL);
}

/*
 * Step through all DA peripheral drivers, and if the device is still open,
 * sync the disk cache to physical media.
 */
static void
dashutdown(void * arg, int howto)
{
	struct cam_periph *periph;
	struct da_softc *softc;

	TAILQ_FOREACH(periph, &dadriver.units, unit_links) {
		union ccb ccb;
		softc = (struct da_softc *)periph->softc;

		/*
		 * We only sync the cache if the drive is still open, and
		 * if the drive is capable of it..
		 */
		if (((softc->flags & DA_FLAG_OPEN) == 0)
		 || (softc->quirks & DA_Q_NO_SYNC_CACHE))
			continue;

		xpt_setup_ccb(&ccb.ccb_h, periph->path, /*priority*/1);

		ccb.ccb_h.ccb_state = DA_CCB_DUMP;
		scsi_synchronize_cache(&ccb.csio,
				       /*retries*/1,
				       /*cbfcnp*/dadone,
				       MSG_SIMPLE_Q_TAG,
				       /*begin_lba*/0, /* whole disk */
				       /*lb_count*/0,
				       SSD_FULL_SIZE,
				       60 * 60 * 1000);

		xpt_polled_action(&ccb);

		if ((ccb.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			if (((ccb.ccb_h.status & CAM_STATUS_MASK) ==
			     CAM_SCSI_STATUS_ERROR)
			 && (ccb.csio.scsi_status == SCSI_STATUS_CHECK_COND)){
				int error_code, sense_key, asc, ascq;

				scsi_extract_sense(&ccb.csio.sense_data,
						   &error_code, &sense_key,
						   &asc, &ascq);

				if (sense_key != SSD_KEY_ILLEGAL_REQUEST)
					scsi_sense_print(&ccb.csio);
			} else {
				xpt_print_path(periph->path);
				printf("Synchronize cache failed, status "
				       "== 0x%x, scsi status == 0x%x\n",
				       ccb.ccb_h.status, ccb.csio.scsi_status);
			}
		}

		if ((ccb.ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(ccb.ccb_h.path,
					 /*relsim_flags*/0,
					 /*reduction*/0,
					 /*timeout*/0,
					 /*getcount_only*/0);

	}
}

#else /* !_KERNEL */

/*
 * XXX This is only left out of the kernel build to silence warnings.  If,
 * for some reason this function is used in the kernel, the ifdefs should
 * be moved so it is included both in the kernel and userland.
 */
void
scsi_format_unit(struct ccb_scsiio *csio, u_int32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 u_int8_t tag_action, u_int8_t byte2, u_int16_t ileave,
		 u_int8_t *data_ptr, u_int32_t dxfer_len, u_int8_t sense_len,
		 u_int32_t timeout)
{
	struct scsi_format_unit *scsi_cmd;

	scsi_cmd = (struct scsi_format_unit *)&csio->cdb_io.cdb_bytes;
	scsi_cmd->opcode = FORMAT_UNIT;
	scsi_cmd->byte2 = byte2;
	scsi_ulto2b(ileave, scsi_cmd->interleave);

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ (dxfer_len > 0) ? CAM_DIR_OUT : CAM_DIR_NONE,
		      tag_action,
		      data_ptr,
		      dxfer_len,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

#endif /* _KERNEL */
