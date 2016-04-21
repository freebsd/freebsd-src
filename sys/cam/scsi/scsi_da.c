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
#include <sys/endian.h>
#include <sys/proc.h>
#include <geom/geom.h>
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
#include <cam/cam_iosched.h>

#include <cam/scsi/scsi_message.h>

#ifndef _KERNEL 
#include <cam/scsi/scsi_da.h>
#endif /* !_KERNEL */

#ifdef _KERNEL
typedef enum {
	DA_STATE_PROBE_RC,
	DA_STATE_PROBE_RC16,
	DA_STATE_PROBE_LBP,
	DA_STATE_PROBE_BLK_LIMITS,
	DA_STATE_PROBE_BDC,
	DA_STATE_PROBE_ATA,
	DA_STATE_NORMAL
} da_state;

typedef enum {
	DA_FLAG_PACK_INVALID	= 0x001,
	DA_FLAG_NEW_PACK	= 0x002,
	DA_FLAG_PACK_LOCKED	= 0x004,
	DA_FLAG_PACK_REMOVABLE	= 0x008,
	DA_FLAG_NEED_OTAG	= 0x020,
	DA_FLAG_WAS_OTAG	= 0x040,
	DA_FLAG_RETRY_UA	= 0x080,
	DA_FLAG_OPEN		= 0x100,
	DA_FLAG_SCTX_INIT	= 0x200,
	DA_FLAG_CAN_RC16	= 0x400,
	DA_FLAG_PROBED		= 0x800,
	DA_FLAG_DIRTY		= 0x1000,
	DA_FLAG_ANNOUNCED	= 0x2000
} da_flags;

typedef enum {
	DA_Q_NONE		= 0x00,
	DA_Q_NO_SYNC_CACHE	= 0x01,
	DA_Q_NO_6_BYTE		= 0x02,
	DA_Q_NO_PREVENT		= 0x04,
	DA_Q_4K			= 0x08,
	DA_Q_NO_RC16		= 0x10,
	DA_Q_NO_UNMAP		= 0x20,
	DA_Q_RETRY_BUSY		= 0x40
} da_quirks;

#define DA_Q_BIT_STRING		\
	"\020"			\
	"\001NO_SYNC_CACHE"	\
	"\002NO_6_BYTE"		\
	"\003NO_PREVENT"	\
	"\0044K"		\
	"\005NO_RC16"		\
	"\006NO_UNMAP"		\
	"\007RETRY_BUSY"

typedef enum {
	DA_CCB_PROBE_RC		= 0x01,
	DA_CCB_PROBE_RC16	= 0x02,
	DA_CCB_PROBE_LBP	= 0x03,
	DA_CCB_PROBE_BLK_LIMITS	= 0x04,
	DA_CCB_PROBE_BDC	= 0x05,
	DA_CCB_PROBE_ATA	= 0x06,
	DA_CCB_BUFFER_IO	= 0x07,
	DA_CCB_DUMP		= 0x0A,
	DA_CCB_DELETE		= 0x0B,
 	DA_CCB_TUR		= 0x0C,
	DA_CCB_TYPE_MASK	= 0x0F,
	DA_CCB_RETRY_UA		= 0x10
} da_ccb_state;

/*
 * Order here is important for method choice
 *
 * We prefer ATA_TRIM as tests run against a Sandforce 2281 SSD attached to
 * LSI 2008 (mps) controller (FW: v12, Drv: v14) resulted 20% quicker deletes
 * using ATA_TRIM than the corresponding UNMAP results for a real world mysql
 * import taking 5mins.
 *
 */
typedef enum {
	DA_DELETE_NONE,
	DA_DELETE_DISABLE,
	DA_DELETE_ATA_TRIM,
	DA_DELETE_UNMAP,
	DA_DELETE_WS16,
	DA_DELETE_WS10,
	DA_DELETE_ZERO,
	DA_DELETE_MIN = DA_DELETE_ATA_TRIM,
	DA_DELETE_MAX = DA_DELETE_ZERO
} da_delete_methods;

typedef void da_delete_func_t (struct cam_periph *periph, union ccb *ccb,
			      struct bio *bp);
static da_delete_func_t da_delete_trim;
static da_delete_func_t da_delete_unmap;
static da_delete_func_t da_delete_ws;

static const void * da_delete_functions[] = {
	NULL,
	NULL,
	da_delete_trim,
	da_delete_unmap,
	da_delete_ws,
	da_delete_ws,
	da_delete_ws
};

static const char *da_delete_method_names[] =
    { "NONE", "DISABLE", "ATA_TRIM", "UNMAP", "WS16", "WS10", "ZERO" };
static const char *da_delete_method_desc[] =
    { "NONE", "DISABLED", "ATA TRIM", "UNMAP", "WRITE SAME(16) with UNMAP",
      "WRITE SAME(10) with UNMAP", "ZERO" };

/* Offsets into our private area for storing information */
#define ccb_state	ppriv_field0
#define ccb_bp		ppriv_ptr1

struct disk_params {
	u_int8_t  heads;
	u_int32_t cylinders;
	u_int8_t  secs_per_track;
	u_int32_t secsize;	/* Number of bytes/sector */
	u_int64_t sectors;	/* total number sectors */
	u_int     stripesize;
	u_int     stripeoffset;
};

#define UNMAP_RANGE_MAX		0xffffffff
#define UNMAP_HEAD_SIZE		8
#define UNMAP_RANGE_SIZE	16
#define UNMAP_MAX_RANGES	2048 /* Protocol Max is 4095 */
#define UNMAP_BUF_SIZE		((UNMAP_MAX_RANGES * UNMAP_RANGE_SIZE) + \
				UNMAP_HEAD_SIZE)

#define WS10_MAX_BLKS		0xffff
#define WS16_MAX_BLKS		0xffffffff
#define ATA_TRIM_MAX_RANGES	((UNMAP_BUF_SIZE / \
	(ATA_DSM_RANGE_SIZE * ATA_DSM_BLK_SIZE)) * ATA_DSM_BLK_SIZE)

#define DA_WORK_TUR		(1 << 16)

struct da_softc {
	struct   cam_iosched_softc *cam_iosched;
	struct	 bio_queue_head delete_run_queue;
	LIST_HEAD(, ccb_hdr) pending_ccbs;
	int	 refcount;		/* Active xpt_action() calls */
	da_state state;
	da_flags flags;	
	da_quirks quirks;
	int	 minimum_cmd_size;
	int	 error_inject;
	int	 trim_max_ranges;
	int	 delete_available;	/* Delete methods possibly available */
	u_int	 maxio;
	uint32_t		unmap_max_ranges;
	uint32_t		unmap_max_lba; /* Max LBAs in UNMAP req */
	uint64_t		ws_max_blks;
	da_delete_methods	delete_method_pref;
	da_delete_methods	delete_method;
	da_delete_func_t	*delete_func;
	int			unmappedio;
	int			rotating;
	struct	 disk_params params;
	struct	 disk *disk;
	union	 ccb saved_ccb;
	struct task		sysctl_task;
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
	struct callout		sendordered_c;
	uint64_t wwpn;
	uint8_t	 unmap_buf[UNMAP_BUF_SIZE];
	struct scsi_read_capacity_data_long rcaplong;
	struct callout		mediapoll_c;
#ifdef CAM_IO_STATS
	struct sysctl_ctx_list	sysctl_stats_ctx;
	struct sysctl_oid	*sysctl_stats_tree;
	u_int	errors;
	u_int	timeouts;
	u_int	invalidations;
#endif
};

#define dadeleteflag(softc, delete_method, enable)			\
	if (enable) {							\
		softc->delete_available |= (1 << delete_method);	\
	} else {							\
		softc->delete_available &= ~(1 << delete_method);	\
	}

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
	{
		/*
		 * The STEC SSDs sometimes hang on UNMAP.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "STEC", "*", "*"},
		/*quirks*/ DA_Q_NO_UNMAP
	},
	{
		/*
		 * VMware returns BUSY status when storage has transient
		 * connectivity problems, so better wait.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "VMware*", "*", "*"},
		/*quirks*/ DA_Q_RETRY_BUSY
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
		 * USB DISK Pro PMAP
		 * Reported by: jhs
		 * PR: usb/96381
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, " ", "USB DISK Pro", "PMAP"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
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
		 * PNY USB 3.0 Flash Drives
		*/
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "PNY", "USB 3.0 FD*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE | DA_Q_NO_RC16
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
		"1.00"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
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
		/*
		 * Storcase (Kingston) InfoStation IFS FC2/SATA-R 201A
		 * PR: 129858
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "IFS", "FC2/SATA-R*",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Samsung YP-U3 mp3-player
		 * PR: 125398
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Samsung", "YP-U3",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Netac", "OnlyDisk*",
		 "2000"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Sony Cyber-Shot DSC cameras
		 * PR: usb/137035
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Sony", "Sony DSC", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE | DA_Q_NO_PREVENT
	},
	{
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Kingston", "DataTraveler G3",
		 "1.00"}, /*quirks*/ DA_Q_NO_PREVENT
	},
	{
		/* At least several Transcent USB sticks lie on RC16. */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "JetFlash", "Transcend*",
		 "*"}, /*quirks*/ DA_Q_NO_RC16
	},
	/* ATA/SATA devices over SAS/USB/... */
	{
		/* Hitachi Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "Hitachi", "H??????????E3*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Samsung Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "SAMSUNG HD155UI*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Samsung Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "SAMSUNG", "HD155UI*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Samsung Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "SAMSUNG HD204UI*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Samsung Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "SAMSUNG", "HD204UI*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST????DL*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST????DL", "*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST???DM*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST???DM*", "*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST????DM*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST????DM", "*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9500423AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST950042", "3AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9500424AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST950042", "4AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9640423AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST964042", "3AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9640424AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST964042", "4AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9750420AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST975042", "0AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9750422AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST975042", "2AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9750423AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST975042", "3AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Thin Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST???LT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Thin Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST???LT*", "*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD????RS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "??RS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD????RX*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "??RX*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD??????RS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "????RS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD??????RX*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "????RX*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Black Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD???PKT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Black Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "?PKT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Black Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD?????PKT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Black Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "???PKT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Blue Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD???PVT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Blue Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "?PVT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Blue Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD?????PVT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Blue Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "???PVT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Olympus FE-210 camera
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "OLYMPUS", "FE210*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * LG UP3S MP3 player
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "LG", "UP3S",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Laser MP3-2GA13 MP3 player
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "USB 2.0", "(HS) Flash Disk",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * LaCie external 250GB Hard drive des by Porsche
		 * Submitted by: Ben Stuyts <ben@altesco.nl>
		 * PR: 121474
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "SAMSUNG", "HM250JI", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	/* SATA SSDs */
	{
		/*
		 * Corsair Force 2 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "Corsair CSSD-F*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Corsair Force 3 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "Corsair Force 3*", "*" },
		/*quirks*/DA_Q_4K
	},
        {
		/*
		 * Corsair Neutron GTX SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "Corsair Neutron GTX*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Corsair Force GT & GS SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "Corsair Force G*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Crucial M4 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "M4-CT???M4SSD2*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Crucial RealSSD C300 SSDs
		 * 4k optimised
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "C300-CTFDDAC???MAG*",
		"*" }, /*quirks*/DA_Q_4K
	},
	{
		/*
		 * Intel 320 Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "INTEL SSDSA2CW*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Intel 330 Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "INTEL SSDSC2CT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Intel 510 Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "INTEL SSDSC2MH*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Intel 520 Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "INTEL SSDSC2BW*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Intel X25-M Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "INTEL SSDSA2M*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Kingston E100 Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "KINGSTON SE100S3*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Kingston HyperX 3k SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "KINGSTON SH103S3*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Marvell SSDs (entry taken from OpenSolaris)
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "MARVELL SD88SA02*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * OCZ Agility 2 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "*", "OCZ-AGILITY2*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * OCZ Agility 3 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "OCZ-AGILITY3*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * OCZ Deneva R Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "DENRSTE251M45*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * OCZ Vertex 2 SSDs (inc pro series)
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "OCZ?VERTEX2*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * OCZ Vertex 3 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "OCZ-VERTEX3*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * OCZ Vertex 4 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "OCZ-VERTEX4*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Samsung 830 Series SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "SAMSUNG SSD 830 Series*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Samsung 840 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "Samsung SSD 840*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Samsung 850 SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "Samsung SSD 850*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Samsung 843T Series SSDs (MZ7WD*)
		 * Samsung PM851 Series SSDs (MZ7TE*)
		 * Samsung PM853T Series SSDs (MZ7GE*)
		 * Samsung SM863 Series SSDs (MZ7KM*)
		 * 4k optimised
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "SAMSUNG MZ7*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * SuperTalent TeraDrive CT SSDs
		 * 4k optimised & trim only works in 4k requests + 4k aligned
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "FTM??CT25H*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * XceedIOPS SATA SSDs
		 * 4k optimised
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "SG9XCS2D*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Hama Innostor USB-Stick 
		 */
		{ T_DIRECT, SIP_MEDIA_REMOVABLE, "Innostor", "Innostor*", "*" }, 
		/*quirks*/DA_Q_NO_RC16
	},
	{
		/*
		 * MX-ES USB Drive by Mach Xtreme
		 */
		{ T_DIRECT, SIP_MEDIA_REMOVABLE, "MX", "MXUB3*", "*"},
		/*quirks*/DA_Q_NO_RC16
	},
};

