/*-
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
	DA_FLAG_OPEN		= 0x100,
	DA_FLAG_SCTX_INIT	= 0x200
} da_flags;

typedef enum {
	DA_Q_NONE		= 0x00,
	DA_Q_NO_SYNC_CACHE	= 0x01,
	DA_Q_NO_6_BYTE		= 0x02,
	DA_Q_NO_PREVENT		= 0x04
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
	struct	 disk *disk;
	union	 ccb saved_ccb;
	struct task		sysctl_task;
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
	struct callout		sendordered_c;
};

struct da_quirk_entry {
	struct scsi_inquiry_pattern inq_pat;
	da_quirks quirks;
};

static const char quantum[] = "QUANTUM";
static const char microp[] = "MICROP";

static struct da_quirk_entry da_quirk_table[] =
{
	/* SPI, FC devices */
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
		 * Reported by: Blaz Zupan <blaz@gold.amis.net>
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
		 * Doesn't like the synchronize cache command.
		 * Reported by: walter@pelissero.de
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "LPS540S", "*"},
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
		/* See above. */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "VIKING 2*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 * Reported by: walter@pelissero.de
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "CONNER", "CP3500*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * The CISS RAID controllers do not support SYNC_CACHE
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "COMPAQ", "RAID*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	/* USB mass storage devices supported by umass(4) */
	{
		/*
		 * EXATELECOM (Sigmatel) i-Bead 100/105 USB Flash MP3 Player
		 * PR: kern/51675
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "EXATEL", "i-BEAD10*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Power Quotient Int. (PQI) USB flash key
		 * PR: kern/53067
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Generic*", "USB Flash Disk*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
 	{
 		/*
 		 * Creative Nomad MUVO mp3 player (USB)
 		 * PR: kern/53094
 		 */
 		{T_DIRECT, SIP_MEDIA_REMOVABLE, "CREATIVE", "NOMAD_MUVO", "*"},
 		/*quirks*/ DA_Q_NO_SYNC_CACHE|DA_Q_NO_PREVENT
 	},
	{
		/*
		 * Jungsoft NEXDISK USB flash key
		 * PR: kern/54737
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "JUNGSOFT", "NEXDISK*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * FreeDik USB Mini Data Drive
		 * PR: kern/54786
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "FreeDik*", "Mini Data Drive",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Sigmatel USB Flash MP3 Player
		 * PR: kern/57046
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "SigmaTel", "MSCN", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE|DA_Q_NO_PREVENT
	},
	{
		/*
		 * Neuros USB Digital Audio Computer
		 * PR: kern/63645
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "NEUROS", "dig. audio comp.",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * SEAGRAND NP-900 MP3 Player
		 * PR: kern/64563
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "SEAGRAND", "NP-900*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE|DA_Q_NO_PREVENT
	},
	{
		/*
		 * iRiver iFP MP3 player (with UMS Firmware)
		 * PR: kern/54881, i386/63941, kern/66124
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "iRiver", "iFP*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
 	},
	{
		/*
		 * Frontier Labs NEX IA+ Digital Audio Player, rev 1.10/0.01
		 * PR: kern/70158
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "FL" , "Nex*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * ZICPlay USB MP3 Player with FM
		 * PR: kern/75057
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "ACTIONS*" , "USB DISK*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * TEAC USB floppy mechanisms
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "TEAC" , "FD-05*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Kingston DataTraveler II+ USB Pen-Drive.
		 * Reported by: Pawel Jakub Dawidek <pjd@FreeBSD.org>
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Kingston" , "DataTraveler II+",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Motorola E398 Mobile Phone (TransFlash memory card).
		 * Reported by: Wojciech A. Koszek <dunstan@FreeBSD.czest.pl>
		 * PR: usb/89889
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Motorola" , "Motorola Phone",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Qware BeatZkey! Pro
		 * PR: usb/79164
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "GENERIC", "USB DISK DEVICE",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Time DPA20B 1GB MP3 Player
		 * PR: usb/81846
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "USB2.0*", "(FS) FLASH DISK*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Samsung USB key 128Mb
		 * PR: usb/90081
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "USB-DISK", "FreeDik-FlashUsb",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Kingston DataTraveler 2.0 USB Flash memory.
		 * PR: usb/89196
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Kingston", "DataTraveler 2.0",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Creative MUVO Slim mp3 player (USB)
		 * PR: usb/86131
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "CREATIVE", "MuVo Slim",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE|DA_Q_NO_PREVENT
		},
	{
		/*
		 * United MP5512 Portable MP3 Player (2-in-1 USB DISK/MP3)
		 * PR: usb/80487
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Generic*", "MUSIC DISK",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * SanDisk Micro Cruzer 128MB
		 * PR: usb/75970
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "SanDisk" , "Micro Cruzer",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * TOSHIBA TransMemory USB sticks
		 * PR: kern/94660
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "TOSHIBA", "TransMemory",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * PNY USB Flash keys
		 * PR: usb/75578, usb/72344, usb/65436 
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "*" , "USB DISK*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Genesys 6-in-1 Card Reader
		 * PR: usb/94647
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Generic*", "STORAGE DEVICE*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Rekam Digital CAMERA
		 * PR: usb/98713
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "CAMERA*", "4MP-9J6*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * iRiver H10 MP3 player
		 * PR: usb/102547
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "iriver", "H10*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * iRiver U10 MP3 player
		 * PR: usb/92306
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "iriver", "U10*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * X-Micro Flash Disk
		 * PR: usb/96901
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "X-Micro", "Flash Disk",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * EasyMP3 EM732X USB 2.0 Flash MP3 Player
		 * PR: usb/96546
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "EM732X", "MP3 Player*",
		"1.0"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Denver MP3 player
		 * PR: usb/107101
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "DENVER", "MP3 PLAYER",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Philips USB Key Audio KEY013
		 * PR: usb/68412
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "PHILIPS", "Key*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE | DA_Q_NO_PREVENT
	},
	{
		/*
		 * JNC MP3 Player
		 * PR: usb/94439
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "JNC*" , "MP3 Player*",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * SAMSUNG MP0402H
		 * PR: usb/108427
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "SAMSUNG", "MP0402H", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * I/O Magic USB flash - Giga Bank
		 * PR: usb/108810
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "GS-Magic", "stor*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * JoyFly 128mb USB Flash Drive
		 * PR: 96133
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "USB 2.0", "Flash Disk*",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * ChipsBnk usb stick
		 * PR: 103702
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "ChipsBnk", "USB*",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Netac", "OnlyDisk*",
		 "2000"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	}
};

static	disk_strategy_t	dastrategy;
static	dumper_t	dadump;
static	periph_init_t	dainit;
static	void		daasync(void *callback_arg, u_int32_t code,
				struct cam_path *path, void *arg);
static	void		dasysctlinit(void *context, int pending);
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

#ifndef	DA_DEFAULT_SEND_ORDERED
#define	DA_DEFAULT_SEND_ORDERED	1
#endif


static int da_retry_count = DA_DEFAULT_RETRY;
static int da_default_timeout = DA_DEFAULT_TIMEOUT;
static int da_send_ordered = DA_DEFAULT_SEND_ORDERED;

SYSCTL_NODE(_kern_cam, OID_AUTO, da, CTLFLAG_RD, 0,
            "CAM Direct Access Disk driver");
SYSCTL_INT(_kern_cam_da, OID_AUTO, retry_count, CTLFLAG_RW,
           &da_retry_count, 0, "Normal I/O retry count");
TUNABLE_INT("kern.cam.da.retry_count", &da_retry_count);
SYSCTL_INT(_kern_cam_da, OID_AUTO, default_timeout, CTLFLAG_RW,
           &da_default_timeout, 0, "Normal I/O timeout (in seconds)");
TUNABLE_INT("kern.cam.da.default_timeout", &da_default_timeout);
SYSCTL_INT(_kern_cam_da, OID_AUTO, da_send_ordered, CTLFLAG_RW,
           &da_send_ordered, 0, "Send Ordered Tags");
TUNABLE_INT("kern.cam.da.da_send_ordered", &da_send_ordered);

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

MALLOC_DEFINE(M_SCSIDA, "scsi_da", "scsi_da buffers");

static int
daopen(struct disk *dp)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	int unit;
	int error;

	periph = (struct cam_periph *)dp->d_drv1;
	if (periph == NULL) {
		return (ENXIO);	
	}

	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		return(ENXIO);
	}

	cam_periph_lock(periph);
	if ((error = cam_periph_hold(periph, PRIBIO|PCATCH)) != 0) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return (error);
	}

	unit = periph->unit_number;
	softc = (struct da_softc *)periph->softc;
	softc->flags |= DA_FLAG_OPEN;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE,
	    ("daopen: disk=%s%d (unit %d)\n", dp->d_name, dp->d_unit,
	     unit));

	if ((softc->flags & DA_FLAG_PACK_INVALID) != 0) {
		/* Invalidate our pack information. */
		softc->flags &= ~DA_FLAG_PACK_INVALID;
	}

	error = dagetcapacity(periph);

	if (error == 0) {

		softc->disk->d_sectorsize = softc->params.secsize;
		softc->disk->d_mediasize = softc->params.secsize * (off_t)softc->params.sectors;
		/* XXX: these are not actually "firmware" values, so they may be wrong */
		softc->disk->d_fwsectors = softc->params.secs_per_track;
		softc->disk->d_fwheads = softc->params.heads;
		softc->disk->d_devstat->block_size = softc->params.secsize;
		softc->disk->d_devstat->flags &= ~DEVSTAT_BS_UNAVAILABLE;
	}
	
	if (error == 0) {
		if ((softc->flags & DA_FLAG_PACK_REMOVABLE) != 0 &&
		    (softc->quirks & DA_Q_NO_PREVENT) == 0)
			daprevent(periph, PR_PREVENT);
	} else {
		softc->flags &= ~DA_FLAG_OPEN;
		cam_periph_release(periph);
	}
	cam_periph_unhold(periph);
	cam_periph_unlock(periph);
	return (error);
}