static	disk_strategy_t	dastrategy;
static	dumper_t	dadump;
static	periph_init_t	dainit;
static	void		daasync(void *callback_arg, u_int32_t code,
				struct cam_path *path, void *arg);
static	void		dasysctlinit(void *context, int pending);
static	int		dasysctlsofttimeout(SYSCTL_HANDLER_ARGS);
static	int		dacmdsizesysctl(SYSCTL_HANDLER_ARGS);
static	int		dadeletemethodsysctl(SYSCTL_HANDLER_ARGS);
static	int		dadeletemaxsysctl(SYSCTL_HANDLER_ARGS);
static	void		dadeletemethodset(struct da_softc *softc,
					  da_delete_methods delete_method);
static	off_t		dadeletemaxsize(struct da_softc *softc,
					da_delete_methods delete_method);
static	void		dadeletemethodchoose(struct da_softc *softc,
					     da_delete_methods default_method);
static	void		daprobedone(struct cam_periph *periph, union ccb *ccb);

static	periph_ctor_t	daregister;
static	periph_dtor_t	dacleanup;
static	periph_start_t	dastart;
static	periph_oninv_t	daoninvalidate;
static	void		dadone(struct cam_periph *periph,
			       union ccb *done_ccb);
static  int		daerror(union ccb *ccb, u_int32_t cam_flags,
				u_int32_t sense_flags);
static void		daprevent(struct cam_periph *periph, int action);
static void		dareprobe(struct cam_periph *periph);
static void		dasetgeom(struct cam_periph *periph, uint32_t block_len,
				  uint64_t maxsector,
				  struct scsi_read_capacity_data_long *rcaplong,
				  size_t rcap_size);
static timeout_t	dasendorderedtag;
static void		dashutdown(void *arg, int howto);
static timeout_t	damediapoll;

#ifndef	DA_DEFAULT_POLL_PERIOD
#define	DA_DEFAULT_POLL_PERIOD	3
#endif

#ifndef DA_DEFAULT_TIMEOUT
#define DA_DEFAULT_TIMEOUT 60	/* Timeout in seconds */
#endif

#ifndef DA_DEFAULT_SOFTTIMEOUT
#define DA_DEFAULT_SOFTTIMEOUT	0
#endif

#ifndef	DA_DEFAULT_RETRY
#define	DA_DEFAULT_RETRY	4
#endif

#ifndef	DA_DEFAULT_SEND_ORDERED
#define	DA_DEFAULT_SEND_ORDERED	1
#endif

static int da_poll_period = DA_DEFAULT_POLL_PERIOD;
static int da_retry_count = DA_DEFAULT_RETRY;
static int da_default_timeout = DA_DEFAULT_TIMEOUT;
static sbintime_t da_default_softtimeout = DA_DEFAULT_SOFTTIMEOUT;
static int da_send_ordered = DA_DEFAULT_SEND_ORDERED;

static SYSCTL_NODE(_kern_cam, OID_AUTO, da, CTLFLAG_RD, 0,
            "CAM Direct Access Disk driver");
SYSCTL_INT(_kern_cam_da, OID_AUTO, poll_period, CTLFLAG_RWTUN,
           &da_poll_period, 0, "Media polling period in seconds");
SYSCTL_INT(_kern_cam_da, OID_AUTO, retry_count, CTLFLAG_RWTUN,
           &da_retry_count, 0, "Normal I/O retry count");
SYSCTL_INT(_kern_cam_da, OID_AUTO, default_timeout, CTLFLAG_RWTUN,
           &da_default_timeout, 0, "Normal I/O timeout (in seconds)");
SYSCTL_INT(_kern_cam_da, OID_AUTO, send_ordered, CTLFLAG_RWTUN,
           &da_send_ordered, 0, "Send Ordered Tags");

SYSCTL_PROC(_kern_cam_da, OID_AUTO, default_softtimeout,
    CTLTYPE_UINT | CTLFLAG_RW, NULL, 0, dasysctlsofttimeout, "I",
    "Soft I/O timeout (ms)");
TUNABLE_INT64("kern.cam.da.default_softtimeout", &da_default_softtimeout);

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

static MALLOC_DEFINE(M_SCSIDA, "scsi_da", "scsi_da buffers");

static int
daopen(struct disk *dp)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	int error;

	periph = (struct cam_periph *)dp->d_drv1;
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		return (ENXIO);
	}

	cam_periph_lock(periph);
	if ((error = cam_periph_hold(periph, PRIBIO|PCATCH)) != 0) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return (error);
	}

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE | CAM_DEBUG_PERIPH,
	    ("daopen\n"));

	softc = (struct da_softc *)periph->softc;
	dareprobe(periph);

	/* Wait for the disk size update.  */
	error = cam_periph_sleep(periph, &softc->disk->d_mediasize, PRIBIO,
	    "dareprobe", 0);
	if (error != 0)
		xpt_print(periph->path, "unable to retrieve capacity data\n");

	if (periph->flags & CAM_PERIPH_INVALID)
		error = ENXIO;

	if (error == 0 && (softc->flags & DA_FLAG_PACK_REMOVABLE) != 0 &&
	    (softc->quirks & DA_Q_NO_PREVENT) == 0)
		daprevent(periph, PR_PREVENT);

	if (error == 0) {
		softc->flags &= ~DA_FLAG_PACK_INVALID;
		softc->flags |= DA_FLAG_OPEN;
	}

	cam_periph_unhold(periph);
	cam_periph_unlock(periph);

	if (error != 0)
		cam_periph_release(periph);

	return (error);
}

static int
daclose(struct disk *dp)
{
	struct	cam_periph *periph;
	struct	da_softc *softc;
	union	ccb *ccb;
	int error;

	periph = (struct cam_periph *)dp->d_drv1;
	softc = (struct da_softc *)periph->softc;
	cam_periph_lock(periph);
	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE | CAM_DEBUG_PERIPH,
	    ("daclose\n"));

	if (cam_periph_hold(periph, PRIBIO) == 0) {

		/* Flush disk cache. */
		if ((softc->flags & DA_FLAG_DIRTY) != 0 &&
		    (softc->quirks & DA_Q_NO_SYNC_CACHE) == 0 &&
		    (softc->flags & DA_FLAG_PACK_INVALID) == 0) {
			ccb = cam_periph_getccb(periph, CAM_PRIORITY_NORMAL);
			scsi_synchronize_cache(&ccb->csio, /*retries*/1,
			    /*cbfcnp*/dadone, MSG_SIMPLE_Q_TAG,
			    /*begin_lba*/0, /*lb_count*/0, SSD_FULL_SIZE,
			    5 * 60 * 1000);
			error = cam_periph_runccb(ccb, daerror, /*cam_flags*/0,
			    /*sense_flags*/SF_RETRY_UA | SF_QUIET_IR,
			    softc->disk->d_devstat);
			if (error == 0)
				softc->flags &= ~DA_FLAG_DIRTY;
			xpt_release_ccb(ccb);
		}

		/* Allow medium removal. */
		if ((softc->flags & DA_FLAG_PACK_REMOVABLE) != 0 &&
		    (softc->quirks & DA_Q_NO_PREVENT) == 0)
			daprevent(periph, PR_ALLOW);

		cam_periph_unhold(periph);
	}

	/*
	 * If we've got removeable media, mark the blocksize as
	 * unavailable, since it could change when new media is
	 * inserted.
	 */
	if ((softc->flags & DA_FLAG_PACK_REMOVABLE) != 0)
		softc->disk->d_devstat->flags |= DEVSTAT_BS_UNAVAILABLE;

	softc->flags &= ~DA_FLAG_OPEN;
	while (softc->refcount != 0)
		cam_periph_sleep(periph, &softc->refcount, PRIBIO, "daclose", 1);
	cam_periph_unlock(periph);
	cam_periph_release(periph);
	return (0);
}