static int
daclose(struct disk *dp)
{
	struct	cam_periph *periph;
	struct	da_softc *softc;
	int error;

	periph = (struct cam_periph *)dp->d_drv1;
	if (periph == NULL)
		return (ENXIO);	

	cam_periph_lock(periph);
	if ((error = cam_periph_hold(periph, PRIBIO)) != 0) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return (error);
	}

	softc = (struct da_softc *)periph->softc;

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
				  softc->disk->d_devstat);

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
				xpt_print(periph->path, "Synchronize cache "
				    "failed, status == 0x%x, scsi status == "
				    "0x%x\n", ccb->csio.ccb_h.status,
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
		if ((softc->quirks & DA_Q_NO_PREVENT) == 0)
			daprevent(periph, PR_ALLOW);
		/*
		 * If we've got removeable media, mark the blocksize as
		 * unavailable, since it could change when new media is
		 * inserted.
		 */
		softc->disk->d_devstat->flags |= DEVSTAT_BS_UNAVAILABLE;
	}

	softc->flags &= ~DA_FLAG_OPEN;
	cam_periph_unhold(periph);
	cam_periph_release(periph);
	cam_periph_unlock(periph);
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
	
	periph = (struct cam_periph *)bp->bio_disk->d_drv1;
	if (periph == NULL) {
		biofinish(bp, NULL, ENXIO);
		return;
	}
	softc = (struct da_softc *)periph->softc;

	cam_periph_lock(periph);

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
	
	/*
	 * If the device has been made invalid, error out
	 */
	if ((softc->flags & DA_FLAG_PACK_INVALID)) {
		cam_periph_unlock(periph);
		biofinish(bp, NULL, ENXIO);
		return;
	}
	
	/*
	 * Place it in the queue of disk activities for this disk
	 */
	bioq_disksort(&softc->bio_queue, bp);

	/*
	 * Schedule ourselves for performing the work.
	 */
	xpt_schedule(periph, /* XXX priority */1);
	cam_periph_unlock(periph);

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
	cam_periph_lock(periph);
	secsize = softc->params.secsize;
	
	if ((softc->flags & DA_FLAG_PACK_INVALID) != 0) {
		cam_periph_unlock(periph);
		return (ENXIO);
	}

	if (length > 0) {
		periph->flags |= CAM_PERIPH_POLLED;
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
			periph->flags |= CAM_PERIPH_POLLED;
			return(EIO);
		}
		cam_periph_unlock(periph);
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
				xpt_print(periph->path, "Synchronize cache "
				    "failed, status == 0x%x, scsi status == "
				    "0x%x\n", csio.ccb_h.status,
				    csio.scsi_status);
			}
		}
	}
	periph->flags &= ~CAM_PERIPH_POLLED;
	cam_periph_unlock(periph);
	return (0);
}