static void
daschedule(struct cam_periph *periph)
{
	struct da_softc *softc = (struct da_softc *)periph->softc;

	if (softc->state != DA_STATE_NORMAL)
		return;

	cam_iosched_schedule(softc->cam_iosched, periph);
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
	softc = (struct da_softc *)periph->softc;

	cam_periph_lock(periph);

	/*
	 * If the device has been made invalid, error out
	 */
	if ((softc->flags & DA_FLAG_PACK_INVALID)) {
		cam_periph_unlock(periph);
		biofinish(bp, NULL, ENXIO);
		return;
	}

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("dastrategy(%p)\n", bp));

	/*
	 * Place it in the queue of disk activities for this disk
	 */
	cam_iosched_queue_work(softc->cam_iosched, bp);

	/*
	 * Schedule ourselves for performing the work.
	 */
	daschedule(periph);
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
	int	    error = 0;

	dp = arg;
	periph = dp->d_drv1;
	softc = (struct da_softc *)periph->softc;
	cam_periph_lock(periph);
	secsize = softc->params.secsize;
	
	if ((softc->flags & DA_FLAG_PACK_INVALID) != 0) {
		cam_periph_unlock(periph);
		return (ENXIO);
	}

	if (length > 0) {
		xpt_setup_ccb(&csio.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
		csio.ccb_h.ccb_state = DA_CCB_DUMP;
		scsi_read_write(&csio,
				/*retries*/0,
				dadone,
				MSG_ORDERED_Q_TAG,
				/*read*/SCSI_RW_WRITE,
				/*byte2*/0,
				/*minimum_cmd_size*/ softc->minimum_cmd_size,
				offset / secsize,
				length / secsize,
				/*data_ptr*/(u_int8_t *) virtual,
				/*dxfer_len*/length,
				/*sense_len*/SSD_FULL_SIZE,
				da_default_timeout * 1000);
		xpt_polled_action((union ccb *)&csio);

		error = cam_periph_error((union ccb *)&csio,
		    0, SF_NO_RECOVERY | SF_NO_RETRY, NULL);
		if ((csio.ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(csio.ccb_h.path, /*relsim_flags*/0,
			    /*reduction*/0, /*timeout*/0, /*getcount_only*/0);
		if (error != 0)
			printf("Aborting dump due to I/O error.\n");
		cam_periph_unlock(periph);
		return (error);
	}
		
	/*
	 * Sync the disk cache contents to the physical media.
	 */
	if ((softc->quirks & DA_Q_NO_SYNC_CACHE) == 0) {

		xpt_setup_ccb(&csio.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
		csio.ccb_h.ccb_state = DA_CCB_DUMP;
		scsi_synchronize_cache(&csio,
				       /*retries*/0,
				       /*cbfcnp*/dadone,
				       MSG_SIMPLE_Q_TAG,
				       /*begin_lba*/0,/* Cover the whole disk */
				       /*lb_count*/0,
				       SSD_FULL_SIZE,
				       5 * 1000);
		xpt_polled_action((union ccb *)&csio);

		error = cam_periph_error((union ccb *)&csio,
		    0, SF_NO_RECOVERY | SF_NO_RETRY | SF_QUIET_IR, NULL);
		if ((csio.ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(csio.ccb_h.path, /*relsim_flags*/0,
			    /*reduction*/0, /*timeout*/0, /*getcount_only*/0);
		if (error != 0)
			xpt_print(periph->path, "Synchronize cache failed\n");
	}
	cam_periph_unlock(periph);
	return (error);
}

static int
dagetattr(struct bio *bp)
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

/*
 * Callback from GEOM, called when it has finished cleaning up its
 * resources.
 */
static void
dadiskgonecb(struct disk *dp)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)dp->d_drv1;
	cam_periph_release(periph);
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
#ifdef CAM_IO_STATS
	softc->invalidations++;
#endif

	/*
	 * Return all queued I/O with ENXIO.
	 * XXX Handle any transactions queued to the card
	 *     with XPT_ABORT_CCB.
	 */
	cam_iosched_flush(softc->cam_iosched, NULL, ENXIO);

	/*
	 * Tell GEOM that we've gone away, we'll get a callback when it is
	 * done cleaning up its resources.
	 */
	disk_gone(softc->disk);
}

static void
dacleanup(struct cam_periph *periph)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	cam_periph_unlock(periph);

	cam_iosched_fini(softc->cam_iosched);

	/*
	 * If we can't free the sysctl tree, oh well...
	 */
	if ((softc->flags & DA_FLAG_SCTX_INIT) != 0) {
#ifdef CAM_IO_STATS
		if (sysctl_ctx_free(&softc->sysctl_stats_ctx) != 0)
			xpt_print(periph->path,
			    "can't remove sysctl stats context\n");
#endif
		if (sysctl_ctx_free(&softc->sysctl_ctx) != 0)
			xpt_print(periph->path,
			    "can't remove sysctl context\n");
	}

	callout_drain(&softc->mediapoll_c);
	disk_destroy(softc->disk);
	callout_drain(&softc->sendordered_c);
	free(softc, M_DEVBUF);
	cam_periph_lock(periph);
}

static void
daasync(void *callback_arg, u_int32_t code,
	struct cam_path *path, void *arg)
{
	struct cam_periph *periph;
	struct da_softc *softc;

	periph = (struct cam_periph *)callback_arg;
	switch (code) {
	case AC_FOUND_DEVICE:
	{
		struct ccb_getdev *cgd;
		cam_status status;
 
		cgd = (struct ccb_getdev *)arg;
		if (cgd == NULL)
			break;

		if (cgd->protocol != PROTO_SCSI)
			break;
		if (SID_QUAL(&cgd->inq_data) != SID_QUAL_LU_CONNECTED)
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
					  path, daasync,
					  AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG)
			printf("daasync: Unable to attach to new device "
				"due to status 0x%x\n", status);
		return;
	}
	case AC_ADVINFO_CHANGED:
	{
		uintptr_t buftype;

		buftype = (uintptr_t)arg;
		if (buftype == CDAI_TYPE_PHYS_PATH) {
			struct da_softc *softc;

			softc = periph->softc;
			disk_attr_changed(softc->disk, "GEOM::physpath",
					  M_NOWAIT);
		}
		break;
	}
	case AC_UNIT_ATTENTION:
	{
		union ccb *ccb;
		int error_code, sense_key, asc, ascq;

		softc = (struct da_softc *)periph->softc;
		ccb = (union ccb *)arg;

		/*
		 * Handle all UNIT ATTENTIONs except our own,
		 * as they will be handled by daerror().
		 */
		if (xpt_path_periph(ccb->ccb_h.path) != periph &&
		    scsi_extract_sense_ccb(ccb,
		     &error_code, &sense_key, &asc, &ascq)) {
			if (asc == 0x2A && ascq == 0x09) {
				xpt_print(ccb->ccb_h.path,
				    "Capacity data has changed\n");
				softc->flags &= ~DA_FLAG_PROBED;
				dareprobe(periph);
			} else if (asc == 0x28 && ascq == 0x00) {
				softc->flags &= ~DA_FLAG_PROBED;
				disk_media_changed(softc->disk, M_NOWAIT);
			} else if (asc == 0x3F && ascq == 0x03) {
				xpt_print(ccb->ccb_h.path,
				    "INQUIRY data has changed\n");
				softc->flags &= ~DA_FLAG_PROBED;
				dareprobe(periph);
			}
		}
		cam_periph_async(periph, code, path, arg);
		break;
	}
	case AC_SCSI_AEN:
		softc = (struct da_softc *)periph->softc;
		if (!cam_iosched_has_work_flags(softc->cam_iosched, DA_WORK_TUR)) {
			if (cam_periph_acquire(periph) == CAM_REQ_CMP) {
				cam_iosched_set_work_flags(softc->cam_iosched, DA_WORK_TUR);
				daschedule(periph);
			}
		}
		/* FALLTHROUGH */
	case AC_SENT_BDR:
	case AC_BUS_RESET:
	{
		struct ccb_hdr *ccbh;

		softc = (struct da_softc *)periph->softc;
		/*
		 * Don't fail on the expected unit attention
		 * that will occur.
		 */
		softc->flags |= DA_FLAG_RETRY_UA;
		LIST_FOREACH(ccbh, &softc->pending_ccbs, periph_links.le)
			ccbh->ccb_state |= DA_CCB_RETRY_UA;
		break;
	}
	default:
		break;
	}
	cam_periph_async(periph, code, path, arg);
}

static void
dasysctlinit(void *context, int pending)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	char tmpstr[80], tmpstr2[80];
	struct ccb_trans_settings cts;

	periph = (struct cam_periph *)context;
	/*
	 * periph was held for us when this task was enqueued
	 */
	if (periph->flags & CAM_PERIPH_INVALID) {
		cam_periph_release(periph);
		return;
	}

	softc = (struct da_softc *)periph->softc;
	snprintf(tmpstr, sizeof(tmpstr), "CAM DA unit %d", periph->unit_number);
	snprintf(tmpstr2, sizeof(tmpstr2), "%d", periph->unit_number);

	sysctl_ctx_init(&softc->sysctl_ctx);
	softc->flags |= DA_FLAG_SCTX_INIT;
	softc->sysctl_tree = SYSCTL_ADD_NODE(&softc->sysctl_ctx,
		SYSCTL_STATIC_CHILDREN(_kern_cam_da), OID_AUTO, tmpstr2,
		CTLFLAG_RD, 0, tmpstr);
	if (softc->sysctl_tree == NULL) {
		printf("dasysctlinit: unable to allocate sysctl tree\n");
		cam_periph_release(periph);
		return;
	}

	/*
	 * Now register the sysctl handler, so the user can change the value on
	 * the fly.
	 */
	SYSCTL_ADD_PROC(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "delete_method", CTLTYPE_STRING | CTLFLAG_RWTUN,
		softc, 0, dadeletemethodsysctl, "A",
		"BIO_DELETE execution method");
	SYSCTL_ADD_PROC(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "delete_max", CTLTYPE_U64 | CTLFLAG_RW,
		softc, 0, dadeletemaxsysctl, "Q",
		"Maximum BIO_DELETE size");
	SYSCTL_ADD_PROC(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "minimum_cmd_size", CTLTYPE_INT | CTLFLAG_RW,
		&softc->minimum_cmd_size, 0, dacmdsizesysctl, "I",
		"Minimum CDB size");

	SYSCTL_ADD_INT(&softc->sysctl_ctx,
		       SYSCTL_CHILDREN(softc->sysctl_tree),
		       OID_AUTO,
		       "error_inject",
		       CTLFLAG_RW,
		       &softc->error_inject,
		       0,
		       "error_inject leaf");

	SYSCTL_ADD_INT(&softc->sysctl_ctx,
		       SYSCTL_CHILDREN(softc->sysctl_tree),
		       OID_AUTO,
		       "unmapped_io",
		       CTLFLAG_RD, 
		       &softc->unmappedio,
		       0,
		       "Unmapped I/O leaf");

	SYSCTL_ADD_INT(&softc->sysctl_ctx,
		       SYSCTL_CHILDREN(softc->sysctl_tree),
		       OID_AUTO,
		       "rotating",
		       CTLFLAG_RD, 
		       &softc->rotating,
		       0,
		       "Rotating media");

	/*
	 * Add some addressing info.
	 */
	memset(&cts, 0, sizeof (cts));
	xpt_setup_ccb(&cts.ccb_h, periph->path, CAM_PRIORITY_NONE);
	cts.ccb_h.func_code = XPT_GET_TRAN_SETTINGS;
	cts.type = CTS_TYPE_CURRENT_SETTINGS;
	cam_periph_lock(periph);
	xpt_action((union ccb *)&cts);
	cam_periph_unlock(periph);
	if (cts.ccb_h.status != CAM_REQ_CMP) {
		cam_periph_release(periph);
		return;
	}
	if (cts.protocol == PROTO_SCSI && cts.transport == XPORT_FC) {
		struct ccb_trans_settings_fc *fc = &cts.xport_specific.fc;
		if (fc->valid & CTS_FC_VALID_WWPN) {
			softc->wwpn = fc->wwpn;
			SYSCTL_ADD_UQUAD(&softc->sysctl_ctx,
			    SYSCTL_CHILDREN(softc->sysctl_tree),
			    OID_AUTO, "wwpn", CTLFLAG_RD,
			    &softc->wwpn, "World Wide Port Name");
		}
	}

#ifdef CAM_IO_STATS
	/*
	 * Now add some useful stats.
	 * XXX These should live in cam_periph and be common to all periphs
	 */
	softc->sysctl_stats_tree = SYSCTL_ADD_NODE(&softc->sysctl_stats_ctx,
	    SYSCTL_CHILDREN(softc->sysctl_tree), OID_AUTO, "stats",
	    CTLFLAG_RD, 0, "Statistics");
	SYSCTL_ADD_INT(&softc->sysctl_stats_ctx,
		       SYSCTL_CHILDREN(softc->sysctl_stats_tree),
		       OID_AUTO,
		       "errors",
		       CTLFLAG_RD,
		       &softc->errors,
		       0,
		       "Transport errors reported by the SIM");
	SYSCTL_ADD_INT(&softc->sysctl_stats_ctx,
		       SYSCTL_CHILDREN(softc->sysctl_stats_tree),
		       OID_AUTO,
		       "timeouts",
		       CTLFLAG_RD,
		       &softc->timeouts,
		       0,
		       "Device timeouts reported by the SIM");
	SYSCTL_ADD_INT(&softc->sysctl_stats_ctx,
		       SYSCTL_CHILDREN(softc->sysctl_stats_tree),
		       OID_AUTO,
		       "pack_invalidations",
		       CTLFLAG_RD,
		       &softc->invalidations,
		       0,
		       "Device pack invalidations");
#endif

	cam_iosched_sysctl_init(softc->cam_iosched, &softc->sysctl_ctx,
	    softc->sysctl_tree);

	cam_periph_release(periph);
}

static int
dadeletemaxsysctl(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint64_t value;
	struct da_softc *softc;

	softc = (struct da_softc *)arg1;

	value = softc->disk->d_delmaxsize;
	error = sysctl_handle_64(oidp, &value, 0, req);
	if ((error != 0) || (req->newptr == NULL))
		return (error);

	/* only accept values smaller than the calculated value */
	if (value > dadeletemaxsize(softc, softc->delete_method)) {
		return (EINVAL);
	}
	softc->disk->d_delmaxsize = value;

	return (0);
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

static int
dasysctlsofttimeout(SYSCTL_HANDLER_ARGS)
{
	sbintime_t value;
	int error;

	value = da_default_softtimeout / SBT_1MS;

	error = sysctl_handle_int(oidp, (int *)&value, 0, req);
	if ((error != 0) || (req->newptr == NULL))
		return (error);

	/* XXX Should clip this to a reasonable level */
	if (value > da_default_timeout * 1000)
		return (EINVAL);

	da_default_softtimeout = value * SBT_1MS;
	return (0);
}

static void
dadeletemethodset(struct da_softc *softc, da_delete_methods delete_method)
{

	softc->delete_method = delete_method;
	softc->disk->d_delmaxsize = dadeletemaxsize(softc, delete_method);
	softc->delete_func = da_delete_functions[delete_method];

	if (softc->delete_method > DA_DELETE_DISABLE)
		softc->disk->d_flags |= DISKFLAG_CANDELETE;
	else
		softc->disk->d_flags &= ~DISKFLAG_CANDELETE;
}

static off_t
dadeletemaxsize(struct da_softc *softc, da_delete_methods delete_method)
{
	off_t sectors;

	switch(delete_method) {
	case DA_DELETE_UNMAP:
		sectors = (off_t)softc->unmap_max_lba;
		break;
	case DA_DELETE_ATA_TRIM:
		sectors = (off_t)ATA_DSM_RANGE_MAX * softc->trim_max_ranges;
		break;
	case DA_DELETE_WS16:
		sectors = omin(softc->ws_max_blks, WS16_MAX_BLKS);
		break;
	case DA_DELETE_ZERO:
	case DA_DELETE_WS10:
		sectors = omin(softc->ws_max_blks, WS10_MAX_BLKS);
		break;
	default:
		return 0;
	}

	return (off_t)softc->params.secsize *
	    omin(sectors, softc->params.sectors);
}

static void
daprobedone(struct cam_periph *periph, union ccb *ccb)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	dadeletemethodchoose(softc, DA_DELETE_NONE);

	if (bootverbose && (softc->flags & DA_FLAG_ANNOUNCED) == 0) {
		char buf[80];
		int i, sep;

		snprintf(buf, sizeof(buf), "Delete methods: <");
		sep = 0;
		for (i = 0; i <= DA_DELETE_MAX; i++) {
			if ((softc->delete_available & (1 << i)) == 0 &&
			    i != softc->delete_method)
				continue;
			if (sep)
				strlcat(buf, ",", sizeof(buf));
			strlcat(buf, da_delete_method_names[i],
			    sizeof(buf));
			if (i == softc->delete_method)
				strlcat(buf, "(*)", sizeof(buf));
			sep = 1;
		}
		strlcat(buf, ">", sizeof(buf));
		printf("%s%d: %s\n", periph->periph_name,
		    periph->unit_number, buf);
	}

	/*
	 * Since our peripheral may be invalidated by an error
	 * above or an external event, we must release our CCB
	 * before releasing the probe lock on the peripheral.
	 * The peripheral will only go away once the last lock
	 * is removed, and we need it around for the CCB release
	 * operation.
	 */
	xpt_release_ccb(ccb);
	softc->state = DA_STATE_NORMAL;
	softc->flags |= DA_FLAG_PROBED;
	daschedule(periph);
	wakeup(&softc->disk->d_mediasize);
	if ((softc->flags & DA_FLAG_ANNOUNCED) == 0) {
		softc->flags |= DA_FLAG_ANNOUNCED;
		cam_periph_unhold(periph);
	} else
		cam_periph_release_locked(periph);
}