static void
dainit(void)
{
	cam_status status;

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, daasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("da: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	} else if (da_send_ordered) {

		/* Register our shutdown event handler */
		if ((EVENTHANDLER_REGISTER(shutdown_post_sync, dashutdown, 
					   NULL, SHUTDOWN_PRI_DEFAULT)) == NULL)
		    printf("dainit: shutdown event registration failed!\n");
	}
}

static void
daoninvalidate(struct cam_periph *periph)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_register_async(0, daasync, periph, periph->path);

	softc->flags |= DA_FLAG_PACK_INVALID;

	/*
	 * Return all queued I/O with ENXIO.
	 * XXX Handle any transactions queued to the card
	 *     with XPT_ABORT_CCB.
	 */
	bioq_flush(&softc->bio_queue, NULL, ENXIO);

	disk_gone(softc->disk);
	xpt_print(periph->path, "lost device\n");
}

static void
dacleanup(struct cam_periph *periph)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	xpt_print(periph->path, "removing device entry\n");
	/*
	 * If we can't free the sysctl tree, oh well...
	 */
	if ((softc->flags & DA_FLAG_SCTX_INIT) != 0
	    && sysctl_ctx_free(&softc->sysctl_ctx) != 0) {
		xpt_print(periph->path, "can't remove sysctl context\n");
	}

	cam_periph_unlock(periph);
	disk_destroy(softc->disk);
	callout_drain(&softc->sendordered_c);
	cam_periph_lock(periph);
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
		struct cam_sim *sim;
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
		sim = xpt_path_sim(cgd->ccb_h.path);
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

		softc = (struct da_softc *)periph->softc;
		/*
		 * Don't fail on the expected unit attention
		 * that will occur.
		 */
		softc->flags |= DA_FLAG_RETRY_UA;
		LIST_FOREACH(ccbh, &softc->pending_ccbs, periph_links.le)
			ccbh->ccb_state |= DA_CCB_RETRY_UA;
		/* FALLTHROUGH*/
	}
	default:
		cam_periph_async(periph, code, path, arg);
		break;
	}
}

static void
dasysctlinit(void *context, int pending)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	char tmpstr[80], tmpstr2[80];

	periph = (struct cam_periph *)context;
	if (cam_periph_acquire(periph) != CAM_REQ_CMP)
		return;

	softc = (struct da_softc *)periph->softc;
	snprintf(tmpstr, sizeof(tmpstr), "CAM DA unit %d", periph->unit_number);
	snprintf(tmpstr2, sizeof(tmpstr2), "%d", periph->unit_number);

	mtx_lock(&Giant);
	sysctl_ctx_init(&softc->sysctl_ctx);
	softc->flags |= DA_FLAG_SCTX_INIT;
	softc->sysctl_tree = SYSCTL_ADD_NODE(&softc->sysctl_ctx,
		SYSCTL_STATIC_CHILDREN(_kern_cam_da), OID_AUTO, tmpstr2,
		CTLFLAG_RD, 0, tmpstr);
	if (softc->sysctl_tree == NULL) {
		printf("dasysctlinit: unable to allocate sysctl tree\n");
		mtx_unlock(&Giant);
		cam_periph_release(periph);
		return;
	}

	/*
	 * Now register the sysctl handler, so the user can the value on
	 * the fly.
	 */
	SYSCTL_ADD_PROC(&softc->sysctl_ctx,SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "minimum_cmd_size", CTLTYPE_INT | CTLFLAG_RW,
		&softc->minimum_cmd_size, 0, dacmdsizesysctl, "I",
		"Minimum CDB size");

	mtx_unlock(&Giant);
	cam_periph_release(periph);
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
	struct da_softc *softc;
	struct ccb_pathinq cpi;
	struct ccb_getdev *cgd;
	char tmpstr[80];
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

	TASK_INIT(&softc->sysctl_task, 0, dasysctlinit, periph);

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
	 * Register this media as a disk
	 */

	mtx_unlock(periph->sim->mtx);
	softc->disk = disk_alloc();
	softc->disk->d_open = daopen;
	softc->disk->d_close = daclose;
	softc->disk->d_strategy = dastrategy;
	softc->disk->d_dump = dadump;
	softc->disk->d_name = "da";
	softc->disk->d_drv1 = periph;
	softc->disk->d_maxsize = DFLTPHYS; /* XXX: probably not arbitrary */
	softc->disk->d_unit = periph->unit_number;
	softc->disk->d_flags = 0;
	if ((softc->quirks & DA_Q_NO_SYNC_CACHE) == 0)
		softc->disk->d_flags |= DISKFLAG_CANFLUSHCACHE;
	disk_create(softc->disk, DISK_VERSION);
	mtx_lock(periph->sim->mtx);

	/*
	 * Add async callbacks for bus reset and
	 * bus device reset calls.  I don't bother
	 * checking if this fails as, in most cases,
	 * the system will function just fine without
	 * them and the only alternative would be to
	 * not attach the device on failure.
	 */
	xpt_register_async(AC_SENT_BDR | AC_BUS_RESET | AC_LOST_DEVICE,
			   daasync, periph, periph->path);

	/*
	 * Take an exclusive refcount on the periph while dastart is called
	 * to finish the probe.  The reference will be dropped in dadone at
	 * the end of probe.
	 */
	(void)cam_periph_hold(periph, PRIBIO);
	xpt_schedule(periph, /*priority*/5);

	/*
	 * Schedule a periodic event to occasionally send an
	 * ordered tag to a device.
	 */
	callout_init_mtx(&softc->sendordered_c, periph->sim->mtx, 0);
	callout_reset(&softc->sendordered_c,
	    (DA_DEFAULT_TIMEOUT * hz) / DA_ORDEREDTAG_INTERVAL,
	    dasendorderedtag, softc);

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

		/*
		 * See if there is a buf with work for us to do..
		 */
		bp = bioq_first(&softc->bio_queue);
		if (periph->immediate_priority <= periph->pinfo.priority) {
			CAM_DEBUG_PRINT(CAM_DEBUG_SUBTRACE,
					("queuing for immediate ccb\n"));
			start_ccb->ccb_h.ccb_state = DA_CCB_WAITING;
			SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
					  periph_links.sle);
			periph->immediate_priority = CAM_PRIORITY_NONE;
			wakeup(&periph->ccb_list);
		} else if (bp == NULL) {
			xpt_release_ccb(start_ccb);
		} else {
			u_int8_t tag_code;

			bioq_remove(&softc->bio_queue, bp);

			if ((softc->flags & DA_FLAG_NEED_OTAG) != 0) {
				softc->flags &= ~DA_FLAG_NEED_OTAG;
				softc->ordered_tag_count++;
				tag_code = MSG_ORDERED_Q_TAG;
			} else {
				tag_code = MSG_SIMPLE_Q_TAG;
			}
			switch (bp->bio_cmd) {
			case BIO_READ:
			case BIO_WRITE:
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
				break;
			case BIO_FLUSH:
				scsi_synchronize_cache(&start_ccb->csio,
						       /*retries*/1,
						       /*cbfcnp*/dadone,
						       MSG_SIMPLE_Q_TAG,
						       /*begin_lba*/0,/* Cover the whole disk */
						       /*lb_count*/0,
						       SSD_FULL_SIZE,
						       /*timeout*/da_default_timeout*1000);
				break;
			}
			start_ccb->ccb_h.ccb_state = DA_CCB_BUFFER_IO;

			/*
			 * Block out any asyncronous callbacks
			 * while we touch the pending ccb list.
			 */
			LIST_INSERT_HEAD(&softc->pending_ccbs,
					 &start_ccb->ccb_h, periph_links.le);
			softc->outstanding_cmds++;

			/* We expect a unit attention from this device */
			if ((softc->flags & DA_FLAG_RETRY_UA) != 0) {
				start_ccb->ccb_h.ccb_state |= DA_CCB_RETRY_UA;
				softc->flags &= ~DA_FLAG_RETRY_UA;
			}

			start_ccb->ccb_h.ccb_bp = bp;
			bp = bioq_first(&softc->bio_queue);

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

		rcap = (struct scsi_read_capacity_data *)
		    malloc(sizeof(*rcap), M_SCSIDA, M_NOWAIT|M_ZERO);
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
			malloc(sizeof(*rcaplong), M_SCSIDA, M_NOWAIT|M_ZERO);
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

	xpt_print(ccb->ccb_h.path, "READ(6)/WRITE(6) not supported, "
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

		bp = (struct bio *)done_ccb->ccb_h.ccb_bp;
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			int error;
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

				if (error == ENXIO) {
					/*
					 * Catastrophic error.  Mark our pack as
					 * invalid.
					 */
					/*
					 * XXX See if this is really a media
					 * XXX change first?
					 */
					xpt_print(periph->path,
					    "Invalidating pack\n");
					softc->flags |= DA_FLAG_PACK_INVALID;
				}

				/*
				 * return all queued I/O with EIO, so that
				 * the client can retry these I/Os in the
				 * proper order should it attempt to recover.
				 */
				bioq_flush(&softc->bio_queue, NULL, EIO);
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
		LIST_REMOVE(&done_ccb->ccb_h, periph_links.le);
		softc->outstanding_cmds--;
		if (softc->outstanding_cmds == 0)
			softc->flags |= DA_FLAG_WENT_IDLE;

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
					free(rdcap, M_SCSIDA);
					xpt_release_ccb(done_ccb);
					xpt_schedule(periph, /*priority*/5);
					return;
				}
			} else {
				block_size = scsi_4btoul(rcaplong->length);
				maxsector = scsi_8btou64(rcaplong->addr);
			}

			/*
			 * Because GEOM code just will panic us if we
			 * give them an 'illegal' value we'll avoid that
			 * here.
			 */
			if (block_size >= MAXPHYS || block_size == 0) {
				xpt_print(periph->path,
				    "unsupportable block size %ju\n",
				    (uintmax_t) block_size);
				announce_buf[0] = '\0';
				cam_periph_invalidate(periph);
			} else {
				dasetgeom(periph, block_size, maxsector);
				dp = &softc->params;
				snprintf(announce_buf, sizeof(announce_buf),
				        "%juMB (%ju %u byte sectors: %dH %dS/T "
                                        "%dC)", (uintmax_t)
	                                (((uintmax_t)dp->secsize *
				        dp->sectors) / (1024*1024)),
			                (uintmax_t)dp->sectors,
				        dp->secsize, dp->heads,
                                        dp->secs_per_track, dp->cylinders);
			}
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
						xpt_print(periph->path,
						    "got CAM status %#x\n",
						    done_ccb->ccb_h.status);
					}

					xpt_print(periph->path, "fatal error, "
					    "failed to attach to device\n");

					/*
					 * Free up resources.
					 */
					cam_periph_invalidate(periph);
				} 
			}
		}
		free(csio->data_ptr, M_SCSIDA);
		if (announce_buf[0] != '\0') {
			xpt_announce_periph(periph, announce_buf);
			/*
			 * Create our sysctl variables, now that we know
			 * we have successfully attached.
			 */
			taskqueue_enqueue(taskqueue_thread,&softc->sysctl_task);
		}
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
		cam_periph_unhold(periph);
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
				  SF_RETRY_UA, softc->disk->d_devstat);

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
	u_int32_t sense_flags;

	softc = (struct da_softc *)periph->softc;
	block_len = 0;
	maxsector = 0;
	error = 0;
	sense_flags = SF_RETRY_UA;
	if (softc->flags & DA_FLAG_PACK_REMOVABLE)
		sense_flags |= SF_NO_PRINT;

	/* Do a read capacity */
	rcap = (struct scsi_read_capacity_data *)malloc(sizeof(*rcaplong),
							M_SCSIDA,
							M_NOWAIT);
	if (rcap == NULL)
		return (ENOMEM);
		
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
				  sense_flags,
				  softc->disk->d_devstat);

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
				  sense_flags,
				  softc->disk->d_devstat);

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

	free(rcap, M_SCSIDA);

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
	if ((ccg.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		/*
		 * We don't know what went wrong here- but just pick
		 * a geometry so we don't have nasty things like divide
		 * by zero.
		 */
		dp->heads = 255;
		dp->secs_per_track = 255;
		dp->cylinders = dp->sectors / (255 * 255);
		if (dp->cylinders == 0) {
			dp->cylinders = 1;
		}
	} else {
		dp->heads = ccg.heads;
		dp->secs_per_track = ccg.secs_per_track;
		dp->cylinders = ccg.cylinders;
	}
}