static void
dadeletemethodchoose(struct da_softc *softc, da_delete_methods default_method)
{
	int i, methods;

	/* If available, prefer the method requested by user. */
	i = softc->delete_method_pref;
	methods = softc->delete_available | (1 << DA_DELETE_DISABLE);
	if (methods & (1 << i)) {
		dadeletemethodset(softc, i);
		return;
	}

	/* Use the pre-defined order to choose the best performing delete. */
	for (i = DA_DELETE_MIN; i <= DA_DELETE_MAX; i++) {
		if (i == DA_DELETE_ZERO)
			continue;
		if (softc->delete_available & (1 << i)) {
			dadeletemethodset(softc, i);
			return;
		}
	}

	/* Fallback to default. */
	dadeletemethodset(softc, default_method);
}

static int
dadeletemethodsysctl(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	const char *p;
	struct da_softc *softc;
	int i, error, methods, value;

	softc = (struct da_softc *)arg1;

	value = softc->delete_method;
	if (value < 0 || value > DA_DELETE_MAX)
		p = "UNKNOWN";
	else
		p = da_delete_method_names[value];
	strncpy(buf, p, sizeof(buf));
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	methods = softc->delete_available | (1 << DA_DELETE_DISABLE);
	for (i = 0; i <= DA_DELETE_MAX; i++) {
		if (strcmp(buf, da_delete_method_names[i]) == 0)
			break;
	}
	if (i > DA_DELETE_MAX)
		return (EINVAL);
	softc->delete_method_pref = i;
	dadeletemethodchoose(softc, DA_DELETE_NONE);
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

	if (cam_iosched_init(&softc->cam_iosched, periph) != 0) {
		printf("daregister: Unable to probe new device. "
		       "Unable to allocate iosched memory\n");
		return(CAM_REQ_CMP_ERR);
	}
	
	LIST_INIT(&softc->pending_ccbs);
	softc->state = DA_STATE_PROBE_RC;
	bioq_init(&softc->delete_run_queue);
	if (SID_IS_REMOVABLE(&cgd->inq_data))
		softc->flags |= DA_FLAG_PACK_REMOVABLE;
	softc->unmap_max_ranges = UNMAP_MAX_RANGES;
	softc->unmap_max_lba = UNMAP_RANGE_MAX;
	softc->ws_max_blks = WS16_MAX_BLKS;
	softc->trim_max_ranges = ATA_TRIM_MAX_RANGES;
	softc->rotating = 1;

	periph->softc = softc;

	/*
	 * See if this device has any quirks.
	 */
	match = cam_quirkmatch((caddr_t)&cgd->inq_data,
			       (caddr_t)da_quirk_table,
			       nitems(da_quirk_table),
			       sizeof(*da_quirk_table), scsi_inquiry_match);

	if (match != NULL)
		softc->quirks = ((struct da_quirk_entry *)match)->quirks;
	else
		softc->quirks = DA_Q_NONE;

	/* Check if the SIM does not want 6 byte commands */
	bzero(&cpi, sizeof(cpi));
	xpt_setup_ccb(&cpi.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);
	if (cpi.ccb_h.status == CAM_REQ_CMP && (cpi.hba_misc & PIM_NO_6_BYTE))
		softc->quirks |= DA_Q_NO_6_BYTE;

	TASK_INIT(&softc->sysctl_task, 0, dasysctlinit, periph);

	/*
	 * Take an exclusive refcount on the periph while dastart is called
	 * to finish the probe.  The reference will be dropped in dadone at
	 * the end of probe.
	 */
	(void)cam_periph_hold(periph, PRIBIO);

	/*
	 * Schedule a periodic event to occasionally send an
	 * ordered tag to a device.
	 */
	callout_init_mtx(&softc->sendordered_c, cam_periph_mtx(periph), 0);
	callout_reset(&softc->sendordered_c,
	    (da_default_timeout * hz) / DA_ORDEREDTAG_INTERVAL,
	    dasendorderedtag, softc);

	cam_periph_unlock(periph);
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

	/* Predict whether device may support READ CAPACITY(16). */
	if (SID_ANSI_REV(&cgd->inq_data) >= SCSI_REV_SPC3 &&
	    (softc->quirks & DA_Q_NO_RC16) == 0) {
		softc->flags |= DA_FLAG_CAN_RC16;
		softc->state = DA_STATE_PROBE_RC16;
	}

	/*
	 * Register this media as a disk.
	 */
	softc->disk = disk_alloc();
	softc->disk->d_devstat = devstat_new_entry(periph->periph_name,
			  periph->unit_number, 0,
			  DEVSTAT_BS_UNAVAILABLE,
			  SID_TYPE(&cgd->inq_data) |
			  XPORT_DEVSTAT_TYPE(cpi.transport),
			  DEVSTAT_PRIORITY_DISK);
	softc->disk->d_open = daopen;
	softc->disk->d_close = daclose;
	softc->disk->d_strategy = dastrategy;
	softc->disk->d_dump = dadump;
	softc->disk->d_getattr = dagetattr;
	softc->disk->d_gone = dadiskgonecb;
	softc->disk->d_name = "da";
	softc->disk->d_drv1 = periph;
	if (cpi.maxio == 0)
		softc->maxio = DFLTPHYS;	/* traditional default */
	else if (cpi.maxio > MAXPHYS)
		softc->maxio = MAXPHYS;		/* for safety */
	else
		softc->maxio = cpi.maxio;
	softc->disk->d_maxsize = softc->maxio;
	softc->disk->d_unit = periph->unit_number;
	softc->disk->d_flags = DISKFLAG_DIRECT_COMPLETION;
	if ((softc->quirks & DA_Q_NO_SYNC_CACHE) == 0)
		softc->disk->d_flags |= DISKFLAG_CANFLUSHCACHE;
	if ((cpi.hba_misc & PIM_UNMAPPED) != 0) {
		softc->unmappedio = 1;
		softc->disk->d_flags |= DISKFLAG_UNMAPPED_BIO;
		xpt_print(periph->path, "UNMAPPED\n");
	}
	cam_strvis(softc->disk->d_descr, cgd->inq_data.vendor,
	    sizeof(cgd->inq_data.vendor), sizeof(softc->disk->d_descr));
	strlcat(softc->disk->d_descr, " ", sizeof(softc->disk->d_descr));
	cam_strvis(&softc->disk->d_descr[strlen(softc->disk->d_descr)],
	    cgd->inq_data.product, sizeof(cgd->inq_data.product),
	    sizeof(softc->disk->d_descr) - strlen(softc->disk->d_descr));
	softc->disk->d_hba_vendor = cpi.hba_vendor;
	softc->disk->d_hba_device = cpi.hba_device;
	softc->disk->d_hba_subvendor = cpi.hba_subvendor;
	softc->disk->d_hba_subdevice = cpi.hba_subdevice;

	/*
	 * Acquire a reference to the periph before we register with GEOM.
	 * We'll release this reference once GEOM calls us back (via
	 * dadiskgonecb()) telling us that our provider has been freed.
	 */
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: lost periph during "
			  "registration!\n", __func__);
		cam_periph_lock(periph);
		return (CAM_REQ_CMP_ERR);
	}

	disk_create(softc->disk, DISK_VERSION);
	cam_periph_lock(periph);

	/*
	 * Add async callbacks for events of interest.
	 * I don't bother checking if this fails as,
	 * in most cases, the system will function just
	 * fine without them and the only alternative
	 * would be to not attach the device on failure.
	 */
	xpt_register_async(AC_SENT_BDR | AC_BUS_RESET | AC_LOST_DEVICE |
	    AC_ADVINFO_CHANGED | AC_SCSI_AEN | AC_UNIT_ATTENTION,
	    daasync, periph, periph->path);

	/*
	 * Emit an attribute changed notification just in case 
	 * physical path information arrived before our async
	 * event handler was registered, but after anyone attaching
	 * to our disk device polled it.
	 */
	disk_attr_changed(softc->disk, "GEOM::physpath", M_NOWAIT);

	/*
	 * Schedule a periodic media polling events.
	 */
	callout_init_mtx(&softc->mediapoll_c, cam_periph_mtx(periph), 0);
	if ((softc->flags & DA_FLAG_PACK_REMOVABLE) &&
	    (cgd->inq_flags & SID_AEN) == 0 &&
	    da_poll_period != 0)
		callout_reset(&softc->mediapoll_c, da_poll_period * hz,
		    damediapoll, periph);

	xpt_schedule(periph, CAM_PRIORITY_DEV);

	return(CAM_REQ_CMP);
}

static void
dastart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("dastart\n"));

skipstate:
	switch (softc->state) {
	case DA_STATE_NORMAL:
	{
		struct bio *bp;
		uint8_t tag_code;

more:
		bp = cam_iosched_next_bio(softc->cam_iosched);
		if (bp == NULL) {
			if (cam_iosched_has_work_flags(softc->cam_iosched, DA_WORK_TUR)) {
				cam_iosched_clr_work_flags(softc->cam_iosched, DA_WORK_TUR);
				scsi_test_unit_ready(&start_ccb->csio,
				     /*retries*/ da_retry_count,
				     dadone,
				     MSG_SIMPLE_Q_TAG,
				     SSD_FULL_SIZE,
				     da_default_timeout * 1000);
				start_ccb->ccb_h.ccb_bp = NULL;
				start_ccb->ccb_h.ccb_state = DA_CCB_TUR;
				xpt_action(start_ccb);
			} else
				xpt_release_ccb(start_ccb);
			break;
		}

		if (bp->bio_cmd == BIO_DELETE) {
			if (softc->delete_func != NULL) {
				softc->delete_func(periph, start_ccb, bp);
				goto out;
			} else {
				/* Not sure this is possible, but failsafe by lying and saying "sure, done." */
				biofinish(bp, NULL, 0);
				goto more;
			}
		}

		if (cam_iosched_has_work_flags(softc->cam_iosched, DA_WORK_TUR)) {
			cam_iosched_clr_work_flags(softc->cam_iosched, DA_WORK_TUR);
			cam_periph_release_locked(periph);	/* XXX is this still valid? I think so but unverified */
		}

		if ((bp->bio_flags & BIO_ORDERED) != 0 ||
		    (softc->flags & DA_FLAG_NEED_OTAG) != 0) {
			softc->flags &= ~DA_FLAG_NEED_OTAG;
			softc->flags |= DA_FLAG_WAS_OTAG;
			tag_code = MSG_ORDERED_Q_TAG;
		} else {
			tag_code = MSG_SIMPLE_Q_TAG;
		}

		switch (bp->bio_cmd) {
		case BIO_WRITE:
		case BIO_READ:
		{
			void *data_ptr;
			int rw_op;

			if (bp->bio_cmd == BIO_WRITE) {
				softc->flags |= DA_FLAG_DIRTY;
				rw_op = SCSI_RW_WRITE;
			} else {
				rw_op = SCSI_RW_READ;
			}

			data_ptr = bp->bio_data;
			if ((bp->bio_flags & (BIO_UNMAPPED|BIO_VLIST)) != 0) {
				rw_op |= SCSI_RW_BIO;
				data_ptr = bp;
			}

			scsi_read_write(&start_ccb->csio,
					/*retries*/da_retry_count,
					/*cbfcnp*/dadone,
					/*tag_action*/tag_code,
					rw_op,
					/*byte2*/0,
					softc->minimum_cmd_size,
					/*lba*/bp->bio_pblkno,
					/*block_count*/bp->bio_bcount /
					softc->params.secsize,
					data_ptr,
					/*dxfer_len*/ bp->bio_bcount,
					/*sense_len*/SSD_FULL_SIZE,
					da_default_timeout * 1000);
			break;
		}
		case BIO_FLUSH:
			/*
			 * BIO_FLUSH doesn't currently communicate
			 * range data, so we synchronize the cache
			 * over the whole disk.  We also force
			 * ordered tag semantics the flush applies
			 * to all previously queued I/O.
			 */
			scsi_synchronize_cache(&start_ccb->csio,
					       /*retries*/1,
					       /*cbfcnp*/dadone,
					       MSG_ORDERED_Q_TAG,
					       /*begin_lba*/0,
					       /*lb_count*/0,
					       SSD_FULL_SIZE,
					       da_default_timeout*1000);
			break;
		}
		start_ccb->ccb_h.ccb_state = DA_CCB_BUFFER_IO;
		start_ccb->ccb_h.flags |= CAM_UNLOCKED;
		start_ccb->ccb_h.softtimeout = sbttotv(da_default_softtimeout);

out:
		LIST_INSERT_HEAD(&softc->pending_ccbs,
				 &start_ccb->ccb_h, periph_links.le);

		/* We expect a unit attention from this device */
		if ((softc->flags & DA_FLAG_RETRY_UA) != 0) {
			start_ccb->ccb_h.ccb_state |= DA_CCB_RETRY_UA;
			softc->flags &= ~DA_FLAG_RETRY_UA;
		}

		start_ccb->ccb_h.ccb_bp = bp;
		softc->refcount++;
		cam_periph_unlock(periph);
		xpt_action(start_ccb);
		cam_periph_lock(periph);
		softc->refcount--;

		/* May have more work to do, so ensure we stay scheduled */
		daschedule(periph);
		break;
	}
	case DA_STATE_PROBE_RC:
	{
		struct scsi_read_capacity_data *rcap;

		rcap = (struct scsi_read_capacity_data *)
		    malloc(sizeof(*rcap), M_SCSIDA, M_NOWAIT|M_ZERO);
		if (rcap == NULL) {
			printf("dastart: Couldn't malloc read_capacity data\n");
			/* da_free_periph??? */
			break;
		}
		scsi_read_capacity(&start_ccb->csio,
				   /*retries*/da_retry_count,
				   dadone,
				   MSG_SIMPLE_Q_TAG,
				   rcap,
				   SSD_FULL_SIZE,
				   /*timeout*/5000);
		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = DA_CCB_PROBE_RC;
		xpt_action(start_ccb);
		break;
	}
	case DA_STATE_PROBE_RC16:
	{
		struct scsi_read_capacity_data_long *rcaplong;

		rcaplong = (struct scsi_read_capacity_data_long *)
			malloc(sizeof(*rcaplong), M_SCSIDA, M_NOWAIT|M_ZERO);
		if (rcaplong == NULL) {
			printf("dastart: Couldn't malloc read_capacity data\n");
			/* da_free_periph??? */
			break;
		}
		scsi_read_capacity_16(&start_ccb->csio,
				      /*retries*/ da_retry_count,
				      /*cbfcnp*/ dadone,
				      /*tag_action*/ MSG_SIMPLE_Q_TAG,
				      /*lba*/ 0,
				      /*reladr*/ 0,
				      /*pmi*/ 0,
				      /*rcap_buf*/ (uint8_t *)rcaplong,
				      /*rcap_buf_len*/ sizeof(*rcaplong),
				      /*sense_len*/ SSD_FULL_SIZE,
				      /*timeout*/ da_default_timeout * 1000);
		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = DA_CCB_PROBE_RC16;
		xpt_action(start_ccb);
		break;
	}
	case DA_STATE_PROBE_LBP:
	{
		struct scsi_vpd_logical_block_prov *lbp;

		if (!scsi_vpd_supported_page(periph, SVPD_LBP)) {
			/*
			 * If we get here we don't support any SBC-3 delete
			 * methods with UNMAP as the Logical Block Provisioning
			 * VPD page support is required for devices which
			 * support it according to T10/1799-D Revision 31
			 * however older revisions of the spec don't mandate
			 * this so we currently don't remove these methods
			 * from the available set.
			 */
			softc->state = DA_STATE_PROBE_BLK_LIMITS;
			goto skipstate;
		}

		lbp = (struct scsi_vpd_logical_block_prov *)
			malloc(sizeof(*lbp), M_SCSIDA, M_NOWAIT|M_ZERO);

		if (lbp == NULL) {
			printf("dastart: Couldn't malloc lbp data\n");
			/* da_free_periph??? */
			break;
		}

		scsi_inquiry(&start_ccb->csio,
			     /*retries*/da_retry_count,
			     /*cbfcnp*/dadone,
			     /*tag_action*/MSG_SIMPLE_Q_TAG,
			     /*inq_buf*/(u_int8_t *)lbp,
			     /*inq_len*/sizeof(*lbp),
			     /*evpd*/TRUE,
			     /*page_code*/SVPD_LBP,
			     /*sense_len*/SSD_MIN_SIZE,
			     /*timeout*/da_default_timeout * 1000);
		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = DA_CCB_PROBE_LBP;
		xpt_action(start_ccb);
		break;
	}
	case DA_STATE_PROBE_BLK_LIMITS:
	{
		struct scsi_vpd_block_limits *block_limits;

		if (!scsi_vpd_supported_page(periph, SVPD_BLOCK_LIMITS)) {
			/* Not supported skip to next probe */
			softc->state = DA_STATE_PROBE_BDC;
			goto skipstate;
		}

		block_limits = (struct scsi_vpd_block_limits *)
			malloc(sizeof(*block_limits), M_SCSIDA, M_NOWAIT|M_ZERO);

		if (block_limits == NULL) {
			printf("dastart: Couldn't malloc block_limits data\n");
			/* da_free_periph??? */
			break;
		}

		scsi_inquiry(&start_ccb->csio,
			     /*retries*/da_retry_count,
			     /*cbfcnp*/dadone,
			     /*tag_action*/MSG_SIMPLE_Q_TAG,
			     /*inq_buf*/(u_int8_t *)block_limits,
			     /*inq_len*/sizeof(*block_limits),
			     /*evpd*/TRUE,
			     /*page_code*/SVPD_BLOCK_LIMITS,
			     /*sense_len*/SSD_MIN_SIZE,
			     /*timeout*/da_default_timeout * 1000);
		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = DA_CCB_PROBE_BLK_LIMITS;
		xpt_action(start_ccb);
		break;
	}
	case DA_STATE_PROBE_BDC:
	{
		struct scsi_vpd_block_characteristics *bdc;

		if (!scsi_vpd_supported_page(periph, SVPD_BDC)) {
			softc->state = DA_STATE_PROBE_ATA;
			goto skipstate;
		}

		bdc = (struct scsi_vpd_block_characteristics *)
			malloc(sizeof(*bdc), M_SCSIDA, M_NOWAIT|M_ZERO);

		if (bdc == NULL) {
			printf("dastart: Couldn't malloc bdc data\n");
			/* da_free_periph??? */
			break;
		}

		scsi_inquiry(&start_ccb->csio,
			     /*retries*/da_retry_count,
			     /*cbfcnp*/dadone,
			     /*tag_action*/MSG_SIMPLE_Q_TAG,
			     /*inq_buf*/(u_int8_t *)bdc,
			     /*inq_len*/sizeof(*bdc),
			     /*evpd*/TRUE,
			     /*page_code*/SVPD_BDC,
			     /*sense_len*/SSD_MIN_SIZE,
			     /*timeout*/da_default_timeout * 1000);
		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = DA_CCB_PROBE_BDC;
		xpt_action(start_ccb);
		break;
	}
	case DA_STATE_PROBE_ATA:
	{
		struct ata_params *ata_params;

		if (!scsi_vpd_supported_page(periph, SVPD_ATA_INFORMATION)) {
			daprobedone(periph, start_ccb);
			break;
		}

		ata_params = (struct ata_params*)
			malloc(sizeof(*ata_params), M_SCSIDA, M_NOWAIT|M_ZERO);

		if (ata_params == NULL) {
			printf("dastart: Couldn't malloc ata_params data\n");
			/* da_free_periph??? */
			break;
		}

		scsi_ata_identify(&start_ccb->csio,
				  /*retries*/da_retry_count,
				  /*cbfcnp*/dadone,
                                  /*tag_action*/MSG_SIMPLE_Q_TAG,
				  /*data_ptr*/(u_int8_t *)ata_params,
				  /*dxfer_len*/sizeof(*ata_params),
				  /*sense_len*/SSD_FULL_SIZE,
				  /*timeout*/da_default_timeout * 1000);
		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = DA_CCB_PROBE_ATA;
		xpt_action(start_ccb);
		break;
	}
	}
}

/*
 * In each of the methods below, while its the caller's
 * responsibility to ensure the request will fit into a
 * single device request, we might have changed the delete
 * method due to the device incorrectly advertising either
 * its supported methods or limits.
 * 
 * To prevent this causing further issues we validate the
 * against the methods limits, and warn which would
 * otherwise be unnecessary.
 */
static void
da_delete_unmap(struct cam_periph *periph, union ccb *ccb, struct bio *bp)
{
	struct da_softc *softc = (struct da_softc *)periph->softc;;
	struct bio *bp1;
	uint8_t *buf = softc->unmap_buf;
	uint64_t lba, lastlba = (uint64_t)-1;
	uint64_t totalcount = 0;
	uint64_t count;
	uint32_t lastcount = 0, c;
	uint32_t off, ranges = 0;

	/*
	 * Currently this doesn't take the UNMAP
	 * Granularity and Granularity Alignment
	 * fields into account.
	 *
	 * This could result in both unoptimal unmap
	 * requests as as well as UNMAP calls unmapping
	 * fewer LBA's than requested.
	 */

	bzero(softc->unmap_buf, sizeof(softc->unmap_buf));
	bp1 = bp;
	do {
		/*
		 * Note: ada and da are different in how they store the
		 * pending bp's in a trim. ada stores all of them in the
		 * trim_req.bps. da stores all but the first one in the
		 * delete_run_queue. ada then completes all the bps in
		 * its adadone() loop. da completes all the bps in the
		 * delete_run_queue in dadone, and relies on the biodone
		 * after to complete. This should be reconciled since there's
		 * no real reason to do it differently. XXX
		 */
		if (bp1 != bp)
			bioq_insert_tail(&softc->delete_run_queue, bp1);
		lba = bp1->bio_pblkno;
		count = bp1->bio_bcount / softc->params.secsize;

		/* Try to extend the previous range. */
		if (lba == lastlba) {
			c = omin(count, UNMAP_RANGE_MAX - lastcount);
			lastcount += c;
			off = ((ranges - 1) * UNMAP_RANGE_SIZE) +
			      UNMAP_HEAD_SIZE;
			scsi_ulto4b(lastcount, &buf[off + 8]);
			count -= c;
			lba +=c;
			totalcount += c;
		}

		while (count > 0) {
			c = omin(count, UNMAP_RANGE_MAX);
			if (totalcount + c > softc->unmap_max_lba ||
			    ranges >= softc->unmap_max_ranges) {
				xpt_print(periph->path,
				    "%s issuing short delete %ld > %ld"
				    "|| %d >= %d",
				    da_delete_method_desc[softc->delete_method],
				    totalcount + c, softc->unmap_max_lba,
				    ranges, softc->unmap_max_ranges);
				break;
			}
			off = (ranges * UNMAP_RANGE_SIZE) + UNMAP_HEAD_SIZE;
			scsi_u64to8b(lba, &buf[off + 0]);
			scsi_ulto4b(c, &buf[off + 8]);
			lba += c;
			totalcount += c;
			ranges++;
			count -= c;
			lastcount = c;
		}
		lastlba = lba;
		bp1 = cam_iosched_next_trim(softc->cam_iosched);
		if (bp1 == NULL)
			break;
		if (ranges >= softc->unmap_max_ranges ||
		    totalcount + bp1->bio_bcount /
		    softc->params.secsize > softc->unmap_max_lba) {
			cam_iosched_put_back_trim(softc->cam_iosched, bp1);
			break;
		}
	} while (1);
	scsi_ulto2b(ranges * 16 + 6, &buf[0]);
	scsi_ulto2b(ranges * 16, &buf[2]);

	scsi_unmap(&ccb->csio,
		   /*retries*/da_retry_count,
		   /*cbfcnp*/dadone,
		   /*tag_action*/MSG_SIMPLE_Q_TAG,
		   /*byte2*/0,
		   /*data_ptr*/ buf,
		   /*dxfer_len*/ ranges * 16 + 8,
		   /*sense_len*/SSD_FULL_SIZE,
		   da_default_timeout * 1000);
	ccb->ccb_h.ccb_state = DA_CCB_DELETE;
	ccb->ccb_h.flags |= CAM_UNLOCKED;
	cam_iosched_submit_trim(softc->cam_iosched);
}

static void
da_delete_trim(struct cam_periph *periph, union ccb *ccb, struct bio *bp)
{
	struct da_softc *softc = (struct da_softc *)periph->softc;
	struct bio *bp1;
	uint8_t *buf = softc->unmap_buf;
	uint64_t lastlba = (uint64_t)-1;
	uint64_t count;
	uint64_t lba;
	uint32_t lastcount = 0, c, requestcount;
	int ranges = 0, off, block_count;

	bzero(softc->unmap_buf, sizeof(softc->unmap_buf));
	bp1 = bp;
	do {
		if (bp1 != bp)//XXX imp XXX
			bioq_insert_tail(&softc->delete_run_queue, bp1);
		lba = bp1->bio_pblkno;
		count = bp1->bio_bcount / softc->params.secsize;
		requestcount = count;

		/* Try to extend the previous range. */
		if (lba == lastlba) {
			c = omin(count, ATA_DSM_RANGE_MAX - lastcount);
			lastcount += c;
			off = (ranges - 1) * 8;
			buf[off + 6] = lastcount & 0xff;
			buf[off + 7] = (lastcount >> 8) & 0xff;
			count -= c;
			lba += c;
		}

		while (count > 0) {
			c = omin(count, ATA_DSM_RANGE_MAX);
			off = ranges * 8;

			buf[off + 0] = lba & 0xff;
			buf[off + 1] = (lba >> 8) & 0xff;
			buf[off + 2] = (lba >> 16) & 0xff;
			buf[off + 3] = (lba >> 24) & 0xff;
			buf[off + 4] = (lba >> 32) & 0xff;
			buf[off + 5] = (lba >> 40) & 0xff;
			buf[off + 6] = c & 0xff;
			buf[off + 7] = (c >> 8) & 0xff;
			lba += c;
			ranges++;
			count -= c;
			lastcount = c;
			if (count != 0 && ranges == softc->trim_max_ranges) {
				xpt_print(periph->path,
				    "%s issuing short delete %ld > %ld\n",
				    da_delete_method_desc[softc->delete_method],
				    requestcount,
				    (softc->trim_max_ranges - ranges) *
				    ATA_DSM_RANGE_MAX);
				break;
			}
		}
		lastlba = lba;
		bp1 = cam_iosched_next_trim(softc->cam_iosched);
		if (bp1 == NULL)
			break;
		if (bp1->bio_bcount / softc->params.secsize >
		    (softc->trim_max_ranges - ranges) * ATA_DSM_RANGE_MAX) {
			cam_iosched_put_back_trim(softc->cam_iosched, bp1);
			break;
		}
	} while (1);

	block_count = (ranges + ATA_DSM_BLK_RANGES - 1) / ATA_DSM_BLK_RANGES;
	scsi_ata_trim(&ccb->csio,
		      /*retries*/da_retry_count,
		      /*cbfcnp*/dadone,
		      /*tag_action*/MSG_SIMPLE_Q_TAG,
		      block_count,
		      /*data_ptr*/buf,
		      /*dxfer_len*/block_count * ATA_DSM_BLK_SIZE,
		      /*sense_len*/SSD_FULL_SIZE,
		      da_default_timeout * 1000);
	ccb->ccb_h.ccb_state = DA_CCB_DELETE;
	ccb->ccb_h.flags |= CAM_UNLOCKED;
	cam_iosched_submit_trim(softc->cam_iosched);
}