static void
dasendorderedtag(void *arg)
{
	struct da_softc *softc = arg;

	if (da_send_ordered) {
		if ((softc->ordered_tag_count == 0) 
		 && ((softc->flags & DA_FLAG_WENT_IDLE) == 0)) {
			softc->flags |= DA_FLAG_NEED_OTAG;
		}
		if (softc->outstanding_cmds > 0)
			softc->flags &= ~DA_FLAG_WENT_IDLE;

		softc->ordered_tag_count = 0;
	}
	/* Queue us up again */
	callout_reset(&softc->sendordered_c,
	    (DA_DEFAULT_TIMEOUT * hz) / DA_ORDEREDTAG_INTERVAL,
	    dasendorderedtag, softc);
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

		cam_periph_lock(periph);
		softc = (struct da_softc *)periph->softc;

		/*
		 * We only sync the cache if the drive is still open, and
		 * if the drive is capable of it..
		 */
		if (((softc->flags & DA_FLAG_OPEN) == 0)
		 || (softc->quirks & DA_Q_NO_SYNC_CACHE)) {
			cam_periph_unlock(periph);
			continue;
		}

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
				xpt_print(periph->path, "Synchronize "
				    "cache failed, status == 0x%x, scsi status "
				    "== 0x%x\n", ccb.ccb_h.status,
				    ccb.csio.scsi_status);
			}
		}

		if ((ccb.ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(ccb.ccb_h.path,
					 /*relsim_flags*/0,
					 /*reduction*/0,
					 /*timeout*/0,
					 /*getcount_only*/0);
		cam_periph_unlock(periph);
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