/*
 * We calculate ws_max_blks here based off d_delmaxsize instead
 * of using softc->ws_max_blks as it is absolute max for the
 * device not the protocol max which may well be lower.
 */
static void
da_delete_ws(struct cam_periph *periph, union ccb *ccb, struct bio *bp)
{
	struct da_softc *softc;
	struct bio *bp1;
	uint64_t ws_max_blks;
	uint64_t lba;
	uint64_t count; /* forward compat with WS32 */

	softc = (struct da_softc *)periph->softc;
	ws_max_blks = softc->disk->d_delmaxsize / softc->params.secsize;
	lba = bp->bio_pblkno;
	count = 0;
	bp1 = bp;
	do {
		if (bp1 != bp)//XXX imp XXX
			bioq_insert_tail(&softc->delete_run_queue, bp1);
		count += bp1->bio_bcount / softc->params.secsize;
		if (count > ws_max_blks) {
			xpt_print(periph->path,
			    "%s issuing short delete %ld > %ld\n",
			    da_delete_method_desc[softc->delete_method],
			    count, ws_max_blks);
			count = omin(count, ws_max_blks);
			break;
		}
		bp1 = cam_iosched_next_trim(softc->cam_iosched);
		if (bp1 == NULL)
			break;
		if (lba + count != bp1->bio_pblkno ||
		    count + bp1->bio_bcount /
		    softc->params.secsize > ws_max_blks) {
			cam_iosched_put_back_trim(softc->cam_iosched, bp1);
			break;
		}
	} while (1);

	scsi_write_same(&ccb->csio,
			/*retries*/da_retry_count,
			/*cbfcnp*/dadone,
			/*tag_action*/MSG_SIMPLE_Q_TAG,
			/*byte2*/softc->delete_method ==
			    DA_DELETE_ZERO ? 0 : SWS_UNMAP,
			softc->delete_method == DA_DELETE_WS16 ? 16 : 10,
			/*lba*/lba,
			/*block_count*/count,
			/*data_ptr*/ __DECONST(void *, zero_region),
			/*dxfer_len*/ softc->params.secsize,
			/*sense_len*/SSD_FULL_SIZE,
			da_default_timeout * 1000);
	ccb->ccb_h.ccb_state = DA_CCB_DELETE;
	ccb->ccb_h.flags |= CAM_UNLOCKED;
	cam_iosched_submit_trim(softc->cam_iosched);
}

static int
cmd6workaround(union ccb *ccb)
{
	struct scsi_rw_6 cmd6;
	struct scsi_rw_10 *cmd10;
	struct da_softc *softc;
	u_int8_t *cdb;
	struct bio *bp;
	int frozen;

	cdb = ccb->csio.cdb_io.cdb_bytes;
	softc = (struct da_softc *)xpt_path_periph(ccb->ccb_h.path)->softc;

	if (ccb->ccb_h.ccb_state == DA_CCB_DELETE) {
		da_delete_methods old_method = softc->delete_method;

		/*
		 * Typically there are two reasons for failure here
		 * 1. Delete method was detected as supported but isn't
		 * 2. Delete failed due to invalid params e.g. too big
		 *
		 * While we will attempt to choose an alternative delete method
		 * this may result in short deletes if the existing delete
		 * requests from geom are big for the new method choosen.
		 *
		 * This method assumes that the error which triggered this
		 * will not retry the io otherwise a panic will occur
		 */
		dadeleteflag(softc, old_method, 0);
		dadeletemethodchoose(softc, DA_DELETE_DISABLE);
		if (softc->delete_method == DA_DELETE_DISABLE)
			xpt_print(ccb->ccb_h.path,
				  "%s failed, disabling BIO_DELETE\n",
				  da_delete_method_desc[old_method]);
		else
			xpt_print(ccb->ccb_h.path,
				  "%s failed, switching to %s BIO_DELETE\n",
				  da_delete_method_desc[old_method],
				  da_delete_method_desc[softc->delete_method]);

		while ((bp = bioq_takefirst(&softc->delete_run_queue)) != NULL)
			cam_iosched_queue_work(softc->cam_iosched, bp);
		cam_iosched_queue_work(softc->cam_iosched,
		    (struct bio *)ccb->ccb_h.ccb_bp);
		ccb->ccb_h.ccb_bp = NULL;
		return (0);
	}

	/* Detect unsupported PREVENT ALLOW MEDIUM REMOVAL. */
	if ((ccb->ccb_h.flags & CAM_CDB_POINTER) == 0 &&
	    (*cdb == PREVENT_ALLOW) &&
	    (softc->quirks & DA_Q_NO_PREVENT) == 0) {
		if (bootverbose)
			xpt_print(ccb->ccb_h.path,
			    "PREVENT ALLOW MEDIUM REMOVAL not supported.\n");
		softc->quirks |= DA_Q_NO_PREVENT;
		return (0);
	}

	/* Detect unsupported SYNCHRONIZE CACHE(10). */
	if ((ccb->ccb_h.flags & CAM_CDB_POINTER) == 0 &&
	    (*cdb == SYNCHRONIZE_CACHE) &&
	    (softc->quirks & DA_Q_NO_SYNC_CACHE) == 0) {
		if (bootverbose)
			xpt_print(ccb->ccb_h.path,
			    "SYNCHRONIZE CACHE(10) not supported.\n");
		softc->quirks |= DA_Q_NO_SYNC_CACHE;
		softc->disk->d_flags &= ~DISKFLAG_CANFLUSHCACHE;
		return (0);
	}

	/* Translation only possible if CDB is an array and cmd is R/W6 */
	if ((ccb->ccb_h.flags & CAM_CDB_POINTER) != 0 ||
	    (*cdb != READ_6 && *cdb != WRITE_6))
		return 0;

	xpt_print(ccb->ccb_h.path, "READ(6)/WRITE(6) not supported, "
	    "increasing minimum_cmd_size to 10.\n");
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
	u_int32_t  priority;
	da_ccb_state state;

	softc = (struct da_softc *)periph->softc;
	priority = done_ccb->ccb_h.pinfo.priority;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("dadone\n"));

	csio = &done_ccb->csio;
	state = csio->ccb_h.ccb_state & DA_CCB_TYPE_MASK;
	switch (state) {
	case DA_CCB_BUFFER_IO:
	case DA_CCB_DELETE:
	{
		struct bio *bp, *bp1;

		cam_periph_lock(periph);
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
				 * A retry was scheduled, so
				 * just return.
				 */
				cam_periph_unlock(periph);
				return;
			}
			bp = (struct bio *)done_ccb->ccb_h.ccb_bp;
			if (error != 0) {
				int queued_error;

				/*
				 * return all queued I/O with EIO, so that
				 * the client can retry these I/Os in the
				 * proper order should it attempt to recover.
				 */
				queued_error = EIO;

				if (error == ENXIO
				 && (softc->flags & DA_FLAG_PACK_INVALID)== 0) {
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
#ifdef CAM_IO_STATS
					softc->invalidations++;
#endif
					queued_error = ENXIO;
				}
				cam_iosched_flush(softc->cam_iosched, NULL,
					   queued_error);
				if (bp != NULL) {
					bp->bio_error = error;
					bp->bio_resid = bp->bio_bcount;
					bp->bio_flags |= BIO_ERROR;
				}
			} else if (bp != NULL) {
				if (state == DA_CCB_DELETE)
					bp->bio_resid = 0;
				else
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
		} else if (bp != NULL) {
			if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
				panic("REQ_CMP with QFRZN");
			if (state == DA_CCB_DELETE)
				bp->bio_resid = 0;
			else
				bp->bio_resid = csio->resid;
			if (csio->resid > 0)
				bp->bio_flags |= BIO_ERROR;
			if (softc->error_inject != 0) {
				bp->bio_error = softc->error_inject;
				bp->bio_resid = bp->bio_bcount;
				bp->bio_flags |= BIO_ERROR;
				softc->error_inject = 0;
			}
		}

		LIST_REMOVE(&done_ccb->ccb_h, periph_links.le);
		if (LIST_EMPTY(&softc->pending_ccbs))
			softc->flags |= DA_FLAG_WAS_OTAG;

		cam_iosched_bio_complete(softc->cam_iosched, bp, done_ccb);
		xpt_release_ccb(done_ccb);
		if (state == DA_CCB_DELETE) {
			TAILQ_HEAD(, bio) queue;

			TAILQ_INIT(&queue);
			TAILQ_CONCAT(&queue, &softc->delete_run_queue.queue, bio_queue);
			softc->delete_run_queue.insert_point = NULL;
			/*
			 * Normally, the xpt_release_ccb() above would make sure
			 * that when we have more work to do, that work would
			 * get kicked off. However, we specifically keep
			 * delete_running set to 0 before the call above to
			 * allow other I/O to progress when many BIO_DELETE
			 * requests are pushed down. We set delete_running to 0
			 * and call daschedule again so that we don't stall if
			 * there are no other I/Os pending apart from BIO_DELETEs.
			 */
			cam_iosched_trim_done(softc->cam_iosched);
			daschedule(periph);
			cam_periph_unlock(periph);
			while ((bp1 = TAILQ_FIRST(&queue)) != NULL) {
				TAILQ_REMOVE(&queue, bp1, bio_queue);
				bp1->bio_error = bp->bio_error;
				if (bp->bio_flags & BIO_ERROR) {
					bp1->bio_flags |= BIO_ERROR;
					bp1->bio_resid = bp1->bio_bcount;
				} else
					bp1->bio_resid = 0;
				biodone(bp1);
			}
		} else {
			daschedule(periph);
			cam_periph_unlock(periph);
		}
		if (bp != NULL)
			biodone(bp);
		return;
	}
	case DA_CCB_PROBE_RC:
	case DA_CCB_PROBE_RC16:
	{
		struct	   scsi_read_capacity_data *rdcap;
		struct     scsi_read_capacity_data_long *rcaplong;
		char	   announce_buf[80];
		int	   lbp;

		lbp = 0;
		rdcap = NULL;
		rcaplong = NULL;
		if (state == DA_CCB_PROBE_RC)
			rdcap =(struct scsi_read_capacity_data *)csio->data_ptr;
		else
			rcaplong = (struct scsi_read_capacity_data_long *)
				csio->data_ptr;

		if ((csio->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			struct disk_params *dp;
			uint32_t block_size;
			uint64_t maxsector;
			u_int lalba;	/* Lowest aligned LBA. */

			if (state == DA_CCB_PROBE_RC) {
				block_size = scsi_4btoul(rdcap->length);
				maxsector = scsi_4btoul(rdcap->addr);
				lalba = 0;

				/*
				 * According to SBC-2, if the standard 10
				 * byte READ CAPACITY command returns 2^32,
				 * we should issue the 16 byte version of
				 * the command, since the device in question
				 * has more sectors than can be represented
				 * with the short version of the command.
				 */
				if (maxsector == 0xffffffff) {
					free(rdcap, M_SCSIDA);
					xpt_release_ccb(done_ccb);
					softc->state = DA_STATE_PROBE_RC16;
					xpt_schedule(periph, priority);
					return;
				}
			} else {
				block_size = scsi_4btoul(rcaplong->length);
				maxsector = scsi_8btou64(rcaplong->addr);
				lalba = scsi_2btoul(rcaplong->lalba_lbp);
			}

			/*
			 * Because GEOM code just will panic us if we
			 * give them an 'illegal' value we'll avoid that
			 * here.
			 */
			if (block_size == 0) {
				block_size = 512;
				if (maxsector == 0)
					maxsector = -1;
			}
			if (block_size >= MAXPHYS) {
				xpt_print(periph->path,
				    "unsupportable block size %ju\n",
				    (uintmax_t) block_size);
				announce_buf[0] = '\0';
				cam_periph_invalidate(periph);
			} else {
				/*
				 * We pass rcaplong into dasetgeom(),
				 * because it will only use it if it is
				 * non-NULL.
				 */
				dasetgeom(periph, block_size, maxsector,
					  rcaplong, sizeof(*rcaplong));
				lbp = (lalba & SRC16_LBPME_A);
				dp = &softc->params;
				snprintf(announce_buf, sizeof(announce_buf),
				    "%juMB (%ju %u byte sectors)",
				    ((uintmax_t)dp->secsize * dp->sectors) /
				     (1024 * 1024),
				    (uintmax_t)dp->sectors, dp->secsize);
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
					      CAM_PRIORITY_NORMAL);
				cgd.ccb_h.func_code = XPT_GDEV_TYPE;
				xpt_action((union ccb *)&cgd);

				if (scsi_extract_sense_ccb(done_ccb,
				    &error_code, &sense_key, &asc, &ascq))
					have_sense = TRUE;
				else
					have_sense = FALSE;

				/*
				 * If we tried READ CAPACITY(16) and failed,
				 * fallback to READ CAPACITY(10).
				 */
				if ((state == DA_CCB_PROBE_RC16) &&
				    (softc->flags & DA_FLAG_CAN_RC16) &&
				    (((csio->ccb_h.status & CAM_STATUS_MASK) ==
					CAM_REQ_INVALID) ||
				     ((have_sense) &&
				      (error_code == SSD_CURRENT_ERROR) &&
				      (sense_key == SSD_KEY_ILLEGAL_REQUEST)))) {
					softc->flags &= ~DA_FLAG_CAN_RC16;
					free(rdcap, M_SCSIDA);
					xpt_release_ccb(done_ccb);
					softc->state = DA_STATE_PROBE_RC;
					xpt_schedule(periph, priority);
					return;
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

					dasetgeom(periph, 512, -1, NULL, 0);
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
		if (announce_buf[0] != '\0' &&
		    ((softc->flags & DA_FLAG_ANNOUNCED) == 0)) {
			/*
			 * Create our sysctl variables, now that we know
			 * we have successfully attached.
			 */
			/* increase the refcount */
			if (cam_periph_acquire(periph) == CAM_REQ_CMP) {
				taskqueue_enqueue(taskqueue_thread,
						  &softc->sysctl_task);
				xpt_announce_periph(periph, announce_buf);
				xpt_announce_quirks(periph, softc->quirks,
				    DA_Q_BIT_STRING);
			} else {
				xpt_print(periph->path, "fatal error, "
				    "could not acquire reference count\n");
			}
		}

		/* We already probed the device. */
		if (softc->flags & DA_FLAG_PROBED) {
			daprobedone(periph, done_ccb);
			return;
		}

		/* Ensure re-probe doesn't see old delete. */
		softc->delete_available = 0;
		dadeleteflag(softc, DA_DELETE_ZERO, 1);
		if (lbp && (softc->quirks & DA_Q_NO_UNMAP) == 0) {
			/*
			 * Based on older SBC-3 spec revisions
			 * any of the UNMAP methods "may" be
			 * available via LBP given this flag so
			 * we flag all of them as availble and
			 * then remove those which further
			 * probes confirm aren't available
			 * later.
			 *
			 * We could also check readcap(16) p_type
			 * flag to exclude one or more invalid
			 * write same (X) types here
			 */
			dadeleteflag(softc, DA_DELETE_WS16, 1);
			dadeleteflag(softc, DA_DELETE_WS10, 1);
			dadeleteflag(softc, DA_DELETE_UNMAP, 1);

			xpt_release_ccb(done_ccb);
			softc->state = DA_STATE_PROBE_LBP;
			xpt_schedule(periph, priority);
			return;
		}

		xpt_release_ccb(done_ccb);
		softc->state = DA_STATE_PROBE_BDC;
		xpt_schedule(periph, priority);
		return;
	}
	case DA_CCB_PROBE_LBP:
	{
		struct scsi_vpd_logical_block_prov *lbp;

		lbp = (struct scsi_vpd_logical_block_prov *)csio->data_ptr;

		if ((csio->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			/*
			 * T10/1799-D Revision 31 states at least one of these
			 * must be supported but we don't currently enforce this.
			 */
			dadeleteflag(softc, DA_DELETE_WS16,
				     (lbp->flags & SVPD_LBP_WS16));
			dadeleteflag(softc, DA_DELETE_WS10,
				     (lbp->flags & SVPD_LBP_WS10));
			dadeleteflag(softc, DA_DELETE_UNMAP,
				     (lbp->flags & SVPD_LBP_UNMAP));
		} else {
			int error;
			error = daerror(done_ccb, CAM_RETRY_SELTO,
					SF_RETRY_UA|SF_NO_PRINT);
			if (error == ERESTART)
				return;
			else if (error != 0) {
				if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
					/* Don't wedge this device's queue */
					cam_release_devq(done_ccb->ccb_h.path,
							 /*relsim_flags*/0,
							 /*reduction*/0,
							 /*timeout*/0,
							 /*getcount_only*/0);
				}

				/*
				 * Failure indicates we don't support any SBC-3
				 * delete methods with UNMAP
				 */
			}
		}

		free(lbp, M_SCSIDA);
		xpt_release_ccb(done_ccb);
		softc->state = DA_STATE_PROBE_BLK_LIMITS;
		xpt_schedule(periph, priority);
		return;
	}
	case DA_CCB_PROBE_BLK_LIMITS:
	{
		struct scsi_vpd_block_limits *block_limits;

		block_limits = (struct scsi_vpd_block_limits *)csio->data_ptr;

		if ((csio->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			uint32_t max_txfer_len = scsi_4btoul(
				block_limits->max_txfer_len);
			uint32_t max_unmap_lba_cnt = scsi_4btoul(
				block_limits->max_unmap_lba_cnt);
			uint32_t max_unmap_blk_cnt = scsi_4btoul(
				block_limits->max_unmap_blk_cnt);
			uint64_t ws_max_blks = scsi_8btou64(
				block_limits->max_write_same_length);

			if (max_txfer_len != 0) {
				softc->disk->d_maxsize = MIN(softc->maxio,
				    (off_t)max_txfer_len * softc->params.secsize);
			}

			/*
			 * We should already support UNMAP but we check lba
			 * and block count to be sure
			 */
			if (max_unmap_lba_cnt != 0x00L &&
			    max_unmap_blk_cnt != 0x00L) {
				softc->unmap_max_lba = max_unmap_lba_cnt;
				softc->unmap_max_ranges = min(max_unmap_blk_cnt,
					UNMAP_MAX_RANGES);
			} else {
				/*
				 * Unexpected UNMAP limits which means the
				 * device doesn't actually support UNMAP
				 */
				dadeleteflag(softc, DA_DELETE_UNMAP, 0);
			}

			if (ws_max_blks != 0x00L)
				softc->ws_max_blks = ws_max_blks;
		} else {
			int error;
			error = daerror(done_ccb, CAM_RETRY_SELTO,
					SF_RETRY_UA|SF_NO_PRINT);
			if (error == ERESTART)
				return;
			else if (error != 0) {
				if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
					/* Don't wedge this device's queue */
					cam_release_devq(done_ccb->ccb_h.path,
							 /*relsim_flags*/0,
							 /*reduction*/0,
							 /*timeout*/0,
							 /*getcount_only*/0);
				}

				/*
				 * Failure here doesn't mean UNMAP is not
				 * supported as this is an optional page.
				 */
				softc->unmap_max_lba = 1;
				softc->unmap_max_ranges = 1;
			}
		}

		free(block_limits, M_SCSIDA);
		xpt_release_ccb(done_ccb);
		softc->state = DA_STATE_PROBE_BDC;
		xpt_schedule(periph, priority);
		return;
	}
	case DA_CCB_PROBE_BDC:
	{
		struct scsi_vpd_block_characteristics *bdc;

		bdc = (struct scsi_vpd_block_characteristics *)csio->data_ptr;

		if ((csio->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			/*
			 * Disable queue sorting for non-rotational media
			 * by default.
			 */
			u_int16_t old_rate = softc->disk->d_rotation_rate;

			softc->disk->d_rotation_rate =
				scsi_2btoul(bdc->medium_rotation_rate);
			if (softc->disk->d_rotation_rate ==
			    SVPD_BDC_RATE_NON_ROTATING) {
				cam_iosched_set_sort_queue(softc->cam_iosched, 0);
				softc->rotating = 0;
			}
			if (softc->disk->d_rotation_rate != old_rate) {
				disk_attr_changed(softc->disk,
				    "GEOM::rotation_rate", M_NOWAIT);
			}
		} else {
			int error;
			error = daerror(done_ccb, CAM_RETRY_SELTO,
					SF_RETRY_UA|SF_NO_PRINT);
			if (error == ERESTART)
				return;
			else if (error != 0) {
				if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
					/* Don't wedge this device's queue */
					cam_release_devq(done_ccb->ccb_h.path,
							 /*relsim_flags*/0,
							 /*reduction*/0,
							 /*timeout*/0,
							 /*getcount_only*/0);
				}
			}
		}

		free(bdc, M_SCSIDA);
		xpt_release_ccb(done_ccb);
		softc->state = DA_STATE_PROBE_ATA;
		xpt_schedule(periph, priority);
		return;
	}
	case DA_CCB_PROBE_ATA:
	{
		int i;
		struct ata_params *ata_params;
		int16_t *ptr;

		ata_params = (struct ata_params *)csio->data_ptr;
		ptr = (uint16_t *)ata_params;

		if ((csio->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			uint16_t old_rate;

			for (i = 0; i < sizeof(*ata_params) / 2; i++)
				ptr[i] = le16toh(ptr[i]);
			if (ata_params->support_dsm & ATA_SUPPORT_DSM_TRIM &&
			    (softc->quirks & DA_Q_NO_UNMAP) == 0) {
				dadeleteflag(softc, DA_DELETE_ATA_TRIM, 1);
				if (ata_params->max_dsm_blocks != 0)
					softc->trim_max_ranges = min(
					  softc->trim_max_ranges,
					  ata_params->max_dsm_blocks *
					  ATA_DSM_BLK_RANGES);
			}
			/*
			 * Disable queue sorting for non-rotational media
			 * by default.
			 */
			old_rate = softc->disk->d_rotation_rate;
			softc->disk->d_rotation_rate =
			    ata_params->media_rotation_rate;
			if (softc->disk->d_rotation_rate ==
			    ATA_RATE_NON_ROTATING) {
				cam_iosched_set_sort_queue(softc->cam_iosched, 0);
				softc->rotating = 0;
			}
			if (softc->disk->d_rotation_rate != old_rate) {
				disk_attr_changed(softc->disk,
				    "GEOM::rotation_rate", M_NOWAIT);
			}
		} else {
			int error;
			error = daerror(done_ccb, CAM_RETRY_SELTO,
					SF_RETRY_UA|SF_NO_PRINT);
			if (error == ERESTART)
				return;
			else if (error != 0) {
				if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
					/* Don't wedge this device's queue */
					cam_release_devq(done_ccb->ccb_h.path,
							 /*relsim_flags*/0,
							 /*reduction*/0,
							 /*timeout*/0,
							 /*getcount_only*/0);
				}
			}
		}

		free(ata_params, M_SCSIDA);
		daprobedone(periph, done_ccb);
		return;
	}
	case DA_CCB_DUMP:
		/* No-op.  We're polling */
		return;
	case DA_CCB_TUR:
	{
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {

			if (daerror(done_ccb, CAM_RETRY_SELTO,
			    SF_RETRY_UA | SF_NO_RECOVERY | SF_NO_PRINT) ==
			    ERESTART)
				return;
			if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
				cam_release_devq(done_ccb->ccb_h.path,
						 /*relsim_flags*/0,
						 /*reduction*/0,
						 /*timeout*/0,
						 /*getcount_only*/0);
		}
		xpt_release_ccb(done_ccb);
		cam_periph_release_locked(periph);
		return;
	}
	default:
		break;
	}
	xpt_release_ccb(done_ccb);
}

static void
dareprobe(struct cam_periph *periph)
{
	struct da_softc	  *softc;
	cam_status status;

	softc = (struct da_softc *)periph->softc;

	/* Probe in progress; don't interfere. */
	if (softc->state != DA_STATE_NORMAL)
		return;

	status = cam_periph_acquire(periph);
	KASSERT(status == CAM_REQ_CMP,
	    ("dareprobe: cam_periph_acquire failed"));

	if (softc->flags & DA_FLAG_CAN_RC16)
		softc->state = DA_STATE_PROBE_RC16;
	else
		softc->state = DA_STATE_PROBE_RC;

	xpt_schedule(periph, CAM_PRIORITY_DEV);
}

static int
daerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	struct da_softc	  *softc;
	struct cam_periph *periph;
	int error, error_code, sense_key, asc, ascq;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct da_softc *)periph->softc;

 	/*
	 * Automatically detect devices that do not support
 	 * READ(6)/WRITE(6) and upgrade to using 10 byte cdbs.
 	 */
	error = 0;
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INVALID) {
		error = cmd6workaround(ccb);
	} else if (scsi_extract_sense_ccb(ccb,
	    &error_code, &sense_key, &asc, &ascq)) {
		if (sense_key == SSD_KEY_ILLEGAL_REQUEST)
 			error = cmd6workaround(ccb);
		/*
		 * If the target replied with CAPACITY DATA HAS CHANGED UA,
		 * query the capacity and notify upper layers.
		 */
		else if (sense_key == SSD_KEY_UNIT_ATTENTION &&
		    asc == 0x2A && ascq == 0x09) {
			xpt_print(periph->path, "Capacity data has changed\n");
			softc->flags &= ~DA_FLAG_PROBED;
			dareprobe(periph);
			sense_flags |= SF_NO_PRINT;
		} else if (sense_key == SSD_KEY_UNIT_ATTENTION &&
		    asc == 0x28 && ascq == 0x00) {
			softc->flags &= ~DA_FLAG_PROBED;
			disk_media_changed(softc->disk, M_NOWAIT);
		} else if (sense_key == SSD_KEY_UNIT_ATTENTION &&
		    asc == 0x3F && ascq == 0x03) {
			xpt_print(periph->path, "INQUIRY data has changed\n");
			softc->flags &= ~DA_FLAG_PROBED;
			dareprobe(periph);
			sense_flags |= SF_NO_PRINT;
		} else if (sense_key == SSD_KEY_NOT_READY &&
		    asc == 0x3a && (softc->flags & DA_FLAG_PACK_INVALID) == 0) {
			softc->flags |= DA_FLAG_PACK_INVALID;
			disk_media_gone(softc->disk, M_NOWAIT);
		}
	}
	if (error == ERESTART)
		return (ERESTART);

#ifdef CAM_IO_STATS
	switch (ccb->ccb_h.status & CAM_STATUS_MASK) {
	case CAM_CMD_TIMEOUT:
		softc->timeouts++;
		break;
	case CAM_REQ_ABORTED:
	case CAM_REQ_CMP_ERR:
	case CAM_REQ_TERMIO:
	case CAM_UNREC_HBA_ERROR:
	case CAM_DATA_RUN_ERR:
		softc->errors++;
		break;
	default:
		break;
	}
#endif

	/*
	 * XXX
	 * Until we have a better way of doing pack validation,
	 * don't treat UAs as errors.
	 */
	sense_flags |= SF_RETRY_UA;

	if (softc->quirks & DA_Q_RETRY_BUSY)
		sense_flags |= SF_RETRY_BUSY;
	return(cam_periph_error(ccb, cam_flags, sense_flags,
				&softc->saved_ccb));
}

static void
damediapoll(void *arg)
{
	struct cam_periph *periph = arg;
	struct da_softc *softc = periph->softc;

	if (!cam_iosched_has_work_flags(softc->cam_iosched, DA_WORK_TUR) &&
	    LIST_EMPTY(&softc->pending_ccbs)) {
		if (cam_periph_acquire(periph) == CAM_REQ_CMP) {
			cam_iosched_set_work_flags(softc->cam_iosched, DA_WORK_TUR);
			daschedule(periph);
		}
	}
	/* Queue us up again */
	if (da_poll_period != 0)
		callout_schedule(&softc->mediapoll_c, da_poll_period * hz);
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

	ccb = cam_periph_getccb(periph, CAM_PRIORITY_NORMAL);

	scsi_prevent(&ccb->csio,
		     /*retries*/1,
		     /*cbcfp*/dadone,
		     MSG_SIMPLE_Q_TAG,
		     action,
		     SSD_FULL_SIZE,
		     5000);

	error = cam_periph_runccb(ccb, daerror, CAM_RETRY_SELTO,
	    SF_RETRY_UA | SF_NO_PRINT, softc->disk->d_devstat);

	if (error == 0) {
		if (action == PR_ALLOW)
			softc->flags &= ~DA_FLAG_PACK_LOCKED;
		else
			softc->flags |= DA_FLAG_PACK_LOCKED;
	}

	xpt_release_ccb(ccb);
}

static void
dasetgeom(struct cam_periph *periph, uint32_t block_len, uint64_t maxsector,
	  struct scsi_read_capacity_data_long *rcaplong, size_t rcap_len)
{
	struct ccb_calc_geometry ccg;
	struct da_softc *softc;
	struct disk_params *dp;
	u_int lbppbe, lalba;
	int error;

	softc = (struct da_softc *)periph->softc;

	dp = &softc->params;
	dp->secsize = block_len;
	dp->sectors = maxsector + 1;
	if (rcaplong != NULL) {
		lbppbe = rcaplong->prot_lbppbe & SRC16_LBPPBE;
		lalba = scsi_2btoul(rcaplong->lalba_lbp);
		lalba &= SRC16_LALBA_A;
	} else {
		lbppbe = 0;
		lalba = 0;
	}

	if (lbppbe > 0) {
		dp->stripesize = block_len << lbppbe;
		dp->stripeoffset = (dp->stripesize - block_len * lalba) %
		    dp->stripesize;
	} else if (softc->quirks & DA_Q_4K) {
		dp->stripesize = 4096;
		dp->stripeoffset = 0;
	} else {
		dp->stripesize = 0;
		dp->stripeoffset = 0;
	}
	/*
	 * Have the controller provide us with a geometry
	 * for this disk.  The only time the geometry
	 * matters is when we boot and the controller
	 * is the only one knowledgeable enough to come
	 * up with something that will make this a bootable
	 * device.
	 */
	xpt_setup_ccb(&ccg.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
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

	/*
	 * If the user supplied a read capacity buffer, and if it is
	 * different than the previous buffer, update the data in the EDT.
	 * If it's the same, we don't bother.  This avoids sending an
	 * update every time someone opens this device.
	 */
	if ((rcaplong != NULL)
	 && (bcmp(rcaplong, &softc->rcaplong,
		  min(sizeof(softc->rcaplong), rcap_len)) != 0)) {
		struct ccb_dev_advinfo cdai;

		xpt_setup_ccb(&cdai.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
		cdai.ccb_h.func_code = XPT_DEV_ADVINFO;
		cdai.buftype = CDAI_TYPE_RCAPLONG;
		cdai.flags = CDAI_FLAG_STORE;
		cdai.bufsiz = rcap_len;
		cdai.buf = (uint8_t *)rcaplong;
		xpt_action((union ccb *)&cdai);
		if ((cdai.ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(cdai.ccb_h.path, 0, 0, 0, FALSE);
		if (cdai.ccb_h.status != CAM_REQ_CMP) {
			xpt_print(periph->path, "%s: failed to set read "
				  "capacity advinfo\n", __func__);
			/* Use cam_error_print() to decode the status */
			cam_error_print((union ccb *)&cdai, CAM_ESF_CAM_STATUS,
					CAM_EPF_ALL);
		} else {
			bcopy(rcaplong, &softc->rcaplong,
			      min(sizeof(softc->rcaplong), rcap_len));
		}
	}

	softc->disk->d_sectorsize = softc->params.secsize;
	softc->disk->d_mediasize = softc->params.secsize * (off_t)softc->params.sectors;
	softc->disk->d_stripesize = softc->params.stripesize;
	softc->disk->d_stripeoffset = softc->params.stripeoffset;
	/* XXX: these are not actually "firmware" values, so they may be wrong */
	softc->disk->d_fwsectors = softc->params.secs_per_track;
	softc->disk->d_fwheads = softc->params.heads;
	softc->disk->d_devstat->block_size = softc->params.secsize;
	softc->disk->d_devstat->flags &= ~DEVSTAT_BS_UNAVAILABLE;

	error = disk_resize(softc->disk, M_NOWAIT);
	if (error != 0)
		xpt_print(periph->path, "disk_resize(9) failed, error = %d\n", error);
}

static void
dasendorderedtag(void *arg)
{
	struct da_softc *softc = arg;

	if (da_send_ordered) {
		if (!LIST_EMPTY(&softc->pending_ccbs)) {
			if ((softc->flags & DA_FLAG_WAS_OTAG) == 0)
				softc->flags |= DA_FLAG_NEED_OTAG;
			softc->flags &= ~DA_FLAG_WAS_OTAG;
		}
	}
	/* Queue us up again */
	callout_reset(&softc->sendordered_c,
	    (da_default_timeout * hz) / DA_ORDEREDTAG_INTERVAL,
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
	union ccb *ccb;
	int error;

	CAM_PERIPH_FOREACH(periph, &dadriver) {
		softc = (struct da_softc *)periph->softc;
		if (SCHEDULER_STOPPED()) {
			/* If we paniced with the lock held, do not recurse. */
			if (!cam_periph_owned(periph) &&
			    (softc->flags & DA_FLAG_OPEN)) {
				dadump(softc->disk, NULL, 0, 0, 0);
			}
			continue;
		}
		cam_periph_lock(periph);

		/*
		 * We only sync the cache if the drive is still open, and
		 * if the drive is capable of it..
		 */
		if (((softc->flags & DA_FLAG_OPEN) == 0)
		 || (softc->quirks & DA_Q_NO_SYNC_CACHE)) {
			cam_periph_unlock(periph);
			continue;
		}

		ccb = cam_periph_getccb(periph, CAM_PRIORITY_NORMAL);
		scsi_synchronize_cache(&ccb->csio,
				       /*retries*/0,
				       /*cbfcnp*/dadone,
				       MSG_SIMPLE_Q_TAG,
				       /*begin_lba*/0, /* whole disk */
				       /*lb_count*/0,
				       SSD_FULL_SIZE,
				       60 * 60 * 1000);

		error = cam_periph_runccb(ccb, daerror, /*cam_flags*/0,
		    /*sense_flags*/ SF_NO_RECOVERY | SF_NO_RETRY | SF_QUIET_IR,
		    softc->disk->d_devstat);
		if (error != 0)
			xpt_print(periph->path, "Synchronize cache failed\n");
		xpt_release_ccb(ccb);
		cam_periph_unlock(periph);
	}
}

#else /* !_KERNEL */

/*
 * XXX These are only left out of the kernel build to silence warnings.  If,
 * for some reason these functions are used in the kernel, the ifdefs should
 * be moved so they are included both in the kernel and userland.
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

void
scsi_read_defects(struct ccb_scsiio *csio, uint32_t retries,
		  void (*cbfcnp)(struct cam_periph *, union ccb *),
		  uint8_t tag_action, uint8_t list_format,
		  uint32_t addr_desc_index, uint8_t *data_ptr,
		  uint32_t dxfer_len, int minimum_cmd_size, 
		  uint8_t sense_len, uint32_t timeout)
{
	uint8_t cdb_len;

	/*
	 * These conditions allow using the 10 byte command.  Otherwise we
	 * need to use the 12 byte command.
	 */
	if ((minimum_cmd_size <= 10)
	 && (addr_desc_index == 0) 
	 && (dxfer_len <= SRDD10_MAX_LENGTH)) {
		struct scsi_read_defect_data_10 *cdb10;

		cdb10 = (struct scsi_read_defect_data_10 *)
			&csio->cdb_io.cdb_bytes;

		cdb_len = sizeof(*cdb10);
		bzero(cdb10, cdb_len);
                cdb10->opcode = READ_DEFECT_DATA_10;
                cdb10->format = list_format;
                scsi_ulto2b(dxfer_len, cdb10->alloc_length);
	} else {
		struct scsi_read_defect_data_12 *cdb12;

		cdb12 = (struct scsi_read_defect_data_12 *)
			&csio->cdb_io.cdb_bytes;

		cdb_len = sizeof(*cdb12);
		bzero(cdb12, cdb_len);
                cdb12->opcode = READ_DEFECT_DATA_12;
                cdb12->format = list_format;
                scsi_ulto4b(dxfer_len, cdb12->alloc_length);
		scsi_ulto4b(addr_desc_index, cdb12->address_descriptor_index);
	}

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ CAM_DIR_IN,
		      tag_action,
		      data_ptr,
		      dxfer_len,
		      sense_len,
		      cdb_len,
		      timeout);
}

void
scsi_sanitize(struct ccb_scsiio *csio, u_int32_t retries,
	      void (*cbfcnp)(struct cam_periph *, union ccb *),
	      u_int8_t tag_action, u_int8_t byte2, u_int16_t control,
	      u_int8_t *data_ptr, u_int32_t dxfer_len, u_int8_t sense_len,
	      u_int32_t timeout)
{
	struct scsi_sanitize *scsi_cmd;

	scsi_cmd = (struct scsi_sanitize *)&csio->cdb_io.cdb_bytes;
	scsi_cmd->opcode = SANITIZE;
	scsi_cmd->byte2 = byte2;
	scsi_cmd->control = control;
	scsi_ulto2b(dxfer_len, scsi_cmd->length);

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
