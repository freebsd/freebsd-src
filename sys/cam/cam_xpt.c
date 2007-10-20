/*-
 * Implementation of the Common Access Method Transport (XPT) layer.
 *
 * Copyright (c) 1997, 1998, 1999 Justin T. Gibbs.
 * Copyright (c) 1997, 1998, 1999 Kenneth D. Merry.
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
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/md5.h>
#include <sys/interrupt.h>
#include <sys/sbuf.h>
#include <sys/taskqueue.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/kthread.h>

#ifdef PC98
#include <pc98/pc98/pc98_machdep.h>	/* geometry translation */
#endif

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_pass.h>
#include <machine/stdarg.h>	/* for xpt_print below */
#include "opt_cam.h"

/* Datastructures internal to the xpt layer */
MALLOC_DEFINE(M_CAMXPT, "CAM XPT", "CAM XPT buffers");

/* Object for defering XPT actions to a taskqueue */
struct xpt_task {
	struct task	task;
	void		*data1;
	uintptr_t	data2;
};

/*
 * Definition of an async handler callback block.  These are used to add
 * SIMs and peripherals to the async callback lists.
 */
struct async_node {
	SLIST_ENTRY(async_node)	links;
	u_int32_t	event_enable;	/* Async Event enables */
	void		(*callback)(void *arg, u_int32_t code,
				    struct cam_path *path, void *args);
	void		*callback_arg;
};

SLIST_HEAD(async_list, async_node);
SLIST_HEAD(periph_list, cam_periph);

/*
 * This is the maximum number of high powered commands (e.g. start unit)
 * that can be outstanding at a particular time.
 */
#ifndef CAM_MAX_HIGHPOWER
#define CAM_MAX_HIGHPOWER  4
#endif

/*
 * Structure for queueing a device in a run queue.
 * There is one run queue for allocating new ccbs,
 * and another for sending ccbs to the controller.
 */
struct cam_ed_qinfo {
	cam_pinfo pinfo;
	struct	  cam_ed *device;
};

/*
 * The CAM EDT (Existing Device Table) contains the device information for
 * all devices for all busses in the system.  The table contains a
 * cam_ed structure for each device on the bus.
 */
struct cam_ed {
	TAILQ_ENTRY(cam_ed) links;
	struct	cam_ed_qinfo alloc_ccb_entry;
	struct	cam_ed_qinfo send_ccb_entry;
	struct	cam_et	 *target;
	struct	cam_sim  *sim;
	lun_id_t	 lun_id;
	struct	camq drvq;		/*
					 * Queue of type drivers wanting to do
					 * work on this device.
					 */
	struct	cam_ccbq ccbq;		/* Queue of pending ccbs */
	struct	async_list asyncs;	/* Async callback info for this B/T/L */
	struct	periph_list periphs;	/* All attached devices */
	u_int	generation;		/* Generation number */
	struct	cam_periph *owner;	/* Peripheral driver's ownership tag */
	struct	xpt_quirk_entry *quirk;	/* Oddities about this device */
					/* Storage for the inquiry data */
	cam_proto	 protocol;
	u_int		 protocol_version;
	cam_xport	 transport;
	u_int		 transport_version;
	struct		 scsi_inquiry_data inq_data;
	u_int8_t	 inq_flags;	/*
					 * Current settings for inquiry flags.
					 * This allows us to override settings
					 * like disconnection and tagged
					 * queuing for a device.
					 */
	u_int8_t	 queue_flags;	/* Queue flags from the control page */
	u_int8_t	 serial_num_len;
	u_int8_t	*serial_num;
	u_int32_t	 qfrozen_cnt;
	u_int32_t	 flags;
#define CAM_DEV_UNCONFIGURED	 	0x01
#define CAM_DEV_REL_TIMEOUT_PENDING	0x02
#define CAM_DEV_REL_ON_COMPLETE		0x04
#define CAM_DEV_REL_ON_QUEUE_EMPTY	0x08
#define CAM_DEV_RESIZE_QUEUE_NEEDED	0x10
#define CAM_DEV_TAG_AFTER_COUNT		0x20
#define CAM_DEV_INQUIRY_DATA_VALID	0x40
#define	CAM_DEV_IN_DV			0x80
#define	CAM_DEV_DV_HIT_BOTTOM		0x100
	u_int32_t	 tag_delay_count;
#define	CAM_TAG_DELAY_COUNT		5
	u_int32_t	 tag_saved_openings;
	u_int32_t	 refcount;
	struct callout	 callout;
};

/*
 * Each target is represented by an ET (Existing Target).  These
 * entries are created when a target is successfully probed with an
 * identify, and removed when a device fails to respond after a number
 * of retries, or a bus rescan finds the device missing.
 */
struct cam_et { 
	TAILQ_HEAD(, cam_ed) ed_entries;
	TAILQ_ENTRY(cam_et) links;
	struct	cam_eb	*bus;	
	target_id_t	target_id;
	u_int32_t	refcount;	
	u_int		generation;
	struct		timeval last_reset;
};

/*
 * Each bus is represented by an EB (Existing Bus).  These entries
 * are created by calls to xpt_bus_register and deleted by calls to
 * xpt_bus_deregister.
 */
struct cam_eb { 
	TAILQ_HEAD(, cam_et) et_entries;
	TAILQ_ENTRY(cam_eb)  links;
	path_id_t	     path_id;
	struct cam_sim	     *sim;
	struct timeval	     last_reset;
	u_int32_t	     flags;
#define	CAM_EB_RUNQ_SCHEDULED	0x01
	u_int32_t	     refcount;
	u_int		     generation;
	device_t	     parent_dev;
};

struct cam_path {
	struct cam_periph *periph;
	struct cam_eb	  *bus;
	struct cam_et	  *target;
	struct cam_ed	  *device;
};

struct xpt_quirk_entry {
	struct scsi_inquiry_pattern inq_pat;
	u_int8_t quirks;
#define	CAM_QUIRK_NOLUNS	0x01
#define	CAM_QUIRK_NOSERIAL	0x02
#define	CAM_QUIRK_HILUNS	0x04
#define	CAM_QUIRK_NOHILUNS	0x08
	u_int mintags;
	u_int maxtags;
};

static int cam_srch_hi = 0;
TUNABLE_INT("kern.cam.cam_srch_hi", &cam_srch_hi);
static int sysctl_cam_search_luns(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_kern_cam, OID_AUTO, cam_srch_hi, CTLTYPE_INT|CTLFLAG_RW, 0, 0,
    sysctl_cam_search_luns, "I",
    "allow search above LUN 7 for SCSI3 and greater devices");

#define	CAM_SCSI2_MAXLUN	8
/*
 * If we're not quirked to search <= the first 8 luns
 * and we are either quirked to search above lun 8,
 * or we're > SCSI-2 and we've enabled hilun searching,
 * or we're > SCSI-2 and the last lun was a success,
 * we can look for luns above lun 8.
 */
#define	CAN_SRCH_HI_SPARSE(dv)				\
  (((dv->quirk->quirks & CAM_QUIRK_NOHILUNS) == 0) 	\
  && ((dv->quirk->quirks & CAM_QUIRK_HILUNS)		\
  || (SID_ANSI_REV(&dv->inq_data) > SCSI_REV_2 && cam_srch_hi)))

#define	CAN_SRCH_HI_DENSE(dv)				\
  (((dv->quirk->quirks & CAM_QUIRK_NOHILUNS) == 0) 	\
  && ((dv->quirk->quirks & CAM_QUIRK_HILUNS)		\
  || (SID_ANSI_REV(&dv->inq_data) > SCSI_REV_2)))

typedef enum {
	XPT_FLAG_OPEN		= 0x01
} xpt_flags;

struct xpt_softc {
	xpt_flags		flags;
	u_int32_t		xpt_generation;

	/* number of high powered commands that can go through right now */
	STAILQ_HEAD(highpowerlist, ccb_hdr)	highpowerq;
	int			num_highpower;

	/* queue for handling async rescan requests. */
	TAILQ_HEAD(, ccb_hdr) ccb_scanq;

	/* Registered busses */
	TAILQ_HEAD(,cam_eb)	xpt_busses;
	u_int			bus_generation;

	struct intr_config_hook	*xpt_config_hook;

	struct mtx		xpt_topo_lock;
	struct mtx		xpt_lock;
};

static const char quantum[] = "QUANTUM";
static const char sony[] = "SONY";
static const char west_digital[] = "WDIGTL";
static const char samsung[] = "SAMSUNG";
static const char seagate[] = "SEAGATE";
static const char microp[] = "MICROP";

static struct xpt_quirk_entry xpt_quirk_table[] = 
{
	{
		/* Reports QUEUE FULL for temporary resource shortages */
		{ T_DIRECT, SIP_MEDIA_FIXED, quantum, "XP39100*", "*" },
		/*quirks*/0, /*mintags*/24, /*maxtags*/32
	},
	{
		/* Reports QUEUE FULL for temporary resource shortages */
		{ T_DIRECT, SIP_MEDIA_FIXED, quantum, "XP34550*", "*" },
		/*quirks*/0, /*mintags*/24, /*maxtags*/32
	},
	{
		/* Reports QUEUE FULL for temporary resource shortages */
		{ T_DIRECT, SIP_MEDIA_FIXED, quantum, "XP32275*", "*" },
		/*quirks*/0, /*mintags*/24, /*maxtags*/32
	},
	{
		/* Broken tagged queuing drive */
		{ T_DIRECT, SIP_MEDIA_FIXED, microp, "4421-07*", "*" },
		/*quirks*/0, /*mintags*/0, /*maxtags*/0
	},
	{
		/* Broken tagged queuing drive */
		{ T_DIRECT, SIP_MEDIA_FIXED, "HP", "C372*", "*" },
		/*quirks*/0, /*mintags*/0, /*maxtags*/0
	},
	{
		/* Broken tagged queuing drive */
		{ T_DIRECT, SIP_MEDIA_FIXED, microp, "3391*", "x43h" },
		/*quirks*/0, /*mintags*/0, /*maxtags*/0
	},
	{
		/*
		 * Unfortunately, the Quantum Atlas III has the same
		 * problem as the Atlas II drives above.
		 * Reported by: "Johan Granlund" <johan@granlund.nu>
		 *
		 * For future reference, the drive with the problem was:
		 * QUANTUM QM39100TD-SW N1B0
		 * 
		 * It's possible that Quantum will fix the problem in later
		 * firmware revisions.  If that happens, the quirk entry
		 * will need to be made specific to the firmware revisions
		 * with the problem.
		 * 
		 */
		/* Reports QUEUE FULL for temporary resource shortages */
		{ T_DIRECT, SIP_MEDIA_FIXED, quantum, "QM39100*", "*" },
		/*quirks*/0, /*mintags*/24, /*maxtags*/32
	},
	{
		/*
		 * 18 Gig Atlas III, same problem as the 9G version.
		 * Reported by: Andre Albsmeier
		 *		<andre.albsmeier@mchp.siemens.de>
		 *
		 * For future reference, the drive with the problem was:
		 * QUANTUM QM318000TD-S N491
		 */
		/* Reports QUEUE FULL for temporary resource shortages */
		{ T_DIRECT, SIP_MEDIA_FIXED, quantum, "QM318000*", "*" },
		/*quirks*/0, /*mintags*/24, /*maxtags*/32
	},
	{
		/*
		 * Broken tagged queuing drive
		 * Reported by: Bret Ford <bford@uop.cs.uop.edu>
		 *         and: Martin Renters <martin@tdc.on.ca>
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, seagate, "ST410800*", "71*" },
		/*quirks*/0, /*mintags*/0, /*maxtags*/0
	},
		/*
		 * The Seagate Medalist Pro drives have very poor write
		 * performance with anything more than 2 tags.
		 * 
		 * Reported by:  Paul van der Zwan <paulz@trantor.xs4all.nl>
		 * Drive:  <SEAGATE ST36530N 1444>
		 *
		 * Reported by:  Jeremy Lea <reg@shale.csir.co.za>
		 * Drive:  <SEAGATE ST34520W 1281>
		 *
		 * No one has actually reported that the 9G version
		 * (ST39140*) of the Medalist Pro has the same problem, but
		 * we're assuming that it does because the 4G and 6.5G
		 * versions of the drive are broken.
		 */
	{
		{ T_DIRECT, SIP_MEDIA_FIXED, seagate, "ST34520*", "*"},
		/*quirks*/0, /*mintags*/2, /*maxtags*/2
	},
	{
		{ T_DIRECT, SIP_MEDIA_FIXED, seagate, "ST36530*", "*"},
		/*quirks*/0, /*mintags*/2, /*maxtags*/2
	},
	{
		{ T_DIRECT, SIP_MEDIA_FIXED, seagate, "ST39140*", "*"},
		/*quirks*/0, /*mintags*/2, /*maxtags*/2
	},
	{
		/*
		 * Slow when tagged queueing is enabled.  Write performance
		 * steadily drops off with more and more concurrent
		 * transactions.  Best sequential write performance with
		 * tagged queueing turned off and write caching turned on.
		 *
		 * PR:  kern/10398
		 * Submitted by:  Hideaki Okada <hokada@isl.melco.co.jp>
		 * Drive:  DCAS-34330 w/ "S65A" firmware.
		 *
		 * The drive with the problem had the "S65A" firmware
		 * revision, and has also been reported (by Stephen J.
		 * Roznowski <sjr@home.net>) for a drive with the "S61A"
		 * firmware revision.
		 *
		 * Although no one has reported problems with the 2 gig
		 * version of the DCAS drive, the assumption is that it
		 * has the same problems as the 4 gig version.  Therefore
		 * this quirk entries disables tagged queueing for all
		 * DCAS drives.
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "IBM", "DCAS*", "*" },
		/*quirks*/0, /*mintags*/0, /*maxtags*/0
	},
	{
		/* Broken tagged queuing drive */
		{ T_DIRECT, SIP_MEDIA_REMOVABLE, "iomega", "jaz*", "*" },
		/*quirks*/0, /*mintags*/0, /*maxtags*/0
	},
	{
		/* Broken tagged queuing drive */ 
		{ T_DIRECT, SIP_MEDIA_FIXED, "CONNER", "CFP2107*", "*" },
		/*quirks*/0, /*mintags*/0, /*maxtags*/0
	},
	{
		/* This does not support other than LUN 0 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "VMware*", "*", "*" },
		CAM_QUIRK_NOLUNS, /*mintags*/2, /*maxtags*/255
	},
	{
		/*
		 * Broken tagged queuing drive.
		 * Submitted by:
		 * NAKAJI Hiroyuki <nakaji@zeisei.dpri.kyoto-u.ac.jp>
		 * in PR kern/9535
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, samsung, "WN34324U*", "*" },
		/*quirks*/0, /*mintags*/0, /*maxtags*/0
	},
        {
		/*
		 * Slow when tagged queueing is enabled. (1.5MB/sec versus
		 * 8MB/sec.)
		 * Submitted by: Andrew Gallatin <gallatin@cs.duke.edu>
		 * Best performance with these drives is achieved with
		 * tagged queueing turned off, and write caching turned on.
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, west_digital, "WDE*", "*" },
		/*quirks*/0, /*mintags*/0, /*maxtags*/0
        },
        {
		/*
		 * Slow when tagged queueing is enabled. (1.5MB/sec versus
		 * 8MB/sec.)
		 * Submitted by: Andrew Gallatin <gallatin@cs.duke.edu>
		 * Best performance with these drives is achieved with
		 * tagged queueing turned off, and write caching turned on.
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, west_digital, "ENTERPRISE", "*" },
		/*quirks*/0, /*mintags*/0, /*maxtags*/0
        },
	{
		/*
		 * Doesn't handle queue full condition correctly,
		 * so we need to limit maxtags to what the device
		 * can handle instead of determining this automatically.
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, samsung, "WN321010S*", "*" },
		/*quirks*/0, /*mintags*/2, /*maxtags*/32
	},
	{
		/* Really only one LUN */
		{ T_ENCLOSURE, SIP_MEDIA_FIXED, "SUN", "SENA", "*" },
		CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/* I can't believe we need a quirk for DPT volumes. */
		{ T_ANY, SIP_MEDIA_FIXED|SIP_MEDIA_REMOVABLE, "DPT", "*", "*" },
		CAM_QUIRK_NOSERIAL|CAM_QUIRK_NOLUNS,
		/*mintags*/0, /*maxtags*/255
	},
	{
		/*
		 * Many Sony CDROM drives don't like multi-LUN probing.
		 */
		{ T_CDROM, SIP_MEDIA_REMOVABLE, sony, "CD-ROM CDU*", "*" },
		CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/*
		 * This drive doesn't like multiple LUN probing.
		 * Submitted by:  Parag Patel <parag@cgt.com>
		 */
		{ T_WORM, SIP_MEDIA_REMOVABLE, sony, "CD-R   CDU9*", "*" },
		CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		{ T_WORM, SIP_MEDIA_REMOVABLE, "YAMAHA", "CDR100*", "*" },
		CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/*
		 * The 8200 doesn't like multi-lun probing, and probably
		 * don't like serial number requests either.
		 */
		{
			T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "EXABYTE",
			"EXB-8200*", "*"
		},
		CAM_QUIRK_NOSERIAL|CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/*
		 * Let's try the same as above, but for a drive that says
		 * it's an IPL-6860 but is actually an EXB 8200.
		 */
		{
			T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "EXABYTE",
			"IPL-6860*", "*"
		},
		CAM_QUIRK_NOSERIAL|CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/*
		 * These Hitachi drives don't like multi-lun probing.
		 * The PR submitter has a DK319H, but says that the Linux
		 * kernel has a similar work-around for the DK312 and DK314,
		 * so all DK31* drives are quirked here.
		 * PR:            misc/18793
		 * Submitted by:  Paul Haddad <paul@pth.com>
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "HITACHI", "DK31*", "*" },
		CAM_QUIRK_NOLUNS, /*mintags*/2, /*maxtags*/255
	},
	{
		/*
		 * The Hitachi CJ series with J8A8 firmware apparantly has
		 * problems with tagged commands.
		 * PR: 23536
		 * Reported by: amagai@nue.org
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "HITACHI", "DK32CJ*", "J8A8" },
		CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/*
		 * These are the large storage arrays.
		 * Submitted by:  William Carrel <william.carrel@infospace.com>
		 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "HITACHI", "OPEN*", "*" },
		CAM_QUIRK_HILUNS, 2, 1024
	},
	{
		/*
		 * This old revision of the TDC3600 is also SCSI-1, and
		 * hangs upon serial number probing.
		 */
		{
			T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "TANDBERG",
			" TDC 3600", "U07:"
		},
		CAM_QUIRK_NOSERIAL, /*mintags*/0, /*maxtags*/0
	},
	{
		/*
		 * Maxtor Personal Storage 3000XT (Firewire)
		 * hangs upon serial number probing.
		 */
		{
			T_DIRECT, SIP_MEDIA_FIXED, "Maxtor",
			"1394 storage", "*"
		},
		CAM_QUIRK_NOSERIAL, /*mintags*/0, /*maxtags*/0
	},
	{
		/*
		 * Would repond to all LUNs if asked for.
		 */
		{
			T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "CALIPER",
			"CP150", "*"
		},
		CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/*
		 * Would repond to all LUNs if asked for.
		 */
		{
			T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "KENNEDY",
			"96X2*", "*"
		},
		CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/* Submitted by: Matthew Dodd <winter@jurai.net> */
		{ T_PROCESSOR, SIP_MEDIA_FIXED, "Cabletrn", "EA41*", "*" },
		CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/* Submitted by: Matthew Dodd <winter@jurai.net> */
		{ T_PROCESSOR, SIP_MEDIA_FIXED, "CABLETRN", "EA41*", "*" },
		CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/* TeraSolutions special settings for TRC-22 RAID */
		{ T_DIRECT, SIP_MEDIA_FIXED, "TERASOLU", "TRC-22", "*" },
		  /*quirks*/0, /*mintags*/55, /*maxtags*/255
	},
	{
		/* Veritas Storage Appliance */
		{ T_DIRECT, SIP_MEDIA_FIXED, "VERITAS", "*", "*" },
		  CAM_QUIRK_HILUNS, /*mintags*/2, /*maxtags*/1024
	},
	{
		/*
		 * Would respond to all LUNs.  Device type and removable
		 * flag are jumper-selectable.
		 */
		{ T_ANY, SIP_MEDIA_REMOVABLE|SIP_MEDIA_FIXED, "MaxOptix",
		  "Tahiti 1", "*"
		},
		CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/* EasyRAID E5A aka. areca ARC-6010 */
		{ T_DIRECT, SIP_MEDIA_FIXED, "easyRAID", "*", "*" },
		  CAM_QUIRK_NOHILUNS, /*mintags*/2, /*maxtags*/255
	},
	{
		{ T_ENCLOSURE, SIP_MEDIA_FIXED, "DP", "BACKPLANE", "*" },
		CAM_QUIRK_NOLUNS, /*mintags*/0, /*maxtags*/0
	},
	{
		/*
		 * Western Digital My Book 250GB (USB)
		 * hangs upon serial number probing.
		 * PR: 107495
		 */
		{
			T_DIRECT, SIP_MEDIA_FIXED, "WD",
			"2500JB External", "*"
		},
		CAM_QUIRK_NOSERIAL, /*mintags*/0, /*maxtags*/0
	},
	{
		/* Default tagged queuing parameters for all devices */
		{
		  T_ANY, SIP_MEDIA_REMOVABLE|SIP_MEDIA_FIXED,
		  /*vendor*/"*", /*product*/"*", /*revision*/"*"
		},
		/*quirks*/0, /*mintags*/2, /*maxtags*/255
	},
};

static const int xpt_quirk_table_size =
	sizeof(xpt_quirk_table) / sizeof(*xpt_quirk_table);

typedef enum {
	DM_RET_COPY		= 0x01,
	DM_RET_FLAG_MASK	= 0x0f,
	DM_RET_NONE		= 0x00,
	DM_RET_STOP		= 0x10,
	DM_RET_DESCEND		= 0x20,
	DM_RET_ERROR		= 0x30,
	DM_RET_ACTION_MASK	= 0xf0
} dev_match_ret;

typedef enum {
	XPT_DEPTH_BUS,
	XPT_DEPTH_TARGET,
	XPT_DEPTH_DEVICE,
	XPT_DEPTH_PERIPH
} xpt_traverse_depth;

struct xpt_traverse_config {
	xpt_traverse_depth	depth;
	void			*tr_func;
	void			*tr_arg;
};

typedef	int	xpt_busfunc_t (struct cam_eb *bus, void *arg);
typedef	int	xpt_targetfunc_t (struct cam_et *target, void *arg);
typedef	int	xpt_devicefunc_t (struct cam_ed *device, void *arg);
typedef	int	xpt_periphfunc_t (struct cam_periph *periph, void *arg);
typedef int	xpt_pdrvfunc_t (struct periph_driver **pdrv, void *arg);

/* Transport layer configuration information */
static struct xpt_softc xsoftc;

/* Queues for our software interrupt handler */
typedef TAILQ_HEAD(cam_isrq, ccb_hdr) cam_isrq_t;
typedef TAILQ_HEAD(cam_simq, cam_sim) cam_simq_t;
static cam_simq_t cam_simq;
static struct mtx cam_simq_lock;

/* Pointers to software interrupt handlers */
static void *cambio_ih;

struct cam_periph *xpt_periph;

static periph_init_t xpt_periph_init;

static periph_init_t probe_periph_init;

static struct periph_driver xpt_driver =
{
	xpt_periph_init, "xpt",
	TAILQ_HEAD_INITIALIZER(xpt_driver.units)
};

static struct periph_driver probe_driver =
{
	probe_periph_init, "probe",
	TAILQ_HEAD_INITIALIZER(probe_driver.units)
};

PERIPHDRIVER_DECLARE(xpt, xpt_driver);
PERIPHDRIVER_DECLARE(probe, probe_driver);


static d_open_t xptopen;
static d_close_t xptclose;
static d_ioctl_t xptioctl;

static struct cdevsw xpt_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_open =	xptopen,
	.d_close =	xptclose,
	.d_ioctl =	xptioctl,
	.d_name =	"xpt",
};


static void dead_sim_action(struct cam_sim *sim, union ccb *ccb);
static void dead_sim_poll(struct cam_sim *sim);

/* Dummy SIM that is used when the real one has gone. */
static struct cam_sim cam_dead_sim = {
	.sim_action =	dead_sim_action,
	.sim_poll =	dead_sim_poll,
	.sim_name =	"dead_sim",
};

#define SIM_DEAD(sim)	((sim) == &cam_dead_sim)


/* Storage for debugging datastructures */
#ifdef	CAMDEBUG
struct cam_path *cam_dpath;
u_int32_t cam_dflags;
u_int32_t cam_debug_delay;
#endif

#if defined(CAM_DEBUG_FLAGS) && !defined(CAMDEBUG)
#error "You must have options CAMDEBUG to use options CAM_DEBUG_FLAGS"
#endif

/*
 * In order to enable the CAM_DEBUG_* options, the user must have CAMDEBUG
 * enabled.  Also, the user must have either none, or all of CAM_DEBUG_BUS,
 * CAM_DEBUG_TARGET, and CAM_DEBUG_LUN specified.
 */
#if defined(CAM_DEBUG_BUS) || defined(CAM_DEBUG_TARGET) \
    || defined(CAM_DEBUG_LUN)
#ifdef CAMDEBUG
#if !defined(CAM_DEBUG_BUS) || !defined(CAM_DEBUG_TARGET) \
    || !defined(CAM_DEBUG_LUN)
#error "You must define all or none of CAM_DEBUG_BUS, CAM_DEBUG_TARGET \
        and CAM_DEBUG_LUN"
#endif /* !CAM_DEBUG_BUS || !CAM_DEBUG_TARGET || !CAM_DEBUG_LUN */
#else /* !CAMDEBUG */
#error "You must use options CAMDEBUG if you use the CAM_DEBUG_* options"
#endif /* CAMDEBUG */
#endif /* CAM_DEBUG_BUS || CAM_DEBUG_TARGET || CAM_DEBUG_LUN */

/* Our boot-time initialization hook */
static int cam_module_event_handler(module_t, int /*modeventtype_t*/, void *);

static moduledata_t cam_moduledata = {
	"cam",
	cam_module_event_handler,
	NULL
};

static int	xpt_init(void *);

DECLARE_MODULE(cam, cam_moduledata, SI_SUB_CONFIGURE, SI_ORDER_SECOND);
MODULE_VERSION(cam, 1);


static cam_status	xpt_compile_path(struct cam_path *new_path,
					 struct cam_periph *perph,
					 path_id_t path_id,
					 target_id_t target_id,
					 lun_id_t lun_id);

static void		xpt_release_path(struct cam_path *path);

static void		xpt_async_bcast(struct async_list *async_head,
					u_int32_t async_code,
					struct cam_path *path,
					void *async_arg);
static void		xpt_dev_async(u_int32_t async_code,
				      struct cam_eb *bus,
				      struct cam_et *target,
				      struct cam_ed *device,
				      void *async_arg);
static path_id_t xptnextfreepathid(void);
static path_id_t xptpathid(const char *sim_name, int sim_unit, int sim_bus);
static union ccb *xpt_get_ccb(struct cam_ed *device);
static int	 xpt_schedule_dev(struct camq *queue, cam_pinfo *dev_pinfo,
				  u_int32_t new_priority);
static void	 xpt_run_dev_allocq(struct cam_eb *bus);
static void	 xpt_run_dev_sendq(struct cam_eb *bus);
static timeout_t xpt_release_devq_timeout;
static void	 xpt_release_simq_timeout(void *arg) __unused;
static void	 xpt_release_bus(struct cam_eb *bus);
static void	 xpt_release_devq_device(struct cam_ed *dev, u_int count,
					 int run_queue);
static struct cam_et*
		 xpt_alloc_target(struct cam_eb *bus, target_id_t target_id);
static void	 xpt_release_target(struct cam_eb *bus, struct cam_et *target);
static struct cam_ed*
		 xpt_alloc_device(struct cam_eb *bus, struct cam_et *target,
				  lun_id_t lun_id);
static void	 xpt_release_device(struct cam_eb *bus, struct cam_et *target,
				    struct cam_ed *device);
static u_int32_t xpt_dev_ccbq_resize(struct cam_path *path, int newopenings);
static struct cam_eb*
		 xpt_find_bus(path_id_t path_id);
static struct cam_et*
		 xpt_find_target(struct cam_eb *bus, target_id_t target_id);
static struct cam_ed*
		 xpt_find_device(struct cam_et *target, lun_id_t lun_id);
static void	 xpt_scan_bus(struct cam_periph *periph, union ccb *ccb);
static void	 xpt_scan_lun(struct cam_periph *periph,
			      struct cam_path *path, cam_flags flags,
			      union ccb *ccb);
static void	 xptscandone(struct cam_periph *periph, union ccb *done_ccb);
static xpt_busfunc_t	xptconfigbuscountfunc;
static xpt_busfunc_t	xptconfigfunc;
static void	 xpt_config(void *arg);
static xpt_devicefunc_t xptpassannouncefunc;
static void	 xpt_finishconfig(struct cam_periph *periph, union ccb *ccb);
static void	 xptaction(struct cam_sim *sim, union ccb *work_ccb);
static void	 xptpoll(struct cam_sim *sim);
static void	 camisr(void *);
static void	 camisr_runqueue(void *);
static dev_match_ret	xptbusmatch(struct dev_match_pattern *patterns,
				    u_int num_patterns, struct cam_eb *bus);
static dev_match_ret	xptdevicematch(struct dev_match_pattern *patterns,
				       u_int num_patterns,
				       struct cam_ed *device);
static dev_match_ret	xptperiphmatch(struct dev_match_pattern *patterns,
				       u_int num_patterns,
				       struct cam_periph *periph);
static xpt_busfunc_t	xptedtbusfunc;
static xpt_targetfunc_t	xptedttargetfunc;
static xpt_devicefunc_t	xptedtdevicefunc;
static xpt_periphfunc_t	xptedtperiphfunc;
static xpt_pdrvfunc_t	xptplistpdrvfunc;
static xpt_periphfunc_t	xptplistperiphfunc;
static int		xptedtmatch(struct ccb_dev_match *cdm);
static int		xptperiphlistmatch(struct ccb_dev_match *cdm);
static int		xptbustraverse(struct cam_eb *start_bus,
				       xpt_busfunc_t *tr_func, void *arg);
static int		xpttargettraverse(struct cam_eb *bus,
					  struct cam_et *start_target,
					  xpt_targetfunc_t *tr_func, void *arg);
static int		xptdevicetraverse(struct cam_et *target,
					  struct cam_ed *start_device,
					  xpt_devicefunc_t *tr_func, void *arg);
static int		xptperiphtraverse(struct cam_ed *device,
					  struct cam_periph *start_periph,
					  xpt_periphfunc_t *tr_func, void *arg);
static int		xptpdrvtraverse(struct periph_driver **start_pdrv,
					xpt_pdrvfunc_t *tr_func, void *arg);
static int		xptpdperiphtraverse(struct periph_driver **pdrv,
					    struct cam_periph *start_periph,
					    xpt_periphfunc_t *tr_func,
					    void *arg);
static xpt_busfunc_t	xptdefbusfunc;
static xpt_targetfunc_t	xptdeftargetfunc;
static xpt_devicefunc_t	xptdefdevicefunc;
static xpt_periphfunc_t	xptdefperiphfunc;
static int		xpt_for_all_busses(xpt_busfunc_t *tr_func, void *arg);
static int		xpt_for_all_devices(xpt_devicefunc_t *tr_func,
					    void *arg);
static xpt_devicefunc_t	xptsetasyncfunc;
static xpt_busfunc_t	xptsetasyncbusfunc;
static cam_status	xptregister(struct cam_periph *periph,
				    void *arg);
static cam_status	proberegister(struct cam_periph *periph,
				      void *arg);
static void	 probeschedule(struct cam_periph *probe_periph);
static void	 probestart(struct cam_periph *periph, union ccb *start_ccb);
static void	 proberequestdefaultnegotiation(struct cam_periph *periph);
static int       proberequestbackoff(struct cam_periph *periph,
				     struct cam_ed *device);
static void	 probedone(struct cam_periph *periph, union ccb *done_ccb);
static void	 probecleanup(struct cam_periph *periph);
static void	 xpt_find_quirk(struct cam_ed *device);
static void	 xpt_devise_transport(struct cam_path *path);
static void	 xpt_set_transfer_settings(struct ccb_trans_settings *cts,
					   struct cam_ed *device,
					   int async_update);
static void	 xpt_toggle_tags(struct cam_path *path);
static void	 xpt_start_tags(struct cam_path *path);
static __inline int xpt_schedule_dev_allocq(struct cam_eb *bus,
					    struct cam_ed *dev);
static __inline int xpt_schedule_dev_sendq(struct cam_eb *bus,
					   struct cam_ed *dev);
static __inline int periph_is_queued(struct cam_periph *periph);
static __inline int device_is_alloc_queued(struct cam_ed *device);
static __inline int device_is_send_queued(struct cam_ed *device);
static __inline int dev_allocq_is_runnable(struct cam_devq *devq);

static __inline int
xpt_schedule_dev_allocq(struct cam_eb *bus, struct cam_ed *dev)
{
	int retval;

	if (dev->ccbq.devq_openings > 0) {
		if ((dev->flags & CAM_DEV_RESIZE_QUEUE_NEEDED) != 0) {
			cam_ccbq_resize(&dev->ccbq,
					dev->ccbq.dev_openings
					+ dev->ccbq.dev_active);
			dev->flags &= ~CAM_DEV_RESIZE_QUEUE_NEEDED;
		}
		/*
		 * The priority of a device waiting for CCB resources
		 * is that of the the highest priority peripheral driver
		 * enqueued.
		 */
		retval = xpt_schedule_dev(&bus->sim->devq->alloc_queue,
					  &dev->alloc_ccb_entry.pinfo,
					  CAMQ_GET_HEAD(&dev->drvq)->priority);	
	} else {
		retval = 0;
	}

	return (retval);
}

static __inline int
xpt_schedule_dev_sendq(struct cam_eb *bus, struct cam_ed *dev)
{
	int	retval;

	if (dev->ccbq.dev_openings > 0) {
		/*
		 * The priority of a device waiting for controller
		 * resources is that of the the highest priority CCB
		 * enqueued.
		 */
		retval =
		    xpt_schedule_dev(&bus->sim->devq->send_queue,
				     &dev->send_ccb_entry.pinfo,
				     CAMQ_GET_HEAD(&dev->ccbq.queue)->priority);
	} else {
		retval = 0;
	}
	return (retval);
}

static __inline int
periph_is_queued(struct cam_periph *periph)
{
	return (periph->pinfo.index != CAM_UNQUEUED_INDEX);
}

static __inline int
device_is_alloc_queued(struct cam_ed *device)
{
	return (device->alloc_ccb_entry.pinfo.index != CAM_UNQUEUED_INDEX);
}

static __inline int
device_is_send_queued(struct cam_ed *device)
{
	return (device->send_ccb_entry.pinfo.index != CAM_UNQUEUED_INDEX);
}

static __inline int
dev_allocq_is_runnable(struct cam_devq *devq)
{
	/*
	 * Have work to do.
	 * Have space to do more work.
	 * Allowed to do work.
	 */
	return ((devq->alloc_queue.qfrozen_cnt == 0)
	     && (devq->alloc_queue.entries > 0)
	     && (devq->alloc_openings > 0));
}

static void
xpt_periph_init()
{
	make_dev(&xpt_cdevsw, 0, UID_ROOT, GID_OPERATOR, 0600, "xpt0");
}

static void
probe_periph_init()
{
}


static void
xptdone(struct cam_periph *periph, union ccb *done_ccb)
{
	/* Caller will release the CCB */
	wakeup(&done_ccb->ccb_h.cbfcnp);
}

static int
xptopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{

	/*
	 * Only allow read-write access.
	 */
	if (((flags & FWRITE) == 0) || ((flags & FREAD) == 0))
		return(EPERM);

	/*
	 * We don't allow nonblocking access.
	 */
	if ((flags & O_NONBLOCK) != 0) {
		printf("%s: can't do nonblocking access\n", devtoname(dev));
		return(ENODEV);
	}

	/* Mark ourselves open */
	mtx_lock(&xsoftc.xpt_lock);
	xsoftc.flags |= XPT_FLAG_OPEN;
	mtx_unlock(&xsoftc.xpt_lock);
	
	return(0);
}

static int
xptclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{

	/* Mark ourselves closed */
	mtx_lock(&xsoftc.xpt_lock);
	xsoftc.flags &= ~XPT_FLAG_OPEN;
	mtx_unlock(&xsoftc.xpt_lock);

	return(0);
}

/*
 * Don't automatically grab the xpt softc lock here even though this is going
 * through the xpt device.  The xpt device is really just a back door for
 * accessing other devices and SIMs, so the right thing to do is to grab
 * the appropriate SIM lock once the bus/SIM is located.
 */
static int
xptioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
	int error;

	error = 0;

	switch(cmd) {
	/*
	 * For the transport layer CAMIOCOMMAND ioctl, we really only want
	 * to accept CCB types that don't quite make sense to send through a
	 * passthrough driver. XPT_PATH_INQ is an exception to this, as stated
	 * in the CAM spec.
	 */
	case CAMIOCOMMAND: {
		union ccb *ccb;
		union ccb *inccb;
		struct cam_eb *bus;

		inccb = (union ccb *)addr;

		bus = xpt_find_bus(inccb->ccb_h.path_id);
		if (bus == NULL) {
			error = EINVAL;
			break;
		}

		switch(inccb->ccb_h.func_code) {
		case XPT_SCAN_BUS:
		case XPT_RESET_BUS:
			if ((inccb->ccb_h.target_id != CAM_TARGET_WILDCARD)
			 || (inccb->ccb_h.target_lun != CAM_LUN_WILDCARD)) {
				error = EINVAL;
				break;
			}
			/* FALLTHROUGH */
		case XPT_PATH_INQ:
		case XPT_ENG_INQ:
		case XPT_SCAN_LUN:

			ccb = xpt_alloc_ccb();

			CAM_SIM_LOCK(bus->sim);

			/*
			 * Create a path using the bus, target, and lun the
			 * user passed in.
			 */
			if (xpt_create_path(&ccb->ccb_h.path, xpt_periph,
					    inccb->ccb_h.path_id,
					    inccb->ccb_h.target_id,
					    inccb->ccb_h.target_lun) !=
					    CAM_REQ_CMP){
				error = EINVAL;
				CAM_SIM_UNLOCK(bus->sim);
				xpt_free_ccb(ccb);
				break;
			}
			/* Ensure all of our fields are correct */
			xpt_setup_ccb(&ccb->ccb_h, ccb->ccb_h.path,
				      inccb->ccb_h.pinfo.priority);
			xpt_merge_ccb(ccb, inccb);
			ccb->ccb_h.cbfcnp = xptdone;
			cam_periph_runccb(ccb, NULL, 0, 0, NULL);
			bcopy(ccb, inccb, sizeof(union ccb));
			xpt_free_path(ccb->ccb_h.path);
			xpt_free_ccb(ccb);
			CAM_SIM_UNLOCK(bus->sim);
			break;

		case XPT_DEBUG: {
			union ccb ccb;

			/*
			 * This is an immediate CCB, so it's okay to
			 * allocate it on the stack.
			 */

			CAM_SIM_LOCK(bus->sim);

			/*
			 * Create a path using the bus, target, and lun the
			 * user passed in.
			 */
			if (xpt_create_path(&ccb.ccb_h.path, xpt_periph,
					    inccb->ccb_h.path_id,
					    inccb->ccb_h.target_id,
					    inccb->ccb_h.target_lun) !=
					    CAM_REQ_CMP){
				error = EINVAL;
				break;
			}
			/* Ensure all of our fields are correct */
			xpt_setup_ccb(&ccb.ccb_h, ccb.ccb_h.path,
				      inccb->ccb_h.pinfo.priority);
			xpt_merge_ccb(&ccb, inccb);
			ccb.ccb_h.cbfcnp = xptdone;
			xpt_action(&ccb);
			CAM_SIM_UNLOCK(bus->sim);
			bcopy(&ccb, inccb, sizeof(union ccb));
			xpt_free_path(ccb.ccb_h.path);
			break;

		}
		case XPT_DEV_MATCH: {
			struct cam_periph_map_info mapinfo;
			struct cam_path *old_path;

			/*
			 * We can't deal with physical addresses for this
			 * type of transaction.
			 */
			if (inccb->ccb_h.flags & CAM_DATA_PHYS) {
				error = EINVAL;
				break;
			}

			/*
			 * Save this in case the caller had it set to
			 * something in particular.
			 */
			old_path = inccb->ccb_h.path;

			/*
			 * We really don't need a path for the matching
			 * code.  The path is needed because of the
			 * debugging statements in xpt_action().  They
			 * assume that the CCB has a valid path.
			 */
			inccb->ccb_h.path = xpt_periph->path;

			bzero(&mapinfo, sizeof(mapinfo));

			/*
			 * Map the pattern and match buffers into kernel
			 * virtual address space.
			 */
			error = cam_periph_mapmem(inccb, &mapinfo);

			if (error) {
				inccb->ccb_h.path = old_path;
				break;
			}

			/*
			 * This is an immediate CCB, we can send it on directly.
			 */
			xpt_action(inccb);

			/*
			 * Map the buffers back into user space.
			 */
			cam_periph_unmapmem(inccb, &mapinfo);

			inccb->ccb_h.path = old_path;

			error = 0;
			break;
		}
		default:
			error = ENOTSUP;
			break;
		}
		xpt_release_bus(bus);
		break;
	}
	/*
	 * This is the getpassthru ioctl. It takes a XPT_GDEVLIST ccb as input,
	 * with the periphal driver name and unit name filled in.  The other
	 * fields don't really matter as input.  The passthrough driver name
	 * ("pass"), and unit number are passed back in the ccb.  The current
	 * device generation number, and the index into the device peripheral
	 * driver list, and the status are also passed back.  Note that
	 * since we do everything in one pass, unlike the XPT_GDEVLIST ccb,
	 * we never return a status of CAM_GDEVLIST_LIST_CHANGED.  It is
	 * (or rather should be) impossible for the device peripheral driver
	 * list to change since we look at the whole thing in one pass, and
	 * we do it with lock protection.
	 * 
	 */
	case CAMGETPASSTHRU: {
		union ccb *ccb;
		struct cam_periph *periph;
		struct periph_driver **p_drv;
		char   *name;
		u_int unit;
		u_int cur_generation;
		int base_periph_found;
		int splbreaknum;

		ccb = (union ccb *)addr;
		unit = ccb->cgdl.unit_number;
		name = ccb->cgdl.periph_name;
		/*
		 * Every 100 devices, we want to drop our lock protection to
		 * give the software interrupt handler a chance to run.
		 * Most systems won't run into this check, but this should
		 * avoid starvation in the software interrupt handler in
		 * large systems.
		 */
		splbreaknum = 100;

		ccb = (union ccb *)addr;

		base_periph_found = 0;

		/*
		 * Sanity check -- make sure we don't get a null peripheral
		 * driver name.
		 */
		if (*ccb->cgdl.periph_name == '\0') {
			error = EINVAL;
			break;
		}

		/* Keep the list from changing while we traverse it */
		mtx_lock(&xsoftc.xpt_topo_lock);
ptstartover:
		cur_generation = xsoftc.xpt_generation;

		/* first find our driver in the list of drivers */
		for (p_drv = periph_drivers; *p_drv != NULL; p_drv++)
			if (strcmp((*p_drv)->driver_name, name) == 0)
				break;

		if (*p_drv == NULL) {
			mtx_unlock(&xsoftc.xpt_topo_lock);
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			ccb->cgdl.status = CAM_GDEVLIST_ERROR;
			*ccb->cgdl.periph_name = '\0';
			ccb->cgdl.unit_number = 0;
			error = ENOENT;
			break;
		}	

		/*
		 * Run through every peripheral instance of this driver
		 * and check to see whether it matches the unit passed
		 * in by the user.  If it does, get out of the loops and
		 * find the passthrough driver associated with that
		 * peripheral driver.
		 */
		for (periph = TAILQ_FIRST(&(*p_drv)->units); periph != NULL;
		     periph = TAILQ_NEXT(periph, unit_links)) {

			if (periph->unit_number == unit) {
				break;
			} else if (--splbreaknum == 0) {
				mtx_unlock(&xsoftc.xpt_topo_lock);
				mtx_lock(&xsoftc.xpt_topo_lock);
				splbreaknum = 100;
				if (cur_generation != xsoftc.xpt_generation)
				       goto ptstartover;
			}
		}
		/*
		 * If we found the peripheral driver that the user passed
		 * in, go through all of the peripheral drivers for that
		 * particular device and look for a passthrough driver.
		 */
		if (periph != NULL) {
			struct cam_ed *device;
			int i;

			base_periph_found = 1;
			device = periph->path->device;
			for (i = 0, periph = SLIST_FIRST(&device->periphs);
			     periph != NULL;
			     periph = SLIST_NEXT(periph, periph_links), i++) {
				/*
				 * Check to see whether we have a
				 * passthrough device or not. 
				 */
				if (strcmp(periph->periph_name, "pass") == 0) {
					/*
					 * Fill in the getdevlist fields.
					 */
					strcpy(ccb->cgdl.periph_name,
					       periph->periph_name);
					ccb->cgdl.unit_number =
						periph->unit_number;
					if (SLIST_NEXT(periph, periph_links))
						ccb->cgdl.status =
							CAM_GDEVLIST_MORE_DEVS;
					else
						ccb->cgdl.status =
						       CAM_GDEVLIST_LAST_DEVICE;
					ccb->cgdl.generation =
						device->generation;
					ccb->cgdl.index = i;
					/*
					 * Fill in some CCB header fields
					 * that the user may want.
					 */
					ccb->ccb_h.path_id =
						periph->path->bus->path_id;
					ccb->ccb_h.target_id =
						periph->path->target->target_id;
					ccb->ccb_h.target_lun =
						periph->path->device->lun_id;
					ccb->ccb_h.status = CAM_REQ_CMP;
					break;
				}
			}
		}

		/*
		 * If the periph is null here, one of two things has
		 * happened.  The first possibility is that we couldn't
		 * find the unit number of the particular peripheral driver
		 * that the user is asking about.  e.g. the user asks for
		 * the passthrough driver for "da11".  We find the list of
		 * "da" peripherals all right, but there is no unit 11.
		 * The other possibility is that we went through the list
		 * of peripheral drivers attached to the device structure,
		 * but didn't find one with the name "pass".  Either way,
		 * we return ENOENT, since we couldn't find something.
		 */
		if (periph == NULL) {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			ccb->cgdl.status = CAM_GDEVLIST_ERROR;
			*ccb->cgdl.periph_name = '\0';
			ccb->cgdl.unit_number = 0;
			error = ENOENT;
			/*
			 * It is unfortunate that this is even necessary,
			 * but there are many, many clueless users out there.
			 * If this is true, the user is looking for the
			 * passthrough driver, but doesn't have one in his
			 * kernel.
			 */
			if (base_periph_found == 1) {
				printf("xptioctl: pass driver is not in the "
				       "kernel\n");
				printf("xptioctl: put \"device pass0\" in "
				       "your kernel config file\n");
			}
		}
		mtx_unlock(&xsoftc.xpt_topo_lock);
		break;
		}
	default:
		error = ENOTTY;
		break;
	}

	return(error);
}

static int
cam_module_event_handler(module_t mod, int what, void *arg)
{
	int error;

	switch (what) {
	case MOD_LOAD:
		if ((error = xpt_init(NULL)) != 0)
			return (error);
		break;
	case MOD_UNLOAD:
		return EBUSY;
	default:
		return EOPNOTSUPP;
	}

	return 0;
}

/* thread to handle bus rescans */
static void
xpt_scanner_thread(void *dummy)
{
	cam_isrq_t	queue;
	union ccb	*ccb;
	struct cam_sim	*sim;

	for (;;) {
		/*
		 * Wait for a rescan request to come in.  When it does, splice
		 * it onto a queue from local storage so that the xpt lock
		 * doesn't need to be held while the requests are being
		 * processed.
		 */
		xpt_lock_buses();
		msleep(&xsoftc.ccb_scanq, &xsoftc.xpt_topo_lock, PRIBIO,
		    "ccb_scanq", 0);
		TAILQ_INIT(&queue);
		TAILQ_CONCAT(&queue, &xsoftc.ccb_scanq, sim_links.tqe);
		xpt_unlock_buses();

		while ((ccb = (union ccb *)TAILQ_FIRST(&queue)) != NULL) {
			TAILQ_REMOVE(&queue, &ccb->ccb_h, sim_links.tqe);

			sim = ccb->ccb_h.path->bus->sim;
			CAM_SIM_LOCK(sim);

			ccb->ccb_h.func_code = XPT_SCAN_BUS;
			ccb->ccb_h.cbfcnp = xptdone;
			xpt_setup_ccb(&ccb->ccb_h, ccb->ccb_h.path, 5);
			cam_periph_runccb(ccb, NULL, 0, 0, NULL);
			xpt_free_path(ccb->ccb_h.path);
			xpt_free_ccb(ccb);
			CAM_SIM_UNLOCK(sim);
		}
	}
}

void
xpt_rescan(union ccb *ccb)
{
	struct ccb_hdr *hdr;

	/*
	 * Don't make duplicate entries for the same paths.
	 */
	xpt_lock_buses();
	TAILQ_FOREACH(hdr, &xsoftc.ccb_scanq, sim_links.tqe) {
		if (xpt_path_comp(hdr->path, ccb->ccb_h.path) == 0) {
			xpt_unlock_buses();
			xpt_print(ccb->ccb_h.path, "rescan already queued\n");
			xpt_free_path(ccb->ccb_h.path);
			xpt_free_ccb(ccb);
			return;
		}
	}
	TAILQ_INSERT_TAIL(&xsoftc.ccb_scanq, &ccb->ccb_h, sim_links.tqe);
	wakeup(&xsoftc.ccb_scanq);
	xpt_unlock_buses();
}

/* Functions accessed by the peripheral drivers */
static int
xpt_init(void *dummy)
{
	struct cam_sim *xpt_sim;
	struct cam_path *path;
	struct cam_devq *devq;
	cam_status status;

	TAILQ_INIT(&xsoftc.xpt_busses);
	TAILQ_INIT(&cam_simq);
	TAILQ_INIT(&xsoftc.ccb_scanq);
	STAILQ_INIT(&xsoftc.highpowerq);
	xsoftc.num_highpower = CAM_MAX_HIGHPOWER;

	mtx_init(&cam_simq_lock, "CAM SIMQ lock", NULL, MTX_DEF);
	mtx_init(&xsoftc.xpt_lock, "XPT lock", NULL, MTX_DEF);
	mtx_init(&xsoftc.xpt_topo_lock, "XPT topology lock", NULL, MTX_DEF);

	/*
	 * The xpt layer is, itself, the equivelent of a SIM.
	 * Allow 16 ccbs in the ccb pool for it.  This should
	 * give decent parallelism when we probe busses and
	 * perform other XPT functions.
	 */
	devq = cam_simq_alloc(16);
	xpt_sim = cam_sim_alloc(xptaction,
				xptpoll,
				"xpt",
				/*softc*/NULL,
				/*unit*/0,
				/*mtx*/&xsoftc.xpt_lock,
				/*max_dev_transactions*/0,
				/*max_tagged_dev_transactions*/0,
				devq);
	if (xpt_sim == NULL)
		return (ENOMEM);

	xpt_sim->max_ccbs = 16;

	mtx_lock(&xsoftc.xpt_lock);
	if ((status = xpt_bus_register(xpt_sim, NULL, 0)) != CAM_SUCCESS) {
		printf("xpt_init: xpt_bus_register failed with status %#x,"
		       " failing attach\n", status);
		return (EINVAL);
	}

	/*
	 * Looking at the XPT from the SIM layer, the XPT is
	 * the equivelent of a peripheral driver.  Allocate
	 * a peripheral driver entry for us.
	 */
	if ((status = xpt_create_path(&path, NULL, CAM_XPT_PATH_ID,
				      CAM_TARGET_WILDCARD,
				      CAM_LUN_WILDCARD)) != CAM_REQ_CMP) {
		printf("xpt_init: xpt_create_path failed with status %#x,"
		       " failing attach\n", status);
		return (EINVAL);
	}

	cam_periph_alloc(xptregister, NULL, NULL, NULL, "xpt", CAM_PERIPH_BIO,
			 path, NULL, 0, xpt_sim);
	xpt_free_path(path);
	mtx_unlock(&xsoftc.xpt_lock);

	/*
	 * Register a callback for when interrupts are enabled.
	 */
	xsoftc.xpt_config_hook =
	    (struct intr_config_hook *)malloc(sizeof(struct intr_config_hook),
					      M_CAMXPT, M_NOWAIT | M_ZERO);
	if (xsoftc.xpt_config_hook == NULL) {
		printf("xpt_init: Cannot malloc config hook "
		       "- failing attach\n");
		return (ENOMEM);
	}

	xsoftc.xpt_config_hook->ich_func = xpt_config;
	if (config_intrhook_establish(xsoftc.xpt_config_hook) != 0) {
		free (xsoftc.xpt_config_hook, M_CAMXPT);
		printf("xpt_init: config_intrhook_establish failed "
		       "- failing attach\n");
	}

	/* fire up rescan thread */
	if (kproc_create(xpt_scanner_thread, NULL, NULL, 0, 0, "xpt_thrd")) {
		printf("xpt_init: failed to create rescan thread\n");
	}
	/* Install our software interrupt handlers */
	swi_add(NULL, "cambio", camisr, NULL, SWI_CAMBIO, INTR_MPSAFE, &cambio_ih);

	return (0);
}

static cam_status
xptregister(struct cam_periph *periph, void *arg)
{
	struct cam_sim *xpt_sim;

	if (periph == NULL) {
		printf("xptregister: periph was NULL!!\n");
		return(CAM_REQ_CMP_ERR);
	}

	xpt_sim = (struct cam_sim *)arg;
	xpt_sim->softc = periph;
	xpt_periph = periph;
	periph->softc = NULL;

	return(CAM_REQ_CMP);
}

int32_t
xpt_add_periph(struct cam_periph *periph)
{
	struct cam_ed *device;
	int32_t	 status;
	struct periph_list *periph_head;

	mtx_assert(periph->sim->mtx, MA_OWNED);

	device = periph->path->device;

	periph_head = &device->periphs;

	status = CAM_REQ_CMP;

	if (device != NULL) {
		/*
		 * Make room for this peripheral
		 * so it will fit in the queue
		 * when it's scheduled to run
		 */
		status = camq_resize(&device->drvq,
				     device->drvq.array_size + 1);

		device->generation++;

		SLIST_INSERT_HEAD(periph_head, periph, periph_links);
	}

	mtx_lock(&xsoftc.xpt_topo_lock);
	xsoftc.xpt_generation++;
	mtx_unlock(&xsoftc.xpt_topo_lock);

	return (status);
}

void
xpt_remove_periph(struct cam_periph *periph)
{
	struct cam_ed *device;

	mtx_assert(periph->sim->mtx, MA_OWNED);

	device = periph->path->device;

	if (device != NULL) {
		struct periph_list *periph_head;

		periph_head = &device->periphs;
		
		/* Release the slot for this peripheral */
		camq_resize(&device->drvq, device->drvq.array_size - 1);

		device->generation++;

		SLIST_REMOVE(periph_head, periph, cam_periph, periph_links);
	}

	mtx_lock(&xsoftc.xpt_topo_lock);
	xsoftc.xpt_generation++;
	mtx_unlock(&xsoftc.xpt_topo_lock);
}


void
xpt_announce_periph(struct cam_periph *periph, char *announce_string)
{
	struct	ccb_pathinq cpi;
	struct	ccb_trans_settings cts;
	struct	cam_path *path;
	u_int	speed;
	u_int	freq;
	u_int	mb;

	mtx_assert(periph->sim->mtx, MA_OWNED);

	path = periph->path;
	/*
	 * To ensure that this is printed in one piece,
	 * mask out CAM interrupts.
	 */
	printf("%s%d at %s%d bus %d target %d lun %d\n",
	       periph->periph_name, periph->unit_number,
	       path->bus->sim->sim_name,
	       path->bus->sim->unit_number,
	       path->bus->sim->bus_id,
	       path->target->target_id,
	       path->device->lun_id);
	printf("%s%d: ", periph->periph_name, periph->unit_number);
	scsi_print_inquiry(&path->device->inq_data);
	if (bootverbose && path->device->serial_num_len > 0) {
		/* Don't wrap the screen  - print only the first 60 chars */
		printf("%s%d: Serial Number %.60s\n", periph->periph_name,
		       periph->unit_number, path->device->serial_num);
	}
	xpt_setup_ccb(&cts.ccb_h, path, /*priority*/1);
	cts.ccb_h.func_code = XPT_GET_TRAN_SETTINGS;
	cts.type = CTS_TYPE_CURRENT_SETTINGS;
	xpt_action((union ccb*)&cts);
	if ((cts.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		return;
	}

	/* Ask the SIM for its base transfer speed */
	xpt_setup_ccb(&cpi.ccb_h, path, /*priority*/1);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);

	speed = cpi.base_transfer_speed;
	freq = 0;
	if (cts.ccb_h.status == CAM_REQ_CMP && cts.transport == XPORT_SPI) {
		struct	ccb_trans_settings_spi *spi;

		spi = &cts.xport_specific.spi;
		if ((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) != 0
		  && spi->sync_offset != 0) {
			freq = scsi_calc_syncsrate(spi->sync_period);
			speed = freq;
		}

		if ((spi->valid & CTS_SPI_VALID_BUS_WIDTH) != 0)
			speed *= (0x01 << spi->bus_width);
	}

	if (cts.ccb_h.status == CAM_REQ_CMP && cts.transport == XPORT_FC) {
		struct	ccb_trans_settings_fc *fc = &cts.xport_specific.fc;
		if (fc->valid & CTS_FC_VALID_SPEED) {
			speed = fc->bitrate;
		}
	}

	if (cts.ccb_h.status == CAM_REQ_CMP && cts.transport == XPORT_SAS) {
		struct	ccb_trans_settings_sas *sas = &cts.xport_specific.sas;
		if (sas->valid & CTS_SAS_VALID_SPEED) {
			speed = sas->bitrate;
		}
	}

	mb = speed / 1000;
	if (mb > 0)
		printf("%s%d: %d.%03dMB/s transfers",
		       periph->periph_name, periph->unit_number,
		       mb, speed % 1000);
	else
		printf("%s%d: %dKB/s transfers", periph->periph_name,
		       periph->unit_number, speed);
	/* Report additional information about SPI connections */
	if (cts.ccb_h.status == CAM_REQ_CMP && cts.transport == XPORT_SPI) {
		struct	ccb_trans_settings_spi *spi;

		spi = &cts.xport_specific.spi;
		if (freq != 0) {
			printf(" (%d.%03dMHz%s, offset %d", freq / 1000,
			       freq % 1000,
			       (spi->ppr_options & MSG_EXT_PPR_DT_REQ) != 0
			     ? " DT" : "",
			       spi->sync_offset);
		}
		if ((spi->valid & CTS_SPI_VALID_BUS_WIDTH) != 0
		 && spi->bus_width > 0) {
			if (freq != 0) {
				printf(", ");
			} else {
				printf(" (");
			}
			printf("%dbit)", 8 * (0x01 << spi->bus_width));
		} else if (freq != 0) {
			printf(")");
		}
	}
	if (cts.ccb_h.status == CAM_REQ_CMP && cts.transport == XPORT_FC) {
		struct	ccb_trans_settings_fc *fc;

		fc = &cts.xport_specific.fc;
		if (fc->valid & CTS_FC_VALID_WWNN)
			printf(" WWNN 0x%llx", (long long) fc->wwnn);
		if (fc->valid & CTS_FC_VALID_WWPN)
			printf(" WWPN 0x%llx", (long long) fc->wwpn);
		if (fc->valid & CTS_FC_VALID_PORT)
			printf(" PortID 0x%x", fc->port);
	}

	if (path->device->inq_flags & SID_CmdQue
	 || path->device->flags & CAM_DEV_TAG_AFTER_COUNT) {
		printf("\n%s%d: Command Queueing Enabled",
		       periph->periph_name, periph->unit_number);
	}
	printf("\n");

	/*
	 * We only want to print the caller's announce string if they've
	 * passed one in..
	 */
	if (announce_string != NULL)
		printf("%s%d: %s\n", periph->periph_name,
		       periph->unit_number, announce_string);
}

static dev_match_ret
xptbusmatch(struct dev_match_pattern *patterns, u_int num_patterns,
	    struct cam_eb *bus)
{
	dev_match_ret retval;
	int i;

	retval = DM_RET_NONE;

	/*
	 * If we aren't given something to match against, that's an error.
	 */
	if (bus == NULL)
		return(DM_RET_ERROR);

	/*
	 * If there are no match entries, then this bus matches no
	 * matter what.
	 */
	if ((patterns == NULL) || (num_patterns == 0))
		return(DM_RET_DESCEND | DM_RET_COPY);

	for (i = 0; i < num_patterns; i++) {
		struct bus_match_pattern *cur_pattern;

		/*
		 * If the pattern in question isn't for a bus node, we
		 * aren't interested.  However, we do indicate to the
		 * calling routine that we should continue descending the
		 * tree, since the user wants to match against lower-level
		 * EDT elements.
		 */
		if (patterns[i].type != DEV_MATCH_BUS) {
			if ((retval & DM_RET_ACTION_MASK) == DM_RET_NONE)
				retval |= DM_RET_DESCEND;
			continue;
		}

		cur_pattern = &patterns[i].pattern.bus_pattern;

		/*
		 * If they want to match any bus node, we give them any
		 * device node.
		 */
		if (cur_pattern->flags == BUS_MATCH_ANY) {
			/* set the copy flag */
			retval |= DM_RET_COPY;

			/*
			 * If we've already decided on an action, go ahead
			 * and return.
			 */
			if ((retval & DM_RET_ACTION_MASK) != DM_RET_NONE)
				return(retval);
		}

		/*
		 * Not sure why someone would do this...
		 */
		if (cur_pattern->flags == BUS_MATCH_NONE)
			continue;

		if (((cur_pattern->flags & BUS_MATCH_PATH) != 0)
		 && (cur_pattern->path_id != bus->path_id))
			continue;

		if (((cur_pattern->flags & BUS_MATCH_BUS_ID) != 0)
		 && (cur_pattern->bus_id != bus->sim->bus_id))
			continue;

		if (((cur_pattern->flags & BUS_MATCH_UNIT) != 0)
		 && (cur_pattern->unit_number != bus->sim->unit_number))
			continue;

		if (((cur_pattern->flags & BUS_MATCH_NAME) != 0)
		 && (strncmp(cur_pattern->dev_name, bus->sim->sim_name,
			     DEV_IDLEN) != 0))
			continue;

		/*
		 * If we get to this point, the user definitely wants 
		 * information on this bus.  So tell the caller to copy the
		 * data out.
		 */
		retval |= DM_RET_COPY;

		/*
		 * If the return action has been set to descend, then we
		 * know that we've already seen a non-bus matching
		 * expression, therefore we need to further descend the tree.
		 * This won't change by continuing around the loop, so we
		 * go ahead and return.  If we haven't seen a non-bus
		 * matching expression, we keep going around the loop until
		 * we exhaust the matching expressions.  We'll set the stop
		 * flag once we fall out of the loop.
		 */
		if ((retval & DM_RET_ACTION_MASK) == DM_RET_DESCEND)
			return(retval);
	}

	/*
	 * If the return action hasn't been set to descend yet, that means
	 * we haven't seen anything other than bus matching patterns.  So
	 * tell the caller to stop descending the tree -- the user doesn't
	 * want to match against lower level tree elements.
	 */
	if ((retval & DM_RET_ACTION_MASK) == DM_RET_NONE)
		retval |= DM_RET_STOP;

	return(retval);
}

static dev_match_ret
xptdevicematch(struct dev_match_pattern *patterns, u_int num_patterns,
	       struct cam_ed *device)
{
	dev_match_ret retval;
	int i;

	retval = DM_RET_NONE;

	/*
	 * If we aren't given something to match against, that's an error.
	 */
	if (device == NULL)
		return(DM_RET_ERROR);

	/*
	 * If there are no match entries, then this device matches no
	 * matter what.
	 */
	if ((patterns == NULL) || (num_patterns == 0))
		return(DM_RET_DESCEND | DM_RET_COPY);

	for (i = 0; i < num_patterns; i++) {
		struct device_match_pattern *cur_pattern;

		/*
		 * If the pattern in question isn't for a device node, we
		 * aren't interested.
		 */
		if (patterns[i].type != DEV_MATCH_DEVICE) {
			if ((patterns[i].type == DEV_MATCH_PERIPH)
			 && ((retval & DM_RET_ACTION_MASK) == DM_RET_NONE))
				retval |= DM_RET_DESCEND;
			continue;
		}

		cur_pattern = &patterns[i].pattern.device_pattern;

		/*
		 * If they want to match any device node, we give them any
		 * device node.
		 */
		if (cur_pattern->flags == DEV_MATCH_ANY) {
			/* set the copy flag */
			retval |= DM_RET_COPY;

			
			/*
			 * If we've already decided on an action, go ahead
			 * and return.
			 */
			if ((retval & DM_RET_ACTION_MASK) != DM_RET_NONE)
				return(retval);
		}

		/*
		 * Not sure why someone would do this...
		 */
		if (cur_pattern->flags == DEV_MATCH_NONE)
			continue;

		if (((cur_pattern->flags & DEV_MATCH_PATH) != 0)
		 && (cur_pattern->path_id != device->target->bus->path_id))
			continue;

		if (((cur_pattern->flags & DEV_MATCH_TARGET) != 0)
		 && (cur_pattern->target_id != device->target->target_id))
			continue;

		if (((cur_pattern->flags & DEV_MATCH_LUN) != 0)
		 && (cur_pattern->target_lun != device->lun_id))
			continue;

		if (((cur_pattern->flags & DEV_MATCH_INQUIRY) != 0)
		 && (cam_quirkmatch((caddr_t)&device->inq_data,
				    (caddr_t)&cur_pattern->inq_pat,
				    1, sizeof(cur_pattern->inq_pat),
				    scsi_static_inquiry_match) == NULL))
			continue;

		/*
		 * If we get to this point, the user definitely wants 
		 * information on this device.  So tell the caller to copy
		 * the data out.
		 */
		retval |= DM_RET_COPY;

		/*
		 * If the return action has been set to descend, then we
		 * know that we've already seen a peripheral matching
		 * expression, therefore we need to further descend the tree.
		 * This won't change by continuing around the loop, so we
		 * go ahead and return.  If we haven't seen a peripheral
		 * matching expression, we keep going around the loop until
		 * we exhaust the matching expressions.  We'll set the stop
		 * flag once we fall out of the loop.
		 */
		if ((retval & DM_RET_ACTION_MASK) == DM_RET_DESCEND)
			return(retval);
	}

	/*
	 * If the return action hasn't been set to descend yet, that means
	 * we haven't seen any peripheral matching patterns.  So tell the
	 * caller to stop descending the tree -- the user doesn't want to
	 * match against lower level tree elements.
	 */
	if ((retval & DM_RET_ACTION_MASK) == DM_RET_NONE)
		retval |= DM_RET_STOP;

	return(retval);
}

/*
 * Match a single peripheral against any number of match patterns.
 */
static dev_match_ret
xptperiphmatch(struct dev_match_pattern *patterns, u_int num_patterns,
	       struct cam_periph *periph)
{
	dev_match_ret retval;
	int i;

	/*
	 * If we aren't given something to match against, that's an error.
	 */
	if (periph == NULL)
		return(DM_RET_ERROR);

	/*
	 * If there are no match entries, then this peripheral matches no
	 * matter what.
	 */
	if ((patterns == NULL) || (num_patterns == 0))
		return(DM_RET_STOP | DM_RET_COPY);

	/*
	 * There aren't any nodes below a peripheral node, so there's no
	 * reason to descend the tree any further.
	 */
	retval = DM_RET_STOP;

	for (i = 0; i < num_patterns; i++) {
		struct periph_match_pattern *cur_pattern;

		/*
		 * If the pattern in question isn't for a peripheral, we
		 * aren't interested.
		 */
		if (patterns[i].type != DEV_MATCH_PERIPH)
			continue;

		cur_pattern = &patterns[i].pattern.periph_pattern;

		/*
		 * If they want to match on anything, then we will do so.
		 */
		if (cur_pattern->flags == PERIPH_MATCH_ANY) {
			/* set the copy flag */
			retval |= DM_RET_COPY;

			/*
			 * We've already set the return action to stop,
			 * since there are no nodes below peripherals in
			 * the tree.
			 */
			return(retval);
		}

		/*
		 * Not sure why someone would do this...
		 */
		if (cur_pattern->flags == PERIPH_MATCH_NONE)
			continue;

		if (((cur_pattern->flags & PERIPH_MATCH_PATH) != 0)
		 && (cur_pattern->path_id != periph->path->bus->path_id))
			continue;

		/*
		 * For the target and lun id's, we have to make sure the
		 * target and lun pointers aren't NULL.  The xpt peripheral
		 * has a wildcard target and device.
		 */
		if (((cur_pattern->flags & PERIPH_MATCH_TARGET) != 0)
		 && ((periph->path->target == NULL)
		 ||(cur_pattern->target_id != periph->path->target->target_id)))
			continue;

		if (((cur_pattern->flags & PERIPH_MATCH_LUN) != 0)
		 && ((periph->path->device == NULL)
		 || (cur_pattern->target_lun != periph->path->device->lun_id)))
			continue;

		if (((cur_pattern->flags & PERIPH_MATCH_UNIT) != 0)
		 && (cur_pattern->unit_number != periph->unit_number))
			continue;

		if (((cur_pattern->flags & PERIPH_MATCH_NAME) != 0)
		 && (strncmp(cur_pattern->periph_name, periph->periph_name,
			     DEV_IDLEN) != 0))
			continue;

		/*
		 * If we get to this point, the user definitely wants 
		 * information on this peripheral.  So tell the caller to
		 * copy the data out.
		 */
		retval |= DM_RET_COPY;

		/*
		 * The return action has already been set to stop, since
		 * peripherals don't have any nodes below them in the EDT.
		 */
		return(retval);
	}

	/*
	 * If we get to this point, the peripheral that was passed in
	 * doesn't match any of the patterns.
	 */
	return(retval);
}

static int
xptedtbusfunc(struct cam_eb *bus, void *arg)
{
	struct ccb_dev_match *cdm;
	dev_match_ret retval;

	cdm = (struct ccb_dev_match *)arg;

	/*
	 * If our position is for something deeper in the tree, that means
	 * that we've already seen this node.  So, we keep going down.
	 */
	if ((cdm->pos.position_type & CAM_DEV_POS_BUS)
	 && (cdm->pos.cookie.bus == bus)
	 && (cdm->pos.position_type & CAM_DEV_POS_TARGET)
	 && (cdm->pos.cookie.target != NULL))
		retval = DM_RET_DESCEND;
	else
		retval = xptbusmatch(cdm->patterns, cdm->num_patterns, bus);

	/*
	 * If we got an error, bail out of the search.
	 */
	if ((retval & DM_RET_ACTION_MASK) == DM_RET_ERROR) {
		cdm->status = CAM_DEV_MATCH_ERROR;
		return(0);
	}

	/*
	 * If the copy flag is set, copy this bus out.
	 */
	if (retval & DM_RET_COPY) {
		int spaceleft, j;

		spaceleft = cdm->match_buf_len - (cdm->num_matches *
			sizeof(struct dev_match_result));

		/*
		 * If we don't have enough space to put in another
		 * match result, save our position and tell the
		 * user there are more devices to check.
		 */
		if (spaceleft < sizeof(struct dev_match_result)) {
			bzero(&cdm->pos, sizeof(cdm->pos));
			cdm->pos.position_type = 
				CAM_DEV_POS_EDT | CAM_DEV_POS_BUS;

			cdm->pos.cookie.bus = bus;
			cdm->pos.generations[CAM_BUS_GENERATION]=
				xsoftc.bus_generation;
			cdm->status = CAM_DEV_MATCH_MORE;
			return(0);
		}
		j = cdm->num_matches;
		cdm->num_matches++;
		cdm->matches[j].type = DEV_MATCH_BUS;
		cdm->matches[j].result.bus_result.path_id = bus->path_id;
		cdm->matches[j].result.bus_result.bus_id = bus->sim->bus_id;
		cdm->matches[j].result.bus_result.unit_number =
			bus->sim->unit_number;
		strncpy(cdm->matches[j].result.bus_result.dev_name,
			bus->sim->sim_name, DEV_IDLEN);
	}

	/*
	 * If the user is only interested in busses, there's no
	 * reason to descend to the next level in the tree.
	 */
	if ((retval & DM_RET_ACTION_MASK) == DM_RET_STOP)
		return(1);

	/*
	 * If there is a target generation recorded, check it to
	 * make sure the target list hasn't changed.
	 */
	if ((cdm->pos.position_type & CAM_DEV_POS_BUS)
	 && (bus == cdm->pos.cookie.bus)
	 && (cdm->pos.position_type & CAM_DEV_POS_TARGET)
	 && (cdm->pos.generations[CAM_TARGET_GENERATION] != 0)
	 && (cdm->pos.generations[CAM_TARGET_GENERATION] !=
	     bus->generation)) {
		cdm->status = CAM_DEV_MATCH_LIST_CHANGED;
		return(0);
	}

	if ((cdm->pos.position_type & CAM_DEV_POS_BUS)
	 && (cdm->pos.cookie.bus == bus)
	 && (cdm->pos.position_type & CAM_DEV_POS_TARGET)
	 && (cdm->pos.cookie.target != NULL))
		return(xpttargettraverse(bus,
					(struct cam_et *)cdm->pos.cookie.target,
					 xptedttargetfunc, arg));
	else
		return(xpttargettraverse(bus, NULL, xptedttargetfunc, arg));
}

static int
xptedttargetfunc(struct cam_et *target, void *arg)
{
	struct ccb_dev_match *cdm;

	cdm = (struct ccb_dev_match *)arg;

	/*
	 * If there is a device list generation recorded, check it to
	 * make sure the device list hasn't changed.
	 */
	if ((cdm->pos.position_type & CAM_DEV_POS_BUS)
	 && (cdm->pos.cookie.bus == target->bus)
	 && (cdm->pos.position_type & CAM_DEV_POS_TARGET)
	 && (cdm->pos.cookie.target == target)
	 && (cdm->pos.position_type & CAM_DEV_POS_DEVICE)
	 && (cdm->pos.generations[CAM_DEV_GENERATION] != 0)
	 && (cdm->pos.generations[CAM_DEV_GENERATION] !=
	     target->generation)) {
		cdm->status = CAM_DEV_MATCH_LIST_CHANGED;
		return(0);
	}

	if ((cdm->pos.position_type & CAM_DEV_POS_BUS)
	 && (cdm->pos.cookie.bus == target->bus)
	 && (cdm->pos.position_type & CAM_DEV_POS_TARGET)
	 && (cdm->pos.cookie.target == target)
	 && (cdm->pos.position_type & CAM_DEV_POS_DEVICE)
	 && (cdm->pos.cookie.device != NULL))
		return(xptdevicetraverse(target,
					(struct cam_ed *)cdm->pos.cookie.device,
					 xptedtdevicefunc, arg));
	else
		return(xptdevicetraverse(target, NULL, xptedtdevicefunc, arg));
}

static int
xptedtdevicefunc(struct cam_ed *device, void *arg)
{

	struct ccb_dev_match *cdm;
	dev_match_ret retval;

	cdm = (struct ccb_dev_match *)arg;

	/*
	 * If our position is for something deeper in the tree, that means
	 * that we've already seen this node.  So, we keep going down.
	 */
	if ((cdm->pos.position_type & CAM_DEV_POS_DEVICE)
	 && (cdm->pos.cookie.device == device)
	 && (cdm->pos.position_type & CAM_DEV_POS_PERIPH)
	 && (cdm->pos.cookie.periph != NULL))
		retval = DM_RET_DESCEND;
	else
		retval = xptdevicematch(cdm->patterns, cdm->num_patterns,
					device);

	if ((retval & DM_RET_ACTION_MASK) == DM_RET_ERROR) {
		cdm->status = CAM_DEV_MATCH_ERROR;
		return(0);
	}

	/*
	 * If the copy flag is set, copy this device out.
	 */
	if (retval & DM_RET_COPY) {
		int spaceleft, j;

		spaceleft = cdm->match_buf_len - (cdm->num_matches *
			sizeof(struct dev_match_result));

		/*
		 * If we don't have enough space to put in another
		 * match result, save our position and tell the
		 * user there are more devices to check.
		 */
		if (spaceleft < sizeof(struct dev_match_result)) {
			bzero(&cdm->pos, sizeof(cdm->pos));
			cdm->pos.position_type = 
				CAM_DEV_POS_EDT | CAM_DEV_POS_BUS |
				CAM_DEV_POS_TARGET | CAM_DEV_POS_DEVICE;

			cdm->pos.cookie.bus = device->target->bus;
			cdm->pos.generations[CAM_BUS_GENERATION]=
				xsoftc.bus_generation;
			cdm->pos.cookie.target = device->target;
			cdm->pos.generations[CAM_TARGET_GENERATION] =
				device->target->bus->generation;
			cdm->pos.cookie.device = device;
			cdm->pos.generations[CAM_DEV_GENERATION] = 
				device->target->generation;
			cdm->status = CAM_DEV_MATCH_MORE;
			return(0);
		}
		j = cdm->num_matches;
		cdm->num_matches++;
		cdm->matches[j].type = DEV_MATCH_DEVICE;
		cdm->matches[j].result.device_result.path_id =
			device->target->bus->path_id;
		cdm->matches[j].result.device_result.target_id =
			device->target->target_id;
		cdm->matches[j].result.device_result.target_lun =
			device->lun_id;
		bcopy(&device->inq_data,
		      &cdm->matches[j].result.device_result.inq_data,
		      sizeof(struct scsi_inquiry_data));

		/* Let the user know whether this device is unconfigured */
		if (device->flags & CAM_DEV_UNCONFIGURED)
			cdm->matches[j].result.device_result.flags =
				DEV_RESULT_UNCONFIGURED;
		else
			cdm->matches[j].result.device_result.flags =
				DEV_RESULT_NOFLAG;
	}

	/*
	 * If the user isn't interested in peripherals, don't descend
	 * the tree any further.
	 */
	if ((retval & DM_RET_ACTION_MASK) == DM_RET_STOP)
		return(1);

	/*
	 * If there is a peripheral list generation recorded, make sure
	 * it hasn't changed.
	 */
	if ((cdm->pos.position_type & CAM_DEV_POS_BUS)
	 && (device->target->bus == cdm->pos.cookie.bus)
	 && (cdm->pos.position_type & CAM_DEV_POS_TARGET)
	 && (device->target == cdm->pos.cookie.target)
	 && (cdm->pos.position_type & CAM_DEV_POS_DEVICE)
	 && (device == cdm->pos.cookie.device)
	 && (cdm->pos.position_type & CAM_DEV_POS_PERIPH)
	 && (cdm->pos.generations[CAM_PERIPH_GENERATION] != 0)
	 && (cdm->pos.generations[CAM_PERIPH_GENERATION] !=
	     device->generation)){
		cdm->status = CAM_DEV_MATCH_LIST_CHANGED;
		return(0);
	}

	if ((cdm->pos.position_type & CAM_DEV_POS_BUS)
	 && (cdm->pos.cookie.bus == device->target->bus)
	 && (cdm->pos.position_type & CAM_DEV_POS_TARGET)
	 && (cdm->pos.cookie.target == device->target)
	 && (cdm->pos.position_type & CAM_DEV_POS_DEVICE)
	 && (cdm->pos.cookie.device == device)
	 && (cdm->pos.position_type & CAM_DEV_POS_PERIPH)
	 && (cdm->pos.cookie.periph != NULL))
		return(xptperiphtraverse(device,
				(struct cam_periph *)cdm->pos.cookie.periph,
				xptedtperiphfunc, arg));
	else
		return(xptperiphtraverse(device, NULL, xptedtperiphfunc, arg));
}

static int
xptedtperiphfunc(struct cam_periph *periph, void *arg)
{
	struct ccb_dev_match *cdm;
	dev_match_ret retval;

	cdm = (struct ccb_dev_match *)arg;

	retval = xptperiphmatch(cdm->patterns, cdm->num_patterns, periph);

	if ((retval & DM_RET_ACTION_MASK) == DM_RET_ERROR) {
		cdm->status = CAM_DEV_MATCH_ERROR;
		return(0);
	}

	/*
	 * If the copy flag is set, copy this peripheral out.
	 */
	if (retval & DM_RET_COPY) {
		int spaceleft, j;

		spaceleft = cdm->match_buf_len - (cdm->num_matches *
			sizeof(struct dev_match_result));

		/*
		 * If we don't have enough space to put in another
		 * match result, save our position and tell the
		 * user there are more devices to check.
		 */
		if (spaceleft < sizeof(struct dev_match_result)) {
			bzero(&cdm->pos, sizeof(cdm->pos));
			cdm->pos.position_type = 
				CAM_DEV_POS_EDT | CAM_DEV_POS_BUS |
				CAM_DEV_POS_TARGET | CAM_DEV_POS_DEVICE |
				CAM_DEV_POS_PERIPH;

			cdm->pos.cookie.bus = periph->path->bus;
			cdm->pos.generations[CAM_BUS_GENERATION]=
				xsoftc.bus_generation;
			cdm->pos.cookie.target = periph->path->target;
			cdm->pos.generations[CAM_TARGET_GENERATION] =
				periph->path->bus->generation;
			cdm->pos.cookie.device = periph->path->device;
			cdm->pos.generations[CAM_DEV_GENERATION] = 
				periph->path->target->generation;
			cdm->pos.cookie.periph = periph;
			cdm->pos.generations[CAM_PERIPH_GENERATION] =
				periph->path->device->generation;
			cdm->status = CAM_DEV_MATCH_MORE;
			return(0);
		}

		j = cdm->num_matches;
		cdm->num_matches++;
		cdm->matches[j].type = DEV_MATCH_PERIPH;
		cdm->matches[j].result.periph_result.path_id =
			periph->path->bus->path_id;
		cdm->matches[j].result.periph_result.target_id =
			periph->path->target->target_id;
		cdm->matches[j].result.periph_result.target_lun =
			periph->path->device->lun_id;
		cdm->matches[j].result.periph_result.unit_number =
			periph->unit_number;
		strncpy(cdm->matches[j].result.periph_result.periph_name,
			periph->periph_name, DEV_IDLEN);
	}

	return(1);
}

static int
xptedtmatch(struct ccb_dev_match *cdm)
{
	int ret;

	cdm->num_matches = 0;

	/*
	 * Check the bus list generation.  If it has changed, the user
	 * needs to reset everything and start over.
	 */
	if ((cdm->pos.position_type & CAM_DEV_POS_BUS)
	 && (cdm->pos.generations[CAM_BUS_GENERATION] != 0)
	 && (cdm->pos.generations[CAM_BUS_GENERATION] != xsoftc.bus_generation)) {
		cdm->status = CAM_DEV_MATCH_LIST_CHANGED;
		return(0);
	}

	if ((cdm->pos.position_type & CAM_DEV_POS_BUS)
	 && (cdm->pos.cookie.bus != NULL))
		ret = xptbustraverse((struct cam_eb *)cdm->pos.cookie.bus,
				     xptedtbusfunc, cdm);
	else
		ret = xptbustraverse(NULL, xptedtbusfunc, cdm);

	/*
	 * If we get back 0, that means that we had to stop before fully
	 * traversing the EDT.  It also means that one of the subroutines
	 * has set the status field to the proper value.  If we get back 1,
	 * we've fully traversed the EDT and copied out any matching entries.
	 */
	if (ret == 1)
		cdm->status = CAM_DEV_MATCH_LAST;

	return(ret);
}

static int
xptplistpdrvfunc(struct periph_driver **pdrv, void *arg)
{
	struct ccb_dev_match *cdm;

	cdm = (struct ccb_dev_match *)arg;

	if ((cdm->pos.position_type & CAM_DEV_POS_PDPTR)
	 && (cdm->pos.cookie.pdrv == pdrv)
	 && (cdm->pos.position_type & CAM_DEV_POS_PERIPH)
	 && (cdm->pos.generations[CAM_PERIPH_GENERATION] != 0)
	 && (cdm->pos.generations[CAM_PERIPH_GENERATION] !=
	     (*pdrv)->generation)) {
		cdm->status = CAM_DEV_MATCH_LIST_CHANGED;
		return(0);
	}

	if ((cdm->pos.position_type & CAM_DEV_POS_PDPTR)
	 && (cdm->pos.cookie.pdrv == pdrv)
	 && (cdm->pos.position_type & CAM_DEV_POS_PERIPH)
	 && (cdm->pos.cookie.periph != NULL))
		return(xptpdperiphtraverse(pdrv,
				(struct cam_periph *)cdm->pos.cookie.periph,
				xptplistperiphfunc, arg));
	else
		return(xptpdperiphtraverse(pdrv, NULL,xptplistperiphfunc, arg));
}

static int
xptplistperiphfunc(struct cam_periph *periph, void *arg)
{
	struct ccb_dev_match *cdm;
	dev_match_ret retval;

	cdm = (struct ccb_dev_match *)arg;

	retval = xptperiphmatch(cdm->patterns, cdm->num_patterns, periph);

	if ((retval & DM_RET_ACTION_MASK) == DM_RET_ERROR) {
		cdm->status = CAM_DEV_MATCH_ERROR;
		return(0);
	}

	/*
	 * If the copy flag is set, copy this peripheral out.
	 */
	if (retval & DM_RET_COPY) {
		int spaceleft, j;

		spaceleft = cdm->match_buf_len - (cdm->num_matches *
			sizeof(struct dev_match_result));

		/*
		 * If we don't have enough space to put in another
		 * match result, save our position and tell the
		 * user there are more devices to check.
		 */
		if (spaceleft < sizeof(struct dev_match_result)) {
			struct periph_driver **pdrv;

			pdrv = NULL;
			bzero(&cdm->pos, sizeof(cdm->pos));
			cdm->pos.position_type = 
				CAM_DEV_POS_PDRV | CAM_DEV_POS_PDPTR |
				CAM_DEV_POS_PERIPH;

			/*
			 * This may look a bit non-sensical, but it is
			 * actually quite logical.  There are very few
			 * peripheral drivers, and bloating every peripheral
			 * structure with a pointer back to its parent
			 * peripheral driver linker set entry would cost
			 * more in the long run than doing this quick lookup.
			 */
			for (pdrv = periph_drivers; *pdrv != NULL; pdrv++) {
				if (strcmp((*pdrv)->driver_name,
				    periph->periph_name) == 0)
					break;
			}

			if (*pdrv == NULL) {
				cdm->status = CAM_DEV_MATCH_ERROR;
				return(0);
			}

			cdm->pos.cookie.pdrv = pdrv;
			/*
			 * The periph generation slot does double duty, as
			 * does the periph pointer slot.  They are used for
			 * both edt and pdrv lookups and positioning.
			 */
			cdm->pos.cookie.periph = periph;
			cdm->pos.generations[CAM_PERIPH_GENERATION] =
				(*pdrv)->generation;
			cdm->status = CAM_DEV_MATCH_MORE;
			return(0);
		}

		j = cdm->num_matches;
		cdm->num_matches++;
		cdm->matches[j].type = DEV_MATCH_PERIPH;
		cdm->matches[j].result.periph_result.path_id =
			periph->path->bus->path_id;

		/*
		 * The transport layer peripheral doesn't have a target or
		 * lun.
		 */
		if (periph->path->target)
			cdm->matches[j].result.periph_result.target_id =
				periph->path->target->target_id;
		else
			cdm->matches[j].result.periph_result.target_id = -1;

		if (periph->path->device)
			cdm->matches[j].result.periph_result.target_lun =
				periph->path->device->lun_id;
		else
			cdm->matches[j].result.periph_result.target_lun = -1;

		cdm->matches[j].result.periph_result.unit_number =
			periph->unit_number;
		strncpy(cdm->matches[j].result.periph_result.periph_name,
			periph->periph_name, DEV_IDLEN);
	}

	return(1);
}

static int
xptperiphlistmatch(struct ccb_dev_match *cdm)
{
	int ret;

	cdm->num_matches = 0;

	/*
	 * At this point in the edt traversal function, we check the bus
	 * list generation to make sure that no busses have been added or
	 * removed since the user last sent a XPT_DEV_MATCH ccb through.
	 * For the peripheral driver list traversal function, however, we
	 * don't have to worry about new peripheral driver types coming or
	 * going; they're in a linker set, and therefore can't change
	 * without a recompile.
	 */

	if ((cdm->pos.position_type & CAM_DEV_POS_PDPTR)
	 && (cdm->pos.cookie.pdrv != NULL))
		ret = xptpdrvtraverse(
				(struct periph_driver **)cdm->pos.cookie.pdrv,
				xptplistpdrvfunc, cdm);
	else
		ret = xptpdrvtraverse(NULL, xptplistpdrvfunc, cdm);

	/*
	 * If we get back 0, that means that we had to stop before fully
	 * traversing the peripheral driver tree.  It also means that one of
	 * the subroutines has set the status field to the proper value.  If
	 * we get back 1, we've fully traversed the EDT and copied out any
	 * matching entries.
	 */
	if (ret == 1)
		cdm->status = CAM_DEV_MATCH_LAST;

	return(ret);
}

static int
xptbustraverse(struct cam_eb *start_bus, xpt_busfunc_t *tr_func, void *arg)
{
	struct cam_eb *bus, *next_bus;
	int retval;

	retval = 1;

	mtx_lock(&xsoftc.xpt_topo_lock);
	for (bus = (start_bus ? start_bus : TAILQ_FIRST(&xsoftc.xpt_busses));
	     bus != NULL;
	     bus = next_bus) {
		next_bus = TAILQ_NEXT(bus, links);

		mtx_unlock(&xsoftc.xpt_topo_lock);
		CAM_SIM_LOCK(bus->sim);
		retval = tr_func(bus, arg);
		CAM_SIM_UNLOCK(bus->sim);
		if (retval == 0)
			return(retval);
		mtx_lock(&xsoftc.xpt_topo_lock);
	}
	mtx_unlock(&xsoftc.xpt_topo_lock);

	return(retval);
}

static int
xpttargettraverse(struct cam_eb *bus, struct cam_et *start_target,
		  xpt_targetfunc_t *tr_func, void *arg)
{
	struct cam_et *target, *next_target;
	int retval;

	retval = 1;
	for (target = (start_target ? start_target :
		       TAILQ_FIRST(&bus->et_entries));
	     target != NULL; target = next_target) {

		next_target = TAILQ_NEXT(target, links);

		retval = tr_func(target, arg);

		if (retval == 0)
			return(retval);
	}

	return(retval);
}

static int
xptdevicetraverse(struct cam_et *target, struct cam_ed *start_device,
		  xpt_devicefunc_t *tr_func, void *arg)
{
	struct cam_ed *device, *next_device;
	int retval;

	retval = 1;
	for (device = (start_device ? start_device :
		       TAILQ_FIRST(&target->ed_entries));
	     device != NULL;
	     device = next_device) {

		next_device = TAILQ_NEXT(device, links);

		retval = tr_func(device, arg);

		if (retval == 0)
			return(retval);
	}

	return(retval);
}

static int
xptperiphtraverse(struct cam_ed *device, struct cam_periph *start_periph,
		  xpt_periphfunc_t *tr_func, void *arg)
{
	struct cam_periph *periph, *next_periph;
	int retval;

	retval = 1;

	for (periph = (start_periph ? start_periph :
		       SLIST_FIRST(&device->periphs));
	     periph != NULL;
	     periph = next_periph) {

		next_periph = SLIST_NEXT(periph, periph_links);

		retval = tr_func(periph, arg);
		if (retval == 0)
			return(retval);
	}

	return(retval);
}

static int
xptpdrvtraverse(struct periph_driver **start_pdrv,
		xpt_pdrvfunc_t *tr_func, void *arg)
{
	struct periph_driver **pdrv;
	int retval;

	retval = 1;

	/*
	 * We don't traverse the peripheral driver list like we do the
	 * other lists, because it is a linker set, and therefore cannot be
	 * changed during runtime.  If the peripheral driver list is ever
	 * re-done to be something other than a linker set (i.e. it can
	 * change while the system is running), the list traversal should
	 * be modified to work like the other traversal functions.
	 */
	for (pdrv = (start_pdrv ? start_pdrv : periph_drivers);
	     *pdrv != NULL; pdrv++) {
		retval = tr_func(pdrv, arg);

		if (retval == 0)
			return(retval);
	}

	return(retval);
}

static int
xptpdperiphtraverse(struct periph_driver **pdrv,
		    struct cam_periph *start_periph,
		    xpt_periphfunc_t *tr_func, void *arg)
{
	struct cam_periph *periph, *next_periph;
	int retval;

	retval = 1;

	for (periph = (start_periph ? start_periph :
	     TAILQ_FIRST(&(*pdrv)->units)); periph != NULL;
	     periph = next_periph) {

		next_periph = TAILQ_NEXT(periph, unit_links);

		retval = tr_func(periph, arg);
		if (retval == 0)
			return(retval);
	}
	return(retval);
}

static int
xptdefbusfunc(struct cam_eb *bus, void *arg)
{
	struct xpt_traverse_config *tr_config;

	tr_config = (struct xpt_traverse_config *)arg;

	if (tr_config->depth == XPT_DEPTH_BUS) {
		xpt_busfunc_t *tr_func;

		tr_func = (xpt_busfunc_t *)tr_config->tr_func;

		return(tr_func(bus, tr_config->tr_arg));
	} else
		return(xpttargettraverse(bus, NULL, xptdeftargetfunc, arg));
}

static int
xptdeftargetfunc(struct cam_et *target, void *arg)
{
	struct xpt_traverse_config *tr_config;

	tr_config = (struct xpt_traverse_config *)arg;

	if (tr_config->depth == XPT_DEPTH_TARGET) {
		xpt_targetfunc_t *tr_func;

		tr_func = (xpt_targetfunc_t *)tr_config->tr_func;

		return(tr_func(target, tr_config->tr_arg));
	} else
		return(xptdevicetraverse(target, NULL, xptdefdevicefunc, arg));
}

static int
xptdefdevicefunc(struct cam_ed *device, void *arg)
{
	struct xpt_traverse_config *tr_config;

	tr_config = (struct xpt_traverse_config *)arg;

	if (tr_config->depth == XPT_DEPTH_DEVICE) {
		xpt_devicefunc_t *tr_func;

		tr_func = (xpt_devicefunc_t *)tr_config->tr_func;

		return(tr_func(device, tr_config->tr_arg));
	} else
		return(xptperiphtraverse(device, NULL, xptdefperiphfunc, arg));
}

static int
xptdefperiphfunc(struct cam_periph *periph, void *arg)
{
	struct xpt_traverse_config *tr_config;
	xpt_periphfunc_t *tr_func;

	tr_config = (struct xpt_traverse_config *)arg;

	tr_func = (xpt_periphfunc_t *)tr_config->tr_func;

	/*
	 * Unlike the other default functions, we don't check for depth
	 * here.  The peripheral driver level is the last level in the EDT,
	 * so if we're here, we should execute the function in question.
	 */
	return(tr_func(periph, tr_config->tr_arg));
}

/*
 * Execute the given function for every bus in the EDT.
 */
static int
xpt_for_all_busses(xpt_busfunc_t *tr_func, void *arg)
{
	struct xpt_traverse_config tr_config;

	tr_config.depth = XPT_DEPTH_BUS;
	tr_config.tr_func = tr_func;
	tr_config.tr_arg = arg;

	return(xptbustraverse(NULL, xptdefbusfunc, &tr_config));
}

/*
 * Execute the given function for every device in the EDT.
 */
static int
xpt_for_all_devices(xpt_devicefunc_t *tr_func, void *arg)
{
	struct xpt_traverse_config tr_config;

	tr_config.depth = XPT_DEPTH_DEVICE;
	tr_config.tr_func = tr_func;
	tr_config.tr_arg = arg;

	return(xptbustraverse(NULL, xptdefbusfunc, &tr_config));
}

static int
xptsetasyncfunc(struct cam_ed *device, void *arg)
{
	struct cam_path path;
	struct ccb_getdev cgd;
	struct async_node *cur_entry;

	cur_entry = (struct async_node *)arg;

	/*
	 * Don't report unconfigured devices (Wildcard devs,
	 * devices only for target mode, device instances
	 * that have been invalidated but are waiting for
	 * their last reference count to be released).
	 */
	if ((device->flags & CAM_DEV_UNCONFIGURED) != 0)
		return (1);

	xpt_compile_path(&path,
			 NULL,
			 device->target->bus->path_id,
			 device->target->target_id,
			 device->lun_id);
	xpt_setup_ccb(&cgd.ccb_h, &path, /*priority*/1);
	cgd.ccb_h.func_code = XPT_GDEV_TYPE;
	xpt_action((union ccb *)&cgd);
	cur_entry->callback(cur_entry->callback_arg,
			    AC_FOUND_DEVICE,
			    &path, &cgd);
	xpt_release_path(&path);

	return(1);
}

static int
xptsetasyncbusfunc(struct cam_eb *bus, void *arg)
{
	struct cam_path path;
	struct ccb_pathinq cpi;
	struct async_node *cur_entry;

	cur_entry = (struct async_node *)arg;

	xpt_compile_path(&path, /*periph*/NULL,
			 bus->sim->path_id,
			 CAM_TARGET_WILDCARD,
			 CAM_LUN_WILDCARD);
	xpt_setup_ccb(&cpi.ccb_h, &path, /*priority*/1);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);
	cur_entry->callback(cur_entry->callback_arg,
			    AC_PATH_REGISTERED,
			    &path, &cpi);
	xpt_release_path(&path);

	return(1);
}

static void
xpt_action_sasync_cb(void *context, int pending)
{
	struct async_node *cur_entry;
	struct xpt_task *task;
	uint32_t added;

	task = (struct xpt_task *)context;
	cur_entry = (struct async_node *)task->data1;
	added = task->data2;

	if ((added & AC_FOUND_DEVICE) != 0) {
		/*
		 * Get this peripheral up to date with all
		 * the currently existing devices.
		 */
		xpt_for_all_devices(xptsetasyncfunc, cur_entry);
	}
	if ((added & AC_PATH_REGISTERED) != 0) {
		/*
		 * Get this peripheral up to date with all
		 * the currently existing busses.
		 */
		xpt_for_all_busses(xptsetasyncbusfunc, cur_entry);
		}

	free(task, M_CAMXPT);
}

void
xpt_action(union ccb *start_ccb)
{

	CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_TRACE, ("xpt_action\n"));

	start_ccb->ccb_h.status = CAM_REQ_INPROG;

	switch (start_ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	{
		struct cam_ed *device;
#ifdef CAMDEBUG
		char cdb_str[(SCSI_MAX_CDBLEN * 3) + 1];
		struct cam_path *path;

		path = start_ccb->ccb_h.path;
#endif

		/*
		 * For the sake of compatibility with SCSI-1
		 * devices that may not understand the identify
		 * message, we include lun information in the
		 * second byte of all commands.  SCSI-1 specifies
		 * that luns are a 3 bit value and reserves only 3
		 * bits for lun information in the CDB.  Later
		 * revisions of the SCSI spec allow for more than 8
		 * luns, but have deprecated lun information in the
		 * CDB.  So, if the lun won't fit, we must omit.
		 *
		 * Also be aware that during initial probing for devices,
		 * the inquiry information is unknown but initialized to 0.
		 * This means that this code will be exercised while probing
		 * devices with an ANSI revision greater than 2.
		 */
		device = start_ccb->ccb_h.path->device;
		if (device->protocol_version <= SCSI_REV_2
		 && start_ccb->ccb_h.target_lun < 8
		 && (start_ccb->ccb_h.flags & CAM_CDB_POINTER) == 0) {

			start_ccb->csio.cdb_io.cdb_bytes[1] |=
			    start_ccb->ccb_h.target_lun << 5;
		}
		start_ccb->csio.scsi_status = SCSI_STATUS_OK;
		CAM_DEBUG(path, CAM_DEBUG_CDB,("%s. CDB: %s\n",
			  scsi_op_desc(start_ccb->csio.cdb_io.cdb_bytes[0],
			  	       &path->device->inq_data),
			  scsi_cdb_string(start_ccb->csio.cdb_io.cdb_bytes,
					  cdb_str, sizeof(cdb_str))));
	}
	/* FALLTHROUGH */
	case XPT_TARGET_IO:
	case XPT_CONT_TARGET_IO:
		start_ccb->csio.sense_resid = 0;
		start_ccb->csio.resid = 0;
		/* FALLTHROUGH */
	case XPT_RESET_DEV:
	case XPT_ENG_EXEC:
	{
		struct cam_path *path;
		struct cam_sim *sim;
		int runq;

		path = start_ccb->ccb_h.path;

		sim = path->bus->sim;
		if (SIM_DEAD(sim)) {
			/* The SIM has gone; just execute the CCB directly. */
			cam_ccbq_send_ccb(&path->device->ccbq, start_ccb);
			(*(sim->sim_action))(sim, start_ccb);
			break;
		}

		cam_ccbq_insert_ccb(&path->device->ccbq, start_ccb);
		if (path->device->qfrozen_cnt == 0)
			runq = xpt_schedule_dev_sendq(path->bus, path->device);
		else
			runq = 0;
		if (runq != 0)
			xpt_run_dev_sendq(path->bus);
		break;
	}
	case XPT_SET_TRAN_SETTINGS:
	{
		xpt_set_transfer_settings(&start_ccb->cts,
					  start_ccb->ccb_h.path->device,
					  /*async_update*/FALSE);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct cam_sim *sim;

		/* Filter out garbage */
		if (start_ccb->ccg.block_size == 0
		 || start_ccb->ccg.volume_size == 0) {
			start_ccb->ccg.cylinders = 0;
			start_ccb->ccg.heads = 0;
			start_ccb->ccg.secs_per_track = 0;
			start_ccb->ccb_h.status = CAM_REQ_CMP;
			break;
		}
#ifdef PC98
		/*
		 * In a PC-98 system, geometry translation depens on
		 * the "real" device geometry obtained from mode page 4.
		 * SCSI geometry translation is performed in the
		 * initialization routine of the SCSI BIOS and the result
		 * stored in host memory.  If the translation is available
		 * in host memory, use it.  If not, rely on the default
		 * translation the device driver performs.
		 */
		if (scsi_da_bios_params(&start_ccb->ccg) != 0) {
			start_ccb->ccb_h.status = CAM_REQ_CMP;
			break;
		}
#endif
		sim = start_ccb->ccb_h.path->bus->sim;
		(*(sim->sim_action))(sim, start_ccb);
		break;
	}
	case XPT_ABORT:
	{
		union ccb* abort_ccb;

		abort_ccb = start_ccb->cab.abort_ccb;
		if (XPT_FC_IS_DEV_QUEUED(abort_ccb)) {

			if (abort_ccb->ccb_h.pinfo.index >= 0) {
				struct cam_ccbq *ccbq;

				ccbq = &abort_ccb->ccb_h.path->device->ccbq;
				cam_ccbq_remove_ccb(ccbq, abort_ccb);
				abort_ccb->ccb_h.status =
				    CAM_REQ_ABORTED|CAM_DEV_QFRZN;
				xpt_freeze_devq(abort_ccb->ccb_h.path, 1);
				xpt_done(abort_ccb);
				start_ccb->ccb_h.status = CAM_REQ_CMP;
				break;
			}
			if (abort_ccb->ccb_h.pinfo.index == CAM_UNQUEUED_INDEX
			 && (abort_ccb->ccb_h.status & CAM_SIM_QUEUED) == 0) {
				/*
				 * We've caught this ccb en route to
				 * the SIM.  Flag it for abort and the
				 * SIM will do so just before starting
				 * real work on the CCB.
				 */
				abort_ccb->ccb_h.status =
				    CAM_REQ_ABORTED|CAM_DEV_QFRZN;
				xpt_freeze_devq(abort_ccb->ccb_h.path, 1);
				start_ccb->ccb_h.status = CAM_REQ_CMP;
				break;
			}
		} 
		if (XPT_FC_IS_QUEUED(abort_ccb)
		 && (abort_ccb->ccb_h.pinfo.index == CAM_DONEQ_INDEX)) {
			/*
			 * It's already completed but waiting
			 * for our SWI to get to it.
			 */
			start_ccb->ccb_h.status = CAM_UA_ABORT;
			break;
		}
		/*
		 * If we weren't able to take care of the abort request
		 * in the XPT, pass the request down to the SIM for processing.
		 */
	}
	/* FALLTHROUGH */
	case XPT_ACCEPT_TARGET_IO:
	case XPT_EN_LUN:
	case XPT_IMMED_NOTIFY:
	case XPT_NOTIFY_ACK:
	case XPT_GET_TRAN_SETTINGS:
	case XPT_RESET_BUS:
	{
		struct cam_sim *sim;

		sim = start_ccb->ccb_h.path->bus->sim;
		(*(sim->sim_action))(sim, start_ccb);
		break;
	}
	case XPT_PATH_INQ:
	{
		struct cam_sim *sim;

		sim = start_ccb->ccb_h.path->bus->sim;
		(*(sim->sim_action))(sim, start_ccb);
		break;
	}
	case XPT_PATH_STATS:
		start_ccb->cpis.last_reset =
			start_ccb->ccb_h.path->bus->last_reset;
		start_ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case XPT_GDEV_TYPE:
	{
		struct cam_ed *dev;

		dev = start_ccb->ccb_h.path->device;
		if ((dev->flags & CAM_DEV_UNCONFIGURED) != 0) {
			start_ccb->ccb_h.status = CAM_DEV_NOT_THERE;
		} else {
			struct ccb_getdev *cgd;
			struct cam_eb *bus;
			struct cam_et *tar;

			cgd = &start_ccb->cgd;
			bus = cgd->ccb_h.path->bus;
			tar = cgd->ccb_h.path->target;
			cgd->inq_data = dev->inq_data;
			cgd->ccb_h.status = CAM_REQ_CMP;
			cgd->serial_num_len = dev->serial_num_len;
			if ((dev->serial_num_len > 0)
			 && (dev->serial_num != NULL))
				bcopy(dev->serial_num, cgd->serial_num,
				      dev->serial_num_len);
		}
		break; 
	}
	case XPT_GDEV_STATS:
	{
		struct cam_ed *dev;

		dev = start_ccb->ccb_h.path->device;
		if ((dev->flags & CAM_DEV_UNCONFIGURED) != 0) {
			start_ccb->ccb_h.status = CAM_DEV_NOT_THERE;
		} else {
			struct ccb_getdevstats *cgds;
			struct cam_eb *bus;
			struct cam_et *tar;

			cgds = &start_ccb->cgds;
			bus = cgds->ccb_h.path->bus;
			tar = cgds->ccb_h.path->target;
			cgds->dev_openings = dev->ccbq.dev_openings;
			cgds->dev_active = dev->ccbq.dev_active;
			cgds->devq_openings = dev->ccbq.devq_openings;
			cgds->devq_queued = dev->ccbq.queue.entries;
			cgds->held = dev->ccbq.held;
			cgds->last_reset = tar->last_reset;
			cgds->maxtags = dev->quirk->maxtags;
			cgds->mintags = dev->quirk->mintags;
			if (timevalcmp(&tar->last_reset, &bus->last_reset, <))
				cgds->last_reset = bus->last_reset;
			cgds->ccb_h.status = CAM_REQ_CMP;
		}
		break;
	}
	case XPT_GDEVLIST:
	{
		struct cam_periph	*nperiph;
		struct periph_list	*periph_head;
		struct ccb_getdevlist	*cgdl;
		u_int			i;
		struct cam_ed		*device;
		int			found;


		found = 0;

		/*
		 * Don't want anyone mucking with our data.
		 */
		device = start_ccb->ccb_h.path->device;
		periph_head = &device->periphs;
		cgdl = &start_ccb->cgdl;

		/*
		 * Check and see if the list has changed since the user
		 * last requested a list member.  If so, tell them that the
		 * list has changed, and therefore they need to start over 
		 * from the beginning.
		 */
		if ((cgdl->index != 0) && 
		    (cgdl->generation != device->generation)) {
			cgdl->status = CAM_GDEVLIST_LIST_CHANGED;
			break;
		}

		/*
		 * Traverse the list of peripherals and attempt to find 
		 * the requested peripheral.
		 */
		for (nperiph = SLIST_FIRST(periph_head), i = 0;
		     (nperiph != NULL) && (i <= cgdl->index);
		     nperiph = SLIST_NEXT(nperiph, periph_links), i++) {
			if (i == cgdl->index) {
				strncpy(cgdl->periph_name,
					nperiph->periph_name,
					DEV_IDLEN);
				cgdl->unit_number = nperiph->unit_number;
				found = 1;
			}
		}
		if (found == 0) {
			cgdl->status = CAM_GDEVLIST_ERROR;
			break;
		}

		if (nperiph == NULL)
			cgdl->status = CAM_GDEVLIST_LAST_DEVICE;
		else
			cgdl->status = CAM_GDEVLIST_MORE_DEVS;

		cgdl->index++;
		cgdl->generation = device->generation;

		cgdl->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_DEV_MATCH:
	{
		dev_pos_type position_type;
		struct ccb_dev_match *cdm;

		cdm = &start_ccb->cdm;

		/*
		 * There are two ways of getting at information in the EDT.
		 * The first way is via the primary EDT tree.  It starts
		 * with a list of busses, then a list of targets on a bus,
		 * then devices/luns on a target, and then peripherals on a
		 * device/lun.  The "other" way is by the peripheral driver
		 * lists.  The peripheral driver lists are organized by
		 * peripheral driver.  (obviously)  So it makes sense to
		 * use the peripheral driver list if the user is looking
		 * for something like "da1", or all "da" devices.  If the
		 * user is looking for something on a particular bus/target
		 * or lun, it's generally better to go through the EDT tree.
		 */

		if (cdm->pos.position_type != CAM_DEV_POS_NONE)
			position_type = cdm->pos.position_type;
		else {
			u_int i;

			position_type = CAM_DEV_POS_NONE;

			for (i = 0; i < cdm->num_patterns; i++) {
				if ((cdm->patterns[i].type == DEV_MATCH_BUS)
				 ||(cdm->patterns[i].type == DEV_MATCH_DEVICE)){
					position_type = CAM_DEV_POS_EDT;
					break;
				}
			}

			if (cdm->num_patterns == 0)
				position_type = CAM_DEV_POS_EDT;
			else if (position_type == CAM_DEV_POS_NONE)
				position_type = CAM_DEV_POS_PDRV;
		}

		switch(position_type & CAM_DEV_POS_TYPEMASK) {
		case CAM_DEV_POS_EDT:
			xptedtmatch(cdm);
			break;
		case CAM_DEV_POS_PDRV:
			xptperiphlistmatch(cdm);
			break;
		default:
			cdm->status = CAM_DEV_MATCH_ERROR;
			break;
		}

		if (cdm->status == CAM_DEV_MATCH_ERROR)
			start_ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		else
			start_ccb->ccb_h.status = CAM_REQ_CMP;

		break;
	}
	case XPT_SASYNC_CB:
	{
		struct ccb_setasync *csa;
		struct async_node *cur_entry;
		struct async_list *async_head;
		u_int32_t added;

		csa = &start_ccb->csa;
		added = csa->event_enable;
		async_head = &csa->ccb_h.path->device->asyncs;

		/*
		 * If there is already an entry for us, simply
		 * update it.
		 */
		cur_entry = SLIST_FIRST(async_head);
		while (cur_entry != NULL) {
			if ((cur_entry->callback_arg == csa->callback_arg)
			 && (cur_entry->callback == csa->callback))
				break;
			cur_entry = SLIST_NEXT(cur_entry, links);
		}

		if (cur_entry != NULL) {
		 	/*
			 * If the request has no flags set,
			 * remove the entry.
			 */
			added &= ~cur_entry->event_enable;
			if (csa->event_enable == 0) {
				SLIST_REMOVE(async_head, cur_entry,
					     async_node, links);
				csa->ccb_h.path->device->refcount--;
				free(cur_entry, M_CAMXPT);
			} else {
				cur_entry->event_enable = csa->event_enable;
			}
		} else {
			cur_entry = malloc(sizeof(*cur_entry), M_CAMXPT,
					   M_NOWAIT);
			if (cur_entry == NULL) {
				csa->ccb_h.status = CAM_RESRC_UNAVAIL;
				break;
			}
			cur_entry->event_enable = csa->event_enable;
			cur_entry->callback_arg = csa->callback_arg;
			cur_entry->callback = csa->callback;
			SLIST_INSERT_HEAD(async_head, cur_entry, links);
			csa->ccb_h.path->device->refcount++;
		}

		/*
		 * Need to decouple this operation via a taqskqueue so that
		 * the locking doesn't become a mess.
		 */
		if ((added & (AC_FOUND_DEVICE | AC_PATH_REGISTERED)) != 0) {
			struct xpt_task *task;

			task = malloc(sizeof(struct xpt_task), M_CAMXPT,
				      M_NOWAIT);
			if (task == NULL) {
				csa->ccb_h.status = CAM_RESRC_UNAVAIL;
				break;
			}

			TASK_INIT(&task->task, 0, xpt_action_sasync_cb, task);
			task->data1 = cur_entry;
			task->data2 = added;
			taskqueue_enqueue(taskqueue_thread, &task->task);
		}

		start_ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_REL_SIMQ:
	{
		struct ccb_relsim *crs;
		struct cam_ed *dev;

		crs = &start_ccb->crs;
		dev = crs->ccb_h.path->device;
		if (dev == NULL) {

			crs->ccb_h.status = CAM_DEV_NOT_THERE;
			break;
		}

		if ((crs->release_flags & RELSIM_ADJUST_OPENINGS) != 0) {

 			if (INQ_DATA_TQ_ENABLED(&dev->inq_data)) {
				/* Don't ever go below one opening */
				if (crs->openings > 0) {
					xpt_dev_ccbq_resize(crs->ccb_h.path,
							    crs->openings);

					if (bootverbose) {
						xpt_print(crs->ccb_h.path,
						    "tagged openings now %d\n",
						    crs->openings);
					}
				}
			}
		}

		if ((crs->release_flags & RELSIM_RELEASE_AFTER_TIMEOUT) != 0) {

			if ((dev->flags & CAM_DEV_REL_TIMEOUT_PENDING) != 0) {

				/*
				 * Just extend the old timeout and decrement
				 * the freeze count so that a single timeout
				 * is sufficient for releasing the queue.
				 */
				start_ccb->ccb_h.flags &= ~CAM_DEV_QFREEZE;
				callout_stop(&dev->callout);
			} else {

				start_ccb->ccb_h.flags |= CAM_DEV_QFREEZE;
			}

			callout_reset(&dev->callout,
			    (crs->release_timeout * hz) / 1000,
			    xpt_release_devq_timeout, dev);

			dev->flags |= CAM_DEV_REL_TIMEOUT_PENDING;

		}

		if ((crs->release_flags & RELSIM_RELEASE_AFTER_CMDCMPLT) != 0) {

			if ((dev->flags & CAM_DEV_REL_ON_COMPLETE) != 0) {
				/*
				 * Decrement the freeze count so that a single
				 * completion is still sufficient to unfreeze
				 * the queue.
				 */
				start_ccb->ccb_h.flags &= ~CAM_DEV_QFREEZE;
			} else {
				
				dev->flags |= CAM_DEV_REL_ON_COMPLETE;
				start_ccb->ccb_h.flags |= CAM_DEV_QFREEZE;
			}
		}

		if ((crs->release_flags & RELSIM_RELEASE_AFTER_QEMPTY) != 0) {

			if ((dev->flags & CAM_DEV_REL_ON_QUEUE_EMPTY) != 0
			 || (dev->ccbq.dev_active == 0)) {

				start_ccb->ccb_h.flags &= ~CAM_DEV_QFREEZE;
			} else {
				
				dev->flags |= CAM_DEV_REL_ON_QUEUE_EMPTY;
				start_ccb->ccb_h.flags |= CAM_DEV_QFREEZE;
			}
		}

		if ((start_ccb->ccb_h.flags & CAM_DEV_QFREEZE) == 0) {

			xpt_release_devq(crs->ccb_h.path, /*count*/1,
					 /*run_queue*/TRUE);
		}
		start_ccb->crs.qfrozen_cnt = dev->qfrozen_cnt;
		start_ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_SCAN_BUS:
		xpt_scan_bus(start_ccb->ccb_h.path->periph, start_ccb);
		break;
	case XPT_SCAN_LUN:
		xpt_scan_lun(start_ccb->ccb_h.path->periph,
			     start_ccb->ccb_h.path, start_ccb->crcn.flags,
			     start_ccb);
		break;
	case XPT_DEBUG: {
#ifdef CAMDEBUG
#ifdef CAM_DEBUG_DELAY
		cam_debug_delay = CAM_DEBUG_DELAY;
#endif
		cam_dflags = start_ccb->cdbg.flags;
		if (cam_dpath != NULL) {
			xpt_free_path(cam_dpath);
			cam_dpath = NULL;
		}

		if (cam_dflags != CAM_DEBUG_NONE) {
			if (xpt_create_path(&cam_dpath, xpt_periph,
					    start_ccb->ccb_h.path_id,
					    start_ccb->ccb_h.target_id,
					    start_ccb->ccb_h.target_lun) !=
					    CAM_REQ_CMP) {
				start_ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
				cam_dflags = CAM_DEBUG_NONE;
			} else {
				start_ccb->ccb_h.status = CAM_REQ_CMP;
				xpt_print(cam_dpath, "debugging flags now %x\n",
				    cam_dflags);
			}
		} else {
			cam_dpath = NULL;
			start_ccb->ccb_h.status = CAM_REQ_CMP;
		}
#else /* !CAMDEBUG */
		start_ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
#endif /* CAMDEBUG */
		break;
	}
	case XPT_NOOP:
		if ((start_ccb->ccb_h.flags & CAM_DEV_QFREEZE) != 0)
			xpt_freeze_devq(start_ccb->ccb_h.path, 1);
		start_ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	default:
	case XPT_SDEV_TYPE:
	case XPT_TERM_IO:
	case XPT_ENG_INQ:
		/* XXX Implement */
		start_ccb->ccb_h.status = CAM_PROVIDE_FAIL;
		break;
	}
}

void
xpt_polled_action(union ccb *start_ccb)
{
	u_int32_t timeout;
	struct	  cam_sim *sim;	
	struct	  cam_devq *devq;
	struct	  cam_ed *dev;


	timeout = start_ccb->ccb_h.timeout;
	sim = start_ccb->ccb_h.path->bus->sim;
	devq = sim->devq;
	dev = start_ccb->ccb_h.path->device;

	mtx_assert(sim->mtx, MA_OWNED);

	/*
	 * Steal an opening so that no other queued requests
	 * can get it before us while we simulate interrupts.
	 */
	dev->ccbq.devq_openings--;
	dev->ccbq.dev_openings--;	
	
	while(((devq != NULL && devq->send_openings <= 0) ||
	   dev->ccbq.dev_openings < 0) && (--timeout > 0)) {
		DELAY(1000);
		(*(sim->sim_poll))(sim);
		camisr_runqueue(&sim->sim_doneq);
	}
	
	dev->ccbq.devq_openings++;
	dev->ccbq.dev_openings++;
	
	if (timeout != 0) {
		xpt_action(start_ccb);
		while(--timeout > 0) {
			(*(sim->sim_poll))(sim);
			camisr_runqueue(&sim->sim_doneq);
			if ((start_ccb->ccb_h.status  & CAM_STATUS_MASK)
			    != CAM_REQ_INPROG)
				break;
			DELAY(1000);
		}
		if (timeout == 0) {
			/*
			 * XXX Is it worth adding a sim_timeout entry
			 * point so we can attempt recovery?  If
			 * this is only used for dumps, I don't think
			 * it is.
			 */
			start_ccb->ccb_h.status = CAM_CMD_TIMEOUT;
		}
	} else {
		start_ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
	}
}
	
/*
 * Schedule a peripheral driver to receive a ccb when it's
 * target device has space for more transactions.
 */
void
xpt_schedule(struct cam_periph *perph, u_int32_t new_priority)
{
	struct cam_ed *device;
	union ccb *work_ccb;
	int runq;

	mtx_assert(perph->sim->mtx, MA_OWNED);

	CAM_DEBUG(perph->path, CAM_DEBUG_TRACE, ("xpt_schedule\n"));
	device = perph->path->device;
	if (periph_is_queued(perph)) {
		/* Simply reorder based on new priority */
		CAM_DEBUG(perph->path, CAM_DEBUG_SUBTRACE,
			  ("   change priority to %d\n", new_priority));
		if (new_priority < perph->pinfo.priority) {
			camq_change_priority(&device->drvq,
					     perph->pinfo.index,
					     new_priority);
		}
		runq = 0;
	} else if (SIM_DEAD(perph->path->bus->sim)) {
		/* The SIM is gone so just call periph_start directly. */
		work_ccb = xpt_get_ccb(perph->path->device);
		if (work_ccb == NULL)
			return; /* XXX */
		xpt_setup_ccb(&work_ccb->ccb_h, perph->path, new_priority);
		perph->pinfo.priority = new_priority;
		perph->periph_start(perph, work_ccb);
		return;
	} else {
		/* New entry on the queue */
		CAM_DEBUG(perph->path, CAM_DEBUG_SUBTRACE,
			  ("   added periph to queue\n"));
		perph->pinfo.priority = new_priority;
		perph->pinfo.generation = ++device->drvq.generation;
		camq_insert(&device->drvq, &perph->pinfo);
		runq = xpt_schedule_dev_allocq(perph->path->bus, device);
	}
	if (runq != 0) {
		CAM_DEBUG(perph->path, CAM_DEBUG_SUBTRACE,
			  ("   calling xpt_run_devq\n"));
		xpt_run_dev_allocq(perph->path->bus);
	}
}


/*
 * Schedule a device to run on a given queue.
 * If the device was inserted as a new entry on the queue,
 * return 1 meaning the device queue should be run. If we
 * were already queued, implying someone else has already
 * started the queue, return 0 so the caller doesn't attempt
 * to run the queue.
 */
static int
xpt_schedule_dev(struct camq *queue, cam_pinfo *pinfo,
		 u_int32_t new_priority)
{
	int retval;
	u_int32_t old_priority;

	CAM_DEBUG_PRINT(CAM_DEBUG_XPT, ("xpt_schedule_dev\n"));

	old_priority = pinfo->priority;

	/*
	 * Are we already queued?
	 */
	if (pinfo->index != CAM_UNQUEUED_INDEX) {
		/* Simply reorder based on new priority */
		if (new_priority < old_priority) {
			camq_change_priority(queue, pinfo->index,
					     new_priority);
			CAM_DEBUG_PRINT(CAM_DEBUG_XPT,
					("changed priority to %d\n",
					 new_priority));
		}
		retval = 0;
	} else {
		/* New entry on the queue */
		if (new_priority < old_priority)
			pinfo->priority = new_priority;

		CAM_DEBUG_PRINT(CAM_DEBUG_XPT,
				("Inserting onto queue\n"));
		pinfo->generation = ++queue->generation;
		camq_insert(queue, pinfo);
		retval = 1;
	}
	return (retval);
}

static void
xpt_run_dev_allocq(struct cam_eb *bus)
{
	struct	cam_devq *devq;

	CAM_DEBUG_PRINT(CAM_DEBUG_XPT, ("xpt_run_dev_allocq\n"));
	devq = bus->sim->devq;

	CAM_DEBUG_PRINT(CAM_DEBUG_XPT,
			("   qfrozen_cnt == 0x%x, entries == %d, "
			 "openings == %d, active == %d\n",
			 devq->alloc_queue.qfrozen_cnt,
			 devq->alloc_queue.entries,
			 devq->alloc_openings,
			 devq->alloc_active));

	devq->alloc_queue.qfrozen_cnt++;
	while ((devq->alloc_queue.entries > 0)
	    && (devq->alloc_openings > 0)
	    && (devq->alloc_queue.qfrozen_cnt <= 1)) {
		struct	cam_ed_qinfo *qinfo;
		struct	cam_ed *device;
		union	ccb *work_ccb;
		struct	cam_periph *drv;
		struct	camq *drvq;
		
		qinfo = (struct cam_ed_qinfo *)camq_remove(&devq->alloc_queue,
							   CAMQ_HEAD);
		device = qinfo->device;

		CAM_DEBUG_PRINT(CAM_DEBUG_XPT,
				("running device %p\n", device));

		drvq = &device->drvq;

#ifdef CAMDEBUG
		if (drvq->entries <= 0) {
			panic("xpt_run_dev_allocq: "
			      "Device on queue without any work to do");
		}
#endif
		if ((work_ccb = xpt_get_ccb(device)) != NULL) {
			devq->alloc_openings--;
			devq->alloc_active++;
			drv = (struct cam_periph*)camq_remove(drvq, CAMQ_HEAD);
			xpt_setup_ccb(&work_ccb->ccb_h, drv->path,
				      drv->pinfo.priority);
			CAM_DEBUG_PRINT(CAM_DEBUG_XPT,
					("calling periph start\n"));
			drv->periph_start(drv, work_ccb);
		} else {
			/*
			 * Malloc failure in alloc_ccb
			 */
			/*
			 * XXX add us to a list to be run from free_ccb
			 * if we don't have any ccbs active on this
			 * device queue otherwise we may never get run
			 * again.
			 */
			break;
		}
	
		if (drvq->entries > 0) {
			/* We have more work.  Attempt to reschedule */
			xpt_schedule_dev_allocq(bus, device);
		}
	}
	devq->alloc_queue.qfrozen_cnt--;
}

static void
xpt_run_dev_sendq(struct cam_eb *bus)
{
	struct	cam_devq *devq;

	CAM_DEBUG_PRINT(CAM_DEBUG_XPT, ("xpt_run_dev_sendq\n"));
	
	devq = bus->sim->devq;

	devq->send_queue.qfrozen_cnt++;
	while ((devq->send_queue.entries > 0)
	    && (devq->send_openings > 0)) {
		struct	cam_ed_qinfo *qinfo;
		struct	cam_ed *device;
		union ccb *work_ccb;
		struct	cam_sim *sim;

	    	if (devq->send_queue.qfrozen_cnt > 1) {
			break;
		}

		qinfo = (struct cam_ed_qinfo *)camq_remove(&devq->send_queue,
							   CAMQ_HEAD);
		device = qinfo->device;

		/*
		 * If the device has been "frozen", don't attempt
		 * to run it.
		 */
		if (device->qfrozen_cnt > 0) {
			continue;
		}

		CAM_DEBUG_PRINT(CAM_DEBUG_XPT,
				("running device %p\n", device));

		work_ccb = cam_ccbq_peek_ccb(&device->ccbq, CAMQ_HEAD);
		if (work_ccb == NULL) {
			printf("device on run queue with no ccbs???\n");
			continue;
		}

		if ((work_ccb->ccb_h.flags & CAM_HIGH_POWER) != 0) {

			mtx_lock(&xsoftc.xpt_lock);
		 	if (xsoftc.num_highpower <= 0) {
				/*
				 * We got a high power command, but we
				 * don't have any available slots.  Freeze
				 * the device queue until we have a slot
				 * available.
				 */
				device->qfrozen_cnt++;
				STAILQ_INSERT_TAIL(&xsoftc.highpowerq, 
						   &work_ccb->ccb_h, 
						   xpt_links.stqe);

				continue;
			} else {
				/*
				 * Consume a high power slot while
				 * this ccb runs.
				 */
				xsoftc.num_highpower--;
			}
			mtx_unlock(&xsoftc.xpt_lock);
		}
		devq->active_dev = device;
		cam_ccbq_remove_ccb(&device->ccbq, work_ccb);

		cam_ccbq_send_ccb(&device->ccbq, work_ccb);

		devq->send_openings--;
		devq->send_active++;		
		
		if (device->ccbq.queue.entries > 0)
			xpt_schedule_dev_sendq(bus, device);

		if (work_ccb && (work_ccb->ccb_h.flags & CAM_DEV_QFREEZE) != 0){
			/*
			 * The client wants to freeze the queue
			 * after this CCB is sent.
			 */
			device->qfrozen_cnt++;
		}

		/* In Target mode, the peripheral driver knows best... */
		if (work_ccb->ccb_h.func_code == XPT_SCSI_IO) {
			if ((device->inq_flags & SID_CmdQue) != 0
			 && work_ccb->csio.tag_action != CAM_TAG_ACTION_NONE)
				work_ccb->ccb_h.flags |= CAM_TAG_ACTION_VALID;
			else
				/*
				 * Clear this in case of a retried CCB that
				 * failed due to a rejected tag.
				 */
				work_ccb->ccb_h.flags &= ~CAM_TAG_ACTION_VALID;
		}

		/*
		 * Device queues can be shared among multiple sim instances
		 * that reside on different busses.  Use the SIM in the queue
		 * CCB's path, rather than the one in the bus that was passed
		 * into this function.
		 */
		sim = work_ccb->ccb_h.path->bus->sim;
		(*(sim->sim_action))(sim, work_ccb);

		devq->active_dev = NULL;
	}
	devq->send_queue.qfrozen_cnt--;
}

/*
 * This function merges stuff from the slave ccb into the master ccb, while
 * keeping important fields in the master ccb constant.
 */
void
xpt_merge_ccb(union ccb *master_ccb, union ccb *slave_ccb)
{

	/*
	 * Pull fields that are valid for peripheral drivers to set
	 * into the master CCB along with the CCB "payload".
	 */
	master_ccb->ccb_h.retry_count = slave_ccb->ccb_h.retry_count;
	master_ccb->ccb_h.func_code = slave_ccb->ccb_h.func_code;
	master_ccb->ccb_h.timeout = slave_ccb->ccb_h.timeout;
	master_ccb->ccb_h.flags = slave_ccb->ccb_h.flags;
	bcopy(&(&slave_ccb->ccb_h)[1], &(&master_ccb->ccb_h)[1],
	      sizeof(union ccb) - sizeof(struct ccb_hdr));
}

void
xpt_setup_ccb(struct ccb_hdr *ccb_h, struct cam_path *path, u_int32_t priority)
{

	CAM_DEBUG(path, CAM_DEBUG_TRACE, ("xpt_setup_ccb\n"));
	ccb_h->pinfo.priority = priority;
	ccb_h->path = path;
	ccb_h->path_id = path->bus->path_id;
	if (path->target)
		ccb_h->target_id = path->target->target_id;
	else
		ccb_h->target_id = CAM_TARGET_WILDCARD;
	if (path->device) {
		ccb_h->target_lun = path->device->lun_id;
		ccb_h->pinfo.generation = ++path->device->ccbq.queue.generation;
	} else {
		ccb_h->target_lun = CAM_TARGET_WILDCARD;
	}
	ccb_h->pinfo.index = CAM_UNQUEUED_INDEX;
	ccb_h->flags = 0;
}

/* Path manipulation functions */
cam_status
xpt_create_path(struct cam_path **new_path_ptr, struct cam_periph *perph,
		path_id_t path_id, target_id_t target_id, lun_id_t lun_id)
{
	struct	   cam_path *path;
	cam_status status;

	path = (struct cam_path *)malloc(sizeof(*path), M_CAMXPT, M_NOWAIT);

	if (path == NULL) {
		status = CAM_RESRC_UNAVAIL;
		return(status);
	}
	status = xpt_compile_path(path, perph, path_id, target_id, lun_id);
	if (status != CAM_REQ_CMP) {
		free(path, M_CAMXPT);
		path = NULL;
	}
	*new_path_ptr = path;
	return (status);
}

cam_status
xpt_create_path_unlocked(struct cam_path **new_path_ptr,
			 struct cam_periph *periph, path_id_t path_id,
			 target_id_t target_id, lun_id_t lun_id)
{
	struct	   cam_path *path;
	struct	   cam_eb *bus = NULL;
	cam_status status;
	int	   need_unlock = 0;

	path = (struct cam_path *)malloc(sizeof(*path), M_CAMXPT, M_WAITOK);

	if (path_id != CAM_BUS_WILDCARD) {
		bus = xpt_find_bus(path_id);
		if (bus != NULL) {
			need_unlock = 1;
			CAM_SIM_LOCK(bus->sim);
		}
	}
	status = xpt_compile_path(path, periph, path_id, target_id, lun_id);
	if (need_unlock)
		CAM_SIM_UNLOCK(bus->sim);
	if (status != CAM_REQ_CMP) {
		free(path, M_CAMXPT);
		path = NULL;
	}
	*new_path_ptr = path;
	return (status);
}

static cam_status
xpt_compile_path(struct cam_path *new_path, struct cam_periph *perph,
		 path_id_t path_id, target_id_t target_id, lun_id_t lun_id)
{
	struct	     cam_eb *bus;
	struct	     cam_et *target;
	struct	     cam_ed *device;
	cam_status   status;

	status = CAM_REQ_CMP;	/* Completed without error */
	target = NULL;		/* Wildcarded */
	device = NULL;		/* Wildcarded */

	/*
	 * We will potentially modify the EDT, so block interrupts
	 * that may attempt to create cam paths.
	 */
	bus = xpt_find_bus(path_id);
	if (bus == NULL) {
		status = CAM_PATH_INVALID;
	} else {
		target = xpt_find_target(bus, target_id);
		if (target == NULL) {
			/* Create one */
			struct cam_et *new_target;

			new_target = xpt_alloc_target(bus, target_id);
			if (new_target == NULL) {
				status = CAM_RESRC_UNAVAIL;
			} else {
				target = new_target;
			}
		}
		if (target != NULL) {
			device = xpt_find_device(target, lun_id);
			if (device == NULL) {
				/* Create one */
				struct cam_ed *new_device;

				new_device = xpt_alloc_device(bus,
							      target,
							      lun_id);
				if (new_device == NULL) {
					status = CAM_RESRC_UNAVAIL;
				} else {
					device = new_device;
				}
			}
		}
	}

	/*
	 * Only touch the user's data if we are successful.
	 */
	if (status == CAM_REQ_CMP) {
		new_path->periph = perph;
		new_path->bus = bus;
		new_path->target = target;
		new_path->device = device;
		CAM_DEBUG(new_path, CAM_DEBUG_TRACE, ("xpt_compile_path\n"));
	} else {
		if (device != NULL)
			xpt_release_device(bus, target, device);
		if (target != NULL)
			xpt_release_target(bus, target);
		if (bus != NULL)
			xpt_release_bus(bus);
	}
	return (status);
}

static void
xpt_release_path(struct cam_path *path)
{
	CAM_DEBUG(path, CAM_DEBUG_TRACE, ("xpt_release_path\n"));
	if (path->device != NULL) {
		xpt_release_device(path->bus, path->target, path->device);
		path->device = NULL;
	}
	if (path->target != NULL) {
		xpt_release_target(path->bus, path->target);
		path->target = NULL;
	}
	if (path->bus != NULL) {
		xpt_release_bus(path->bus);
		path->bus = NULL;
	}
}

void
xpt_free_path(struct cam_path *path)
{

	CAM_DEBUG(path, CAM_DEBUG_TRACE, ("xpt_free_path\n"));
	xpt_release_path(path);
	free(path, M_CAMXPT);
}


/*
 * Return -1 for failure, 0 for exact match, 1 for match with wildcards
 * in path1, 2 for match with wildcards in path2.
 */
int
xpt_path_comp(struct cam_path *path1, struct cam_path *path2)
{
	int retval = 0;

	if (path1->bus != path2->bus) {
		if (path1->bus->path_id == CAM_BUS_WILDCARD)
			retval = 1;
		else if (path2->bus->path_id == CAM_BUS_WILDCARD)
			retval = 2;
		else
			return (-1);
	}
	if (path1->target != path2->target) {
		if (path1->target->target_id == CAM_TARGET_WILDCARD) {
			if (retval == 0)
				retval = 1;
		} else if (path2->target->target_id == CAM_TARGET_WILDCARD)
			retval = 2;
		else
			return (-1);
	}
	if (path1->device != path2->device) {
		if (path1->device->lun_id == CAM_LUN_WILDCARD) {
			if (retval == 0)
				retval = 1;
		} else if (path2->device->lun_id == CAM_LUN_WILDCARD)
			retval = 2;
		else
			return (-1);
	}
	return (retval);
}

void
xpt_print_path(struct cam_path *path)
{

	if (path == NULL)
		printf("(nopath): ");
	else {
		if (path->periph != NULL)
			printf("(%s%d:", path->periph->periph_name,
			       path->periph->unit_number);
		else
			printf("(noperiph:");

		if (path->bus != NULL)
			printf("%s%d:%d:", path->bus->sim->sim_name,
			       path->bus->sim->unit_number,
			       path->bus->sim->bus_id);
		else
			printf("nobus:");

		if (path->target != NULL)
			printf("%d:", path->target->target_id);
		else
			printf("X:");

		if (path->device != NULL)
			printf("%d): ", path->device->lun_id);
		else
			printf("X): ");
	}
}

void
xpt_print(struct cam_path *path, const char *fmt, ...)
{
	va_list ap;
	xpt_print_path(path);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

int
xpt_path_string(struct cam_path *path, char *str, size_t str_len)
{
	struct sbuf sb;

	mtx_assert(path->bus->sim->mtx, MA_OWNED);

	sbuf_new(&sb, str, str_len, 0);

	if (path == NULL)
		sbuf_printf(&sb, "(nopath): ");
	else {
		if (path->periph != NULL)
			sbuf_printf(&sb, "(%s%d:", path->periph->periph_name,
				    path->periph->unit_number);
		else
			sbuf_printf(&sb, "(noperiph:");

		if (path->bus != NULL)
			sbuf_printf(&sb, "%s%d:%d:", path->bus->sim->sim_name,
				    path->bus->sim->unit_number,
				    path->bus->sim->bus_id);
		else
			sbuf_printf(&sb, "nobus:");

		if (path->target != NULL)
			sbuf_printf(&sb, "%d:", path->target->target_id);
		else
			sbuf_printf(&sb, "X:");

		if (path->device != NULL)
			sbuf_printf(&sb, "%d): ", path->device->lun_id);
		else
			sbuf_printf(&sb, "X): ");
	}
	sbuf_finish(&sb);

	return(sbuf_len(&sb));
}

path_id_t
xpt_path_path_id(struct cam_path *path)
{
	mtx_assert(path->bus->sim->mtx, MA_OWNED);

	return(path->bus->path_id);
}

target_id_t
xpt_path_target_id(struct cam_path *path)
{
	mtx_assert(path->bus->sim->mtx, MA_OWNED);

	if (path->target != NULL)
		return (path->target->target_id);
	else
		return (CAM_TARGET_WILDCARD);
}

lun_id_t
xpt_path_lun_id(struct cam_path *path)
{
	mtx_assert(path->bus->sim->mtx, MA_OWNED);

	if (path->device != NULL)
		return (path->device->lun_id);
	else
		return (CAM_LUN_WILDCARD);
}

struct cam_sim *
xpt_path_sim(struct cam_path *path)
{

	return (path->bus->sim);
}

struct cam_periph*
xpt_path_periph(struct cam_path *path)
{
	mtx_assert(path->bus->sim->mtx, MA_OWNED);

	return (path->periph);
}

/*
 * Release a CAM control block for the caller.  Remit the cost of the structure
 * to the device referenced by the path.  If the this device had no 'credits'
 * and peripheral drivers have registered async callbacks for this notification
 * call them now.
 */
void
xpt_release_ccb(union ccb *free_ccb)
{
	struct	 cam_path *path;
	struct	 cam_ed *device;
	struct	 cam_eb *bus;
	struct   cam_sim *sim;

	CAM_DEBUG_PRINT(CAM_DEBUG_XPT, ("xpt_release_ccb\n"));
	path = free_ccb->ccb_h.path;
	device = path->device;
	bus = path->bus;
	sim = bus->sim;

	mtx_assert(sim->mtx, MA_OWNED);

	cam_ccbq_release_opening(&device->ccbq);
	if (sim->ccb_count > sim->max_ccbs) {
		xpt_free_ccb(free_ccb);
		sim->ccb_count--;
	} else {
		SLIST_INSERT_HEAD(&sim->ccb_freeq, &free_ccb->ccb_h,
		    xpt_links.sle);
	}
	if (sim->devq == NULL) {
		return;
	}
	sim->devq->alloc_openings++;
	sim->devq->alloc_active--;
	/* XXX Turn this into an inline function - xpt_run_device?? */
	if ((device_is_alloc_queued(device) == 0)
	 && (device->drvq.entries > 0)) {
		xpt_schedule_dev_allocq(bus, device);
	}
	if (dev_allocq_is_runnable(sim->devq))
		xpt_run_dev_allocq(bus);
}

/* Functions accessed by SIM drivers */

/*
 * A sim structure, listing the SIM entry points and instance
 * identification info is passed to xpt_bus_register to hook the SIM
 * into the CAM framework.  xpt_bus_register creates a cam_eb entry
 * for this new bus and places it in the array of busses and assigns
 * it a path_id.  The path_id may be influenced by "hard wiring"
 * information specified by the user.  Once interrupt services are
 * availible, the bus will be probed.
 */
int32_t
xpt_bus_register(struct cam_sim *sim, device_t parent, u_int32_t bus)
{
	struct cam_eb *new_bus;
	struct cam_eb *old_bus;
	struct ccb_pathinq cpi;

	mtx_assert(sim->mtx, MA_OWNED);

	sim->bus_id = bus;
	new_bus = (struct cam_eb *)malloc(sizeof(*new_bus),
					  M_CAMXPT, M_NOWAIT);
	if (new_bus == NULL) {
		/* Couldn't satisfy request */
		return (CAM_RESRC_UNAVAIL);
	}

	if (strcmp(sim->sim_name, "xpt") != 0) {

		sim->path_id =
		    xptpathid(sim->sim_name, sim->unit_number, sim->bus_id);
	}

	TAILQ_INIT(&new_bus->et_entries);
	new_bus->path_id = sim->path_id;
	new_bus->sim = sim;
	timevalclear(&new_bus->last_reset);
	new_bus->flags = 0;
	new_bus->refcount = 1;	/* Held until a bus_deregister event */
	new_bus->generation = 0;
	mtx_lock(&xsoftc.xpt_topo_lock);
	old_bus = TAILQ_FIRST(&xsoftc.xpt_busses);
	while (old_bus != NULL
	    && old_bus->path_id < new_bus->path_id)
		old_bus = TAILQ_NEXT(old_bus, links);
	if (old_bus != NULL)
		TAILQ_INSERT_BEFORE(old_bus, new_bus, links);
	else
		TAILQ_INSERT_TAIL(&xsoftc.xpt_busses, new_bus, links);
	xsoftc.bus_generation++;
	mtx_unlock(&xsoftc.xpt_topo_lock);

	/* Notify interested parties */
	if (sim->path_id != CAM_XPT_PATH_ID) {
		struct cam_path path;

		xpt_compile_path(&path, /*periph*/NULL, sim->path_id,
			         CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);
		xpt_setup_ccb(&cpi.ccb_h, &path, /*priority*/1);
		cpi.ccb_h.func_code = XPT_PATH_INQ;
		xpt_action((union ccb *)&cpi);
		xpt_async(AC_PATH_REGISTERED, &path, &cpi);
		xpt_release_path(&path);
	}
	return (CAM_SUCCESS);
}

int32_t
xpt_bus_deregister(path_id_t pathid)
{
	struct cam_path bus_path;
	struct cam_ed *device;
	struct cam_ed_qinfo *qinfo;
	struct cam_devq *devq;
	struct cam_periph *periph;
	struct cam_sim *ccbsim;
	union ccb *work_ccb;
	cam_status status;


	status = xpt_compile_path(&bus_path, NULL, pathid,
				  CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);
	if (status != CAM_REQ_CMP)
		return (status);

	xpt_async(AC_LOST_DEVICE, &bus_path, NULL);
	xpt_async(AC_PATH_DEREGISTERED, &bus_path, NULL);

	/* The SIM may be gone, so use a dummy SIM for any stray operations. */
	devq = bus_path.bus->sim->devq;
	ccbsim = bus_path.bus->sim;
	bus_path.bus->sim = &cam_dead_sim;

	/* Execute any pending operations now. */
	while ((qinfo = (struct cam_ed_qinfo *)camq_remove(&devq->send_queue,
	    CAMQ_HEAD)) != NULL ||
	    (qinfo = (struct cam_ed_qinfo *)camq_remove(&devq->alloc_queue,
	    CAMQ_HEAD)) != NULL) {
		do {
			device = qinfo->device;
			work_ccb = cam_ccbq_peek_ccb(&device->ccbq, CAMQ_HEAD);
			if (work_ccb != NULL) {
				devq->active_dev = device;
				cam_ccbq_remove_ccb(&device->ccbq, work_ccb);
				cam_ccbq_send_ccb(&device->ccbq, work_ccb);
				(*(ccbsim->sim_action))(ccbsim, work_ccb);
			}

			periph = (struct cam_periph *)camq_remove(&device->drvq,
			    CAMQ_HEAD);
			if (periph != NULL)
				xpt_schedule(periph, periph->pinfo.priority);
		} while (work_ccb != NULL || periph != NULL);
	}

	/* Make sure all completed CCBs are processed. */
	while (!TAILQ_EMPTY(&ccbsim->sim_doneq)) {
		camisr_runqueue(&ccbsim->sim_doneq);

		/* Repeat the async's for the benefit of any new devices. */
		xpt_async(AC_LOST_DEVICE, &bus_path, NULL);
		xpt_async(AC_PATH_DEREGISTERED, &bus_path, NULL);
	}

	/* Release the reference count held while registered. */
	xpt_release_bus(bus_path.bus);
	xpt_release_path(&bus_path);

	return (CAM_REQ_CMP);
}

static path_id_t
xptnextfreepathid(void)
{
	struct cam_eb *bus;
	path_id_t pathid;
	const char *strval;

	pathid = 0;
	mtx_lock(&xsoftc.xpt_topo_lock);
	bus = TAILQ_FIRST(&xsoftc.xpt_busses);
retry:
	/* Find an unoccupied pathid */
	while (bus != NULL && bus->path_id <= pathid) {
		if (bus->path_id == pathid)
			pathid++;
		bus = TAILQ_NEXT(bus, links);
	}
	mtx_unlock(&xsoftc.xpt_topo_lock);

	/*
	 * Ensure that this pathid is not reserved for
	 * a bus that may be registered in the future.
	 */
	if (resource_string_value("scbus", pathid, "at", &strval) == 0) {
		++pathid;
		/* Start the search over */
		mtx_lock(&xsoftc.xpt_topo_lock);
		goto retry;
	}
	return (pathid);
}

static path_id_t
xptpathid(const char *sim_name, int sim_unit, int sim_bus)
{
	path_id_t pathid;
	int i, dunit, val;
	char buf[32];
	const char *dname;

	pathid = CAM_XPT_PATH_ID;
	snprintf(buf, sizeof(buf), "%s%d", sim_name, sim_unit);
	i = 0;
	while ((resource_find_match(&i, &dname, &dunit, "at", buf)) == 0) {
		if (strcmp(dname, "scbus")) {
			/* Avoid a bit of foot shooting. */
			continue;
		}
		if (dunit < 0)		/* unwired?! */
			continue;
		if (resource_int_value("scbus", dunit, "bus", &val) == 0) {
			if (sim_bus == val) {
				pathid = dunit;
				break;
			}
		} else if (sim_bus == 0) {
			/* Unspecified matches bus 0 */
			pathid = dunit;
			break;
		} else {
			printf("Ambiguous scbus configuration for %s%d "
			       "bus %d, cannot wire down.  The kernel "
			       "config entry for scbus%d should "
			       "specify a controller bus.\n"
			       "Scbus will be assigned dynamically.\n",
			       sim_name, sim_unit, sim_bus, dunit);
			break;
		}
	}

	if (pathid == CAM_XPT_PATH_ID)
		pathid = xptnextfreepathid();
	return (pathid);
}

void
xpt_async(u_int32_t async_code, struct cam_path *path, void *async_arg)
{
	struct cam_eb *bus;
	struct cam_et *target, *next_target;
	struct cam_ed *device, *next_device;

	mtx_assert(path->bus->sim->mtx, MA_OWNED);

	CAM_DEBUG(path, CAM_DEBUG_TRACE, ("xpt_async\n"));

	/*
	 * Most async events come from a CAM interrupt context.  In
	 * a few cases, the error recovery code at the peripheral layer,
	 * which may run from our SWI or a process context, may signal
	 * deferred events with a call to xpt_async.
	 */

	bus = path->bus;

	if (async_code == AC_BUS_RESET) { 
		/* Update our notion of when the last reset occurred */
		microtime(&bus->last_reset);
	}

	for (target = TAILQ_FIRST(&bus->et_entries);
	     target != NULL;
	     target = next_target) {

		next_target = TAILQ_NEXT(target, links);

		if (path->target != target
		 && path->target->target_id != CAM_TARGET_WILDCARD
		 && target->target_id != CAM_TARGET_WILDCARD)
			continue;

		if (async_code == AC_SENT_BDR) {
			/* Update our notion of when the last reset occurred */
			microtime(&path->target->last_reset);
		}

		for (device = TAILQ_FIRST(&target->ed_entries);
		     device != NULL;
		     device = next_device) {

			next_device = TAILQ_NEXT(device, links);

			if (path->device != device 
			 && path->device->lun_id != CAM_LUN_WILDCARD
			 && device->lun_id != CAM_LUN_WILDCARD)
				continue;

			xpt_dev_async(async_code, bus, target,
				      device, async_arg);

			xpt_async_bcast(&device->asyncs, async_code,
					path, async_arg);
		}
	}
	
	/*
	 * If this wasn't a fully wildcarded async, tell all
	 * clients that want all async events.
	 */
	if (bus != xpt_periph->path->bus)
		xpt_async_bcast(&xpt_periph->path->device->asyncs, async_code,
				path, async_arg);
}

static void
xpt_async_bcast(struct async_list *async_head,
		u_int32_t async_code,
		struct cam_path *path, void *async_arg)
{
	struct async_node *cur_entry;

	cur_entry = SLIST_FIRST(async_head);
	while (cur_entry != NULL) {
		struct async_node *next_entry;
		/*
		 * Grab the next list entry before we call the current
		 * entry's callback.  This is because the callback function
		 * can delete its async callback entry.
		 */
		next_entry = SLIST_NEXT(cur_entry, links);
		if ((cur_entry->event_enable & async_code) != 0)
			cur_entry->callback(cur_entry->callback_arg,
					    async_code, path,
					    async_arg);
		cur_entry = next_entry;
	}
}

/*
 * Handle any per-device event notifications that require action by the XPT.
 */
static void
xpt_dev_async(u_int32_t async_code, struct cam_eb *bus, struct cam_et *target,
	      struct cam_ed *device, void *async_arg)
{
	cam_status status;
	struct cam_path newpath;

	/*
	 * We only need to handle events for real devices.
	 */
	if (target->target_id == CAM_TARGET_WILDCARD
	 || device->lun_id == CAM_LUN_WILDCARD)
		return;

	/*
	 * We need our own path with wildcards expanded to
	 * handle certain types of events.
	 */
	if ((async_code == AC_SENT_BDR)
	 || (async_code == AC_BUS_RESET)
	 || (async_code == AC_INQ_CHANGED))
		status = xpt_compile_path(&newpath, NULL,
					  bus->path_id,
					  target->target_id,
					  device->lun_id);
	else
		status = CAM_REQ_CMP_ERR;

	if (status == CAM_REQ_CMP) {

		/*
		 * Allow transfer negotiation to occur in a
		 * tag free environment.
		 */
		if (async_code == AC_SENT_BDR
		 || async_code == AC_BUS_RESET)
			xpt_toggle_tags(&newpath);

		if (async_code == AC_INQ_CHANGED) {
			/*
			 * We've sent a start unit command, or
			 * something similar to a device that
			 * may have caused its inquiry data to
			 * change. So we re-scan the device to
			 * refresh the inquiry data for it.
			 */
			xpt_scan_lun(newpath.periph, &newpath,
				     CAM_EXPECT_INQ_CHANGE, NULL);
		}
		xpt_release_path(&newpath);
	} else if (async_code == AC_LOST_DEVICE) {
		device->flags |= CAM_DEV_UNCONFIGURED;
	} else if (async_code == AC_TRANSFER_NEG) {
		struct ccb_trans_settings *settings;

		settings = (struct ccb_trans_settings *)async_arg;
		xpt_set_transfer_settings(settings, device,
					  /*async_update*/TRUE);
	}
}

u_int32_t
xpt_freeze_devq(struct cam_path *path, u_int count)
{
	struct ccb_hdr *ccbh;

	mtx_assert(path->bus->sim->mtx, MA_OWNED);

	path->device->qfrozen_cnt += count;

	/*
	 * Mark the last CCB in the queue as needing
	 * to be requeued if the driver hasn't
	 * changed it's state yet.  This fixes a race
	 * where a ccb is just about to be queued to
	 * a controller driver when it's interrupt routine
	 * freezes the queue.  To completly close the
	 * hole, controller drives must check to see
	 * if a ccb's status is still CAM_REQ_INPROG
	 * just before they queue
	 * the CCB.  See ahc_action/ahc_freeze_devq for
	 * an example.
	 */
	ccbh = TAILQ_LAST(&path->device->ccbq.active_ccbs, ccb_hdr_tailq);
	if (ccbh && ccbh->status == CAM_REQ_INPROG)
		ccbh->status = CAM_REQUEUE_REQ;
	return (path->device->qfrozen_cnt);
}

u_int32_t
xpt_freeze_simq(struct cam_sim *sim, u_int count)
{
	mtx_assert(sim->mtx, MA_OWNED);

	sim->devq->send_queue.qfrozen_cnt += count;
	if (sim->devq->active_dev != NULL) {
		struct ccb_hdr *ccbh;
		
		ccbh = TAILQ_LAST(&sim->devq->active_dev->ccbq.active_ccbs,
				  ccb_hdr_tailq);
		if (ccbh && ccbh->status == CAM_REQ_INPROG)
			ccbh->status = CAM_REQUEUE_REQ;
	}
	return (sim->devq->send_queue.qfrozen_cnt);
}

static void
xpt_release_devq_timeout(void *arg)
{
	struct cam_ed *device;

	device = (struct cam_ed *)arg;

	xpt_release_devq_device(device, /*count*/1, /*run_queue*/TRUE);
}

void
xpt_release_devq(struct cam_path *path, u_int count, int run_queue)
{
	mtx_assert(path->bus->sim->mtx, MA_OWNED);

	xpt_release_devq_device(path->device, count, run_queue);
}

static void
xpt_release_devq_device(struct cam_ed *dev, u_int count, int run_queue)
{
	int	rundevq;

	rundevq = 0;
	if (dev->qfrozen_cnt > 0) {

		count = (count > dev->qfrozen_cnt) ? dev->qfrozen_cnt : count;
		dev->qfrozen_cnt -= count;
		if (dev->qfrozen_cnt == 0) {

			/*
			 * No longer need to wait for a successful
			 * command completion.
			 */
			dev->flags &= ~CAM_DEV_REL_ON_COMPLETE;

			/*
			 * Remove any timeouts that might be scheduled
			 * to release this queue.
			 */
			if ((dev->flags & CAM_DEV_REL_TIMEOUT_PENDING) != 0) {
				callout_stop(&dev->callout);
				dev->flags &= ~CAM_DEV_REL_TIMEOUT_PENDING;
			}

			/*
			 * Now that we are unfrozen schedule the
			 * device so any pending transactions are
			 * run.
			 */
			if ((dev->ccbq.queue.entries > 0)
			 && (xpt_schedule_dev_sendq(dev->target->bus, dev))
			 && (run_queue != 0)) {
				rundevq = 1;
			}
		}
	}
	if (rundevq != 0)
		xpt_run_dev_sendq(dev->target->bus);
}

void
xpt_release_simq(struct cam_sim *sim, int run_queue)
{
	struct	camq *sendq;

	mtx_assert(sim->mtx, MA_OWNED);

	sendq = &(sim->devq->send_queue);
	if (sendq->qfrozen_cnt > 0) {

		sendq->qfrozen_cnt--;
		if (sendq->qfrozen_cnt == 0) {
			struct cam_eb *bus;

			/*
			 * If there is a timeout scheduled to release this
			 * sim queue, remove it.  The queue frozen count is
			 * already at 0.
			 */
			if ((sim->flags & CAM_SIM_REL_TIMEOUT_PENDING) != 0){
				callout_stop(&sim->callout);
				sim->flags &= ~CAM_SIM_REL_TIMEOUT_PENDING;
			}
			bus = xpt_find_bus(sim->path_id);

			if (run_queue) {
				/*
				 * Now that we are unfrozen run the send queue.
				 */
				xpt_run_dev_sendq(bus);
			}
			xpt_release_bus(bus);
		}
	}
}

/*
 * XXX Appears to be unused.
 */
static void
xpt_release_simq_timeout(void *arg)
{
	struct cam_sim *sim;

	sim = (struct cam_sim *)arg;
	xpt_release_simq(sim, /* run_queue */ TRUE);
}

void
xpt_done(union ccb *done_ccb)
{
	struct cam_sim *sim;

	CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_TRACE, ("xpt_done\n"));
	if ((done_ccb->ccb_h.func_code & XPT_FC_QUEUED) != 0) {
		/*
		 * Queue up the request for handling by our SWI handler
		 * any of the "non-immediate" type of ccbs.
		 */
		sim = done_ccb->ccb_h.path->bus->sim;
		switch (done_ccb->ccb_h.path->periph->type) {
		case CAM_PERIPH_BIO:
			TAILQ_INSERT_TAIL(&sim->sim_doneq, &done_ccb->ccb_h,
					  sim_links.tqe);
			done_ccb->ccb_h.pinfo.index = CAM_DONEQ_INDEX;
			if ((sim->flags & CAM_SIM_ON_DONEQ) == 0) {
				mtx_lock(&cam_simq_lock);
				TAILQ_INSERT_TAIL(&cam_simq, sim,
						  links);
				sim->flags |= CAM_SIM_ON_DONEQ;
				mtx_unlock(&cam_simq_lock);
			}
			if ((done_ccb->ccb_h.path->periph->flags &
			    CAM_PERIPH_POLLED) == 0)
				swi_sched(cambio_ih, 0);
			break;
		default:
			panic("unknown periph type %d",
			    done_ccb->ccb_h.path->periph->type);
		}
	}
}

union ccb *
xpt_alloc_ccb()
{
	union ccb *new_ccb;

	new_ccb = malloc(sizeof(*new_ccb), M_CAMXPT, M_ZERO|M_WAITOK);
	return (new_ccb);
}

union ccb *
xpt_alloc_ccb_nowait()
{
	union ccb *new_ccb;

	new_ccb = malloc(sizeof(*new_ccb), M_CAMXPT, M_ZERO|M_NOWAIT);
	return (new_ccb);
}

void
xpt_free_ccb(union ccb *free_ccb)
{
	free(free_ccb, M_CAMXPT);
}



/* Private XPT functions */

/*
 * Get a CAM control block for the caller. Charge the structure to the device
 * referenced by the path.  If the this device has no 'credits' then the
 * device already has the maximum number of outstanding operations under way
 * and we return NULL. If we don't have sufficient resources to allocate more
 * ccbs, we also return NULL.
 */
static union ccb *
xpt_get_ccb(struct cam_ed *device)
{
	union ccb *new_ccb;
	struct cam_sim *sim;

	sim = device->sim;
	if ((new_ccb = (union ccb *)SLIST_FIRST(&sim->ccb_freeq)) == NULL) {
		new_ccb = xpt_alloc_ccb_nowait();
                if (new_ccb == NULL) {
			return (NULL);
		}
		if ((sim->flags & CAM_SIM_MPSAFE) == 0)
			callout_handle_init(&new_ccb->ccb_h.timeout_ch);
		SLIST_INSERT_HEAD(&sim->ccb_freeq, &new_ccb->ccb_h,
				  xpt_links.sle);
		sim->ccb_count++;
	}
	cam_ccbq_take_opening(&device->ccbq);
	SLIST_REMOVE_HEAD(&sim->ccb_freeq, xpt_links.sle);
	return (new_ccb);
}

static void
xpt_release_bus(struct cam_eb *bus)
{

	if ((--bus->refcount == 0)
	 && (TAILQ_FIRST(&bus->et_entries) == NULL)) {
		mtx_lock(&xsoftc.xpt_topo_lock);
		TAILQ_REMOVE(&xsoftc.xpt_busses, bus, links);
		xsoftc.bus_generation++;
		mtx_unlock(&xsoftc.xpt_topo_lock);
		free(bus, M_CAMXPT);
	}
}

static struct cam_et *
xpt_alloc_target(struct cam_eb *bus, target_id_t target_id)
{
	struct cam_et *target;

	target = (struct cam_et *)malloc(sizeof(*target), M_CAMXPT, M_NOWAIT);
	if (target != NULL) {
		struct cam_et *cur_target;

		TAILQ_INIT(&target->ed_entries);
		target->bus = bus;
		target->target_id = target_id;
		target->refcount = 1;
		target->generation = 0;
		timevalclear(&target->last_reset);
		/*
		 * Hold a reference to our parent bus so it
		 * will not go away before we do.
		 */
		bus->refcount++;

		/* Insertion sort into our bus's target list */
		cur_target = TAILQ_FIRST(&bus->et_entries);
		while (cur_target != NULL && cur_target->target_id < target_id)
			cur_target = TAILQ_NEXT(cur_target, links);

		if (cur_target != NULL) {
			TAILQ_INSERT_BEFORE(cur_target, target, links);
		} else {
			TAILQ_INSERT_TAIL(&bus->et_entries, target, links);
		}
		bus->generation++;
	}
	return (target);
}

static void
xpt_release_target(struct cam_eb *bus, struct cam_et *target)
{

	if ((--target->refcount == 0)
	 && (TAILQ_FIRST(&target->ed_entries) == NULL)) {
		TAILQ_REMOVE(&bus->et_entries, target, links);
		bus->generation++;
		free(target, M_CAMXPT);
		xpt_release_bus(bus);
	}
}

static struct cam_ed *
xpt_alloc_device(struct cam_eb *bus, struct cam_et *target, lun_id_t lun_id)
{
	struct	   cam_path path;
	struct	   cam_ed *device;
	struct	   cam_devq *devq;
	cam_status status;

	if (SIM_DEAD(bus->sim))
		return (NULL);

	/* Make space for us in the device queue on our bus */
	devq = bus->sim->devq;
	status = cam_devq_resize(devq, devq->alloc_queue.array_size + 1);

	if (status != CAM_REQ_CMP) {
		device = NULL;
	} else {
		device = (struct cam_ed *)malloc(sizeof(*device),
						 M_CAMXPT, M_NOWAIT);
	}

	if (device != NULL) {
		struct cam_ed *cur_device;

		cam_init_pinfo(&device->alloc_ccb_entry.pinfo);
		device->alloc_ccb_entry.device = device;
		cam_init_pinfo(&device->send_ccb_entry.pinfo);
		device->send_ccb_entry.device = device;
		device->target = target;
		device->lun_id = lun_id;
		device->sim = bus->sim;
		/* Initialize our queues */
		if (camq_init(&device->drvq, 0) != 0) {
			free(device, M_CAMXPT);
			return (NULL);
		}
		if (cam_ccbq_init(&device->ccbq,
				  bus->sim->max_dev_openings) != 0) {
			camq_fini(&device->drvq);
			free(device, M_CAMXPT);
			return (NULL);
		}
		SLIST_INIT(&device->asyncs);
		SLIST_INIT(&device->periphs);
		device->generation = 0;
		device->owner = NULL;
		/*
		 * Take the default quirk entry until we have inquiry
		 * data and can determine a better quirk to use.
		 */
		device->quirk = &xpt_quirk_table[xpt_quirk_table_size - 1];
		bzero(&device->inq_data, sizeof(device->inq_data));
		device->inq_flags = 0;
		device->queue_flags = 0;
		device->serial_num = NULL;
		device->serial_num_len = 0;
		device->qfrozen_cnt = 0;
		device->flags = CAM_DEV_UNCONFIGURED;
		device->tag_delay_count = 0;
		device->tag_saved_openings = 0;
		device->refcount = 1;
		if (bus->sim->flags & CAM_SIM_MPSAFE)
			callout_init_mtx(&device->callout, bus->sim->mtx, 0);
		else
			callout_init_mtx(&device->callout, &Giant, 0);

		/*
		 * Hold a reference to our parent target so it
		 * will not go away before we do.
		 */
		target->refcount++;

		/*
		 * XXX should be limited by number of CCBs this bus can
		 * do.
		 */
		bus->sim->max_ccbs += device->ccbq.devq_openings;
		/* Insertion sort into our target's device list */
		cur_device = TAILQ_FIRST(&target->ed_entries);
		while (cur_device != NULL && cur_device->lun_id < lun_id)
			cur_device = TAILQ_NEXT(cur_device, links);
		if (cur_device != NULL) {
			TAILQ_INSERT_BEFORE(cur_device, device, links);
		} else {
			TAILQ_INSERT_TAIL(&target->ed_entries, device, links);
		}
		target->generation++;
		if (lun_id != CAM_LUN_WILDCARD) {
			xpt_compile_path(&path,
					 NULL,
					 bus->path_id,
					 target->target_id,
					 lun_id);
			xpt_devise_transport(&path);
			xpt_release_path(&path);
		}
	}
	return (device);
}

static void
xpt_release_device(struct cam_eb *bus, struct cam_et *target,
		   struct cam_ed *device)
{

	if ((--device->refcount == 0)
	 && ((device->flags & CAM_DEV_UNCONFIGURED) != 0)) {
		struct cam_devq *devq;

		if (device->alloc_ccb_entry.pinfo.index != CAM_UNQUEUED_INDEX
		 || device->send_ccb_entry.pinfo.index != CAM_UNQUEUED_INDEX)
			panic("Removing device while still queued for ccbs");

		if ((device->flags & CAM_DEV_REL_TIMEOUT_PENDING) != 0)
				callout_stop(&device->callout);

		TAILQ_REMOVE(&target->ed_entries, device,links);
		target->generation++;
		bus->sim->max_ccbs -= device->ccbq.devq_openings;
		if (!SIM_DEAD(bus->sim)) {
			/* Release our slot in the devq */
			devq = bus->sim->devq;
			cam_devq_resize(devq, devq->alloc_queue.array_size - 1);
		}
		camq_fini(&device->drvq);
		camq_fini(&device->ccbq.queue);
		free(device, M_CAMXPT);
		xpt_release_target(bus, target);
	}
}

static u_int32_t
xpt_dev_ccbq_resize(struct cam_path *path, int newopenings)
{
	int	diff;
	int	result;
	struct	cam_ed *dev;

	dev = path->device;

	diff = newopenings - (dev->ccbq.dev_active + dev->ccbq.dev_openings);
	result = cam_ccbq_resize(&dev->ccbq, newopenings);
	if (result == CAM_REQ_CMP && (diff < 0)) {
		dev->flags |= CAM_DEV_RESIZE_QUEUE_NEEDED;
	}
	if ((dev->flags & CAM_DEV_TAG_AFTER_COUNT) != 0
	 || (dev->inq_flags & SID_CmdQue) != 0)
		dev->tag_saved_openings = newopenings;
	/* Adjust the global limit */
	dev->sim->max_ccbs += diff;
	return (result);
}

static struct cam_eb *
xpt_find_bus(path_id_t path_id)
{
	struct cam_eb *bus;

	mtx_lock(&xsoftc.xpt_topo_lock);
	for (bus = TAILQ_FIRST(&xsoftc.xpt_busses);
	     bus != NULL;
	     bus = TAILQ_NEXT(bus, links)) {
		if (bus->path_id == path_id) {
			bus->refcount++;
			break;
		}
	}
	mtx_unlock(&xsoftc.xpt_topo_lock);
	return (bus);
}

static struct cam_et *
xpt_find_target(struct cam_eb *bus, target_id_t	target_id)
{
	struct cam_et *target;

	for (target = TAILQ_FIRST(&bus->et_entries);
	     target != NULL;
	     target = TAILQ_NEXT(target, links)) {
		if (target->target_id == target_id) {
			target->refcount++;
			break;
		}
	}
	return (target);
}

static struct cam_ed *
xpt_find_device(struct cam_et *target, lun_id_t lun_id)
{
	struct cam_ed *device;

	for (device = TAILQ_FIRST(&target->ed_entries);
	     device != NULL;
	     device = TAILQ_NEXT(device, links)) {
		if (device->lun_id == lun_id) {
			device->refcount++;
			break;
		}
	}
	return (device);
}

typedef struct {
	union	ccb *request_ccb;
	struct 	ccb_pathinq *cpi;
	int	counter;
} xpt_scan_bus_info;

/*
 * To start a scan, request_ccb is an XPT_SCAN_BUS ccb.
 * As the scan progresses, xpt_scan_bus is used as the
 * callback on completion function.
 */
static void
xpt_scan_bus(struct cam_periph *periph, union ccb *request_ccb)
{
	CAM_DEBUG(request_ccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("xpt_scan_bus\n"));
	switch (request_ccb->ccb_h.func_code) {
	case XPT_SCAN_BUS:
	{
		xpt_scan_bus_info *scan_info;
		union	ccb *work_ccb;
		struct	cam_path *path;
		u_int	i;
		u_int	max_target;
		u_int	initiator_id;

		/* Find out the characteristics of the bus */
		work_ccb = xpt_alloc_ccb_nowait();
		if (work_ccb == NULL) {
			request_ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
			xpt_done(request_ccb);
			return;
		}
		xpt_setup_ccb(&work_ccb->ccb_h, request_ccb->ccb_h.path,
			      request_ccb->ccb_h.pinfo.priority);
		work_ccb->ccb_h.func_code = XPT_PATH_INQ;
		xpt_action(work_ccb);
		if (work_ccb->ccb_h.status != CAM_REQ_CMP) {
			request_ccb->ccb_h.status = work_ccb->ccb_h.status;
			xpt_free_ccb(work_ccb);
			xpt_done(request_ccb);
			return;
		}

		if ((work_ccb->cpi.hba_misc & PIM_NOINITIATOR) != 0) {
			/*
			 * Can't scan the bus on an adapter that
			 * cannot perform the initiator role.
			 */
			request_ccb->ccb_h.status = CAM_REQ_CMP;
			xpt_free_ccb(work_ccb);
			xpt_done(request_ccb);
			return;
		}

		/* Save some state for use while we probe for devices */
		scan_info = (xpt_scan_bus_info *)
		    malloc(sizeof(xpt_scan_bus_info), M_CAMXPT, M_NOWAIT);
		scan_info->request_ccb = request_ccb;
		scan_info->cpi = &work_ccb->cpi;

		/* Cache on our stack so we can work asynchronously */
		max_target = scan_info->cpi->max_target;
		initiator_id = scan_info->cpi->initiator_id;


		/*
		 * We can scan all targets in parallel, or do it sequentially.
		 */
		if (scan_info->cpi->hba_misc & PIM_SEQSCAN) {
			max_target = 0;
			scan_info->counter = 0;
		} else {
			scan_info->counter = scan_info->cpi->max_target + 1;
			if (scan_info->cpi->initiator_id < scan_info->counter) {
				scan_info->counter--;
			}
		}
		
		for (i = 0; i <= max_target; i++) {
			cam_status status;
			if (i == initiator_id)
				continue;

			status = xpt_create_path(&path, xpt_periph,
						 request_ccb->ccb_h.path_id,
						 i, 0);
			if (status != CAM_REQ_CMP) {
				printf("xpt_scan_bus: xpt_create_path failed"
				       " with status %#x, bus scan halted\n",
				       status);
				free(scan_info, M_CAMXPT);
				request_ccb->ccb_h.status = status;
				xpt_free_ccb(work_ccb);
				xpt_done(request_ccb);
				break;
			}
			work_ccb = xpt_alloc_ccb_nowait();
			if (work_ccb == NULL) {
				free(scan_info, M_CAMXPT);
				xpt_free_path(path);
				request_ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
				xpt_done(request_ccb);
				break;
			}
			xpt_setup_ccb(&work_ccb->ccb_h, path,
				      request_ccb->ccb_h.pinfo.priority);
			work_ccb->ccb_h.func_code = XPT_SCAN_LUN;
			work_ccb->ccb_h.cbfcnp = xpt_scan_bus;
			work_ccb->ccb_h.ppriv_ptr0 = scan_info;
			work_ccb->crcn.flags = request_ccb->crcn.flags;
			xpt_action(work_ccb);
		}
		break;
	}
	case XPT_SCAN_LUN:
	{
		cam_status status;
		struct cam_path *path;
		xpt_scan_bus_info *scan_info;
		path_id_t path_id;
		target_id_t target_id;
		lun_id_t lun_id;

		/* Reuse the same CCB to query if a device was really found */
		scan_info = (xpt_scan_bus_info *)request_ccb->ccb_h.ppriv_ptr0;
		xpt_setup_ccb(&request_ccb->ccb_h, request_ccb->ccb_h.path,
			      request_ccb->ccb_h.pinfo.priority);
		request_ccb->ccb_h.func_code = XPT_GDEV_TYPE;

		path_id = request_ccb->ccb_h.path_id;
		target_id = request_ccb->ccb_h.target_id;
		lun_id = request_ccb->ccb_h.target_lun;
		xpt_action(request_ccb);

		if (request_ccb->ccb_h.status != CAM_REQ_CMP) {
			struct cam_ed *device;
			struct cam_et *target;
			int phl;

			/*
			 * If we already probed lun 0 successfully, or
			 * we have additional configured luns on this
			 * target that might have "gone away", go onto
			 * the next lun.
			 */
			target = request_ccb->ccb_h.path->target;
			/*
			 * We may touch devices that we don't
			 * hold references too, so ensure they
			 * don't disappear out from under us.
			 * The target above is referenced by the
			 * path in the request ccb.
			 */
			phl = 0;
			device = TAILQ_FIRST(&target->ed_entries);
			if (device != NULL) {
				phl = CAN_SRCH_HI_SPARSE(device);
				if (device->lun_id == 0)
					device = TAILQ_NEXT(device, links);
			}
			if ((lun_id != 0) || (device != NULL)) {
				if (lun_id < (CAM_SCSI2_MAXLUN-1) || phl)
					lun_id++;
			}
		} else {
			struct cam_ed *device;
			
			device = request_ccb->ccb_h.path->device;

			if ((device->quirk->quirks & CAM_QUIRK_NOLUNS) == 0) {
				/* Try the next lun */
				if (lun_id < (CAM_SCSI2_MAXLUN-1)
				  || CAN_SRCH_HI_DENSE(device))
					lun_id++;
			}
		}

		/*
		 * Free the current request path- we're done with it.
		 */
		xpt_free_path(request_ccb->ccb_h.path);

		/*
		 * Check to see if we scan any further luns.
		 */
		if (lun_id == request_ccb->ccb_h.target_lun
                 || lun_id > scan_info->cpi->max_lun) {
			int done;

 hop_again:
			done = 0;
			if (scan_info->cpi->hba_misc & PIM_SEQSCAN) {
				scan_info->counter++;
				if (scan_info->counter == 
				    scan_info->cpi->initiator_id) {
					scan_info->counter++;
				}
				if (scan_info->counter >=
				    scan_info->cpi->max_target+1) {
					done = 1;
				}
			} else {
				scan_info->counter--;
				if (scan_info->counter == 0) {
					done = 1;
				}
			}
			if (done) {
				xpt_free_ccb(request_ccb);
				xpt_free_ccb((union ccb *)scan_info->cpi);
				request_ccb = scan_info->request_ccb;
				free(scan_info, M_CAMXPT);
				request_ccb->ccb_h.status = CAM_REQ_CMP;
				xpt_done(request_ccb);
				break;
			}

			if ((scan_info->cpi->hba_misc & PIM_SEQSCAN) == 0) {
				break;
			}
			status = xpt_create_path(&path, xpt_periph,
			    scan_info->request_ccb->ccb_h.path_id,
			    scan_info->counter, 0);
			if (status != CAM_REQ_CMP) {
				printf("xpt_scan_bus: xpt_create_path failed"
				    " with status %#x, bus scan halted\n",
			       	    status);
				xpt_free_ccb(request_ccb);
				xpt_free_ccb((union ccb *)scan_info->cpi);
				request_ccb = scan_info->request_ccb;
				free(scan_info, M_CAMXPT);
				request_ccb->ccb_h.status = status;
				xpt_done(request_ccb);
				break;
			}
			xpt_setup_ccb(&request_ccb->ccb_h, path,
			    request_ccb->ccb_h.pinfo.priority);
			request_ccb->ccb_h.func_code = XPT_SCAN_LUN;
			request_ccb->ccb_h.cbfcnp = xpt_scan_bus;
			request_ccb->ccb_h.ppriv_ptr0 = scan_info;
			request_ccb->crcn.flags =
			    scan_info->request_ccb->crcn.flags;
		} else {
			status = xpt_create_path(&path, xpt_periph,
						 path_id, target_id, lun_id);
			if (status != CAM_REQ_CMP) {
				printf("xpt_scan_bus: xpt_create_path failed "
				       "with status %#x, halting LUN scan\n",
			 	       status);
				goto hop_again;
			}
			xpt_setup_ccb(&request_ccb->ccb_h, path,
				      request_ccb->ccb_h.pinfo.priority);
			request_ccb->ccb_h.func_code = XPT_SCAN_LUN;
			request_ccb->ccb_h.cbfcnp = xpt_scan_bus;
			request_ccb->ccb_h.ppriv_ptr0 = scan_info;
			request_ccb->crcn.flags =
				scan_info->request_ccb->crcn.flags;
		}
		xpt_action(request_ccb);
		break;
	}
	default:
		break;
	}
}

typedef enum {
	PROBE_TUR,
	PROBE_INQUIRY,	/* this counts as DV0 for Basic Domain Validation */
	PROBE_FULL_INQUIRY,
	PROBE_MODE_SENSE,
	PROBE_SERIAL_NUM,
	PROBE_TUR_FOR_NEGOTIATION,
	PROBE_INQUIRY_BASIC_DV1,
	PROBE_INQUIRY_BASIC_DV2,
	PROBE_DV_EXIT
} probe_action;

typedef enum {
	PROBE_INQUIRY_CKSUM	= 0x01,
	PROBE_SERIAL_CKSUM	= 0x02,
	PROBE_NO_ANNOUNCE	= 0x04
} probe_flags;

typedef struct {
	TAILQ_HEAD(, ccb_hdr) request_ccbs;
	probe_action	action;
	union ccb	saved_ccb;
	probe_flags	flags;
	MD5_CTX		context;
	u_int8_t	digest[16];
} probe_softc;

static void
xpt_scan_lun(struct cam_periph *periph, struct cam_path *path,
	     cam_flags flags, union ccb *request_ccb)
{
	struct ccb_pathinq cpi;
	cam_status status;
	struct cam_path *new_path;
	struct cam_periph *old_periph;

	CAM_DEBUG(request_ccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("xpt_scan_lun\n"));
	
	xpt_setup_ccb(&cpi.ccb_h, path, /*priority*/1);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);

	if (cpi.ccb_h.status != CAM_REQ_CMP) {
		if (request_ccb != NULL) {
			request_ccb->ccb_h.status = cpi.ccb_h.status;
			xpt_done(request_ccb);
		}
		return;
	}

	if ((cpi.hba_misc & PIM_NOINITIATOR) != 0) {
		/*
		 * Can't scan the bus on an adapter that
		 * cannot perform the initiator role.
		 */
		if (request_ccb != NULL) {
			request_ccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(request_ccb);
		}
		return;
	}

	if (request_ccb == NULL) {
		request_ccb = malloc(sizeof(union ccb), M_CAMXPT, M_NOWAIT);
		if (request_ccb == NULL) {
			xpt_print(path, "xpt_scan_lun: can't allocate CCB, "
			    "can't continue\n");
			return;
		}
		new_path = malloc(sizeof(*new_path), M_CAMXPT, M_NOWAIT);
		if (new_path == NULL) {
			xpt_print(path, "xpt_scan_lun: can't allocate path, "
			    "can't continue\n");
			free(request_ccb, M_CAMXPT);
			return;
		}
		status = xpt_compile_path(new_path, xpt_periph,
					  path->bus->path_id,
					  path->target->target_id,
					  path->device->lun_id);

		if (status != CAM_REQ_CMP) {
			xpt_print(path, "xpt_scan_lun: can't compile path, "
			    "can't continue\n");
			free(request_ccb, M_CAMXPT);
			free(new_path, M_CAMXPT);
			return;
		}
		xpt_setup_ccb(&request_ccb->ccb_h, new_path, /*priority*/ 1);
		request_ccb->ccb_h.cbfcnp = xptscandone;
		request_ccb->ccb_h.func_code = XPT_SCAN_LUN;
		request_ccb->crcn.flags = flags;
	}

	if ((old_periph = cam_periph_find(path, "probe")) != NULL) {
		probe_softc *softc;

		softc = (probe_softc *)old_periph->softc;
		TAILQ_INSERT_TAIL(&softc->request_ccbs, &request_ccb->ccb_h,
				  periph_links.tqe);
	} else {
		status = cam_periph_alloc(proberegister, NULL, probecleanup,
					  probestart, "probe",
					  CAM_PERIPH_BIO,
					  request_ccb->ccb_h.path, NULL, 0,
					  request_ccb);

		if (status != CAM_REQ_CMP) {
			xpt_print(path, "xpt_scan_lun: cam_alloc_periph "
			    "returned an error, can't continue probe\n");
			request_ccb->ccb_h.status = status;
			xpt_done(request_ccb);
		}
	}
}

static void
xptscandone(struct cam_periph *periph, union ccb *done_ccb)
{
	xpt_release_path(done_ccb->ccb_h.path);
	free(done_ccb->ccb_h.path, M_CAMXPT);
	free(done_ccb, M_CAMXPT);
}

static cam_status
proberegister(struct cam_periph *periph, void *arg)
{
	union ccb *request_ccb;	/* CCB representing the probe request */
	cam_status status;
	probe_softc *softc;

	request_ccb = (union ccb *)arg;
	if (periph == NULL) {
		printf("proberegister: periph was NULL!!\n");
		return(CAM_REQ_CMP_ERR);
	}

	if (request_ccb == NULL) {
		printf("proberegister: no probe CCB, "
		       "can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = (probe_softc *)malloc(sizeof(*softc), M_CAMXPT, M_NOWAIT);

	if (softc == NULL) {
		printf("proberegister: Unable to probe new device. "
		       "Unable to allocate softc\n");				
		return(CAM_REQ_CMP_ERR);
	}
	TAILQ_INIT(&softc->request_ccbs);
	TAILQ_INSERT_TAIL(&softc->request_ccbs, &request_ccb->ccb_h,
			  periph_links.tqe);
	softc->flags = 0;
	periph->softc = softc;
	status = cam_periph_acquire(periph);
	if (status != CAM_REQ_CMP) {
		return (status);
	}


	/*
	 * Ensure we've waited at least a bus settle
	 * delay before attempting to probe the device.
	 * For HBAs that don't do bus resets, this won't make a difference.
	 */
	cam_periph_freeze_after_event(periph, &periph->path->bus->last_reset,
				      scsi_delay);
	probeschedule(periph);
	return(CAM_REQ_CMP);
}

static void
probeschedule(struct cam_periph *periph)
{
	struct ccb_pathinq cpi;
	union ccb *ccb;
	probe_softc *softc;

	softc = (probe_softc *)periph->softc;
	ccb = (union ccb *)TAILQ_FIRST(&softc->request_ccbs);

	xpt_setup_ccb(&cpi.ccb_h, periph->path, /*priority*/1);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);

	/*
	 * If a device has gone away and another device, or the same one,
	 * is back in the same place, it should have a unit attention
	 * condition pending.  It will not report the unit attention in
	 * response to an inquiry, which may leave invalid transfer
	 * negotiations in effect.  The TUR will reveal the unit attention
	 * condition.  Only send the TUR for lun 0, since some devices 
	 * will get confused by commands other than inquiry to non-existent
	 * luns.  If you think a device has gone away start your scan from
	 * lun 0.  This will insure that any bogus transfer settings are
	 * invalidated.
	 *
	 * If we haven't seen the device before and the controller supports
	 * some kind of transfer negotiation, negotiate with the first
	 * sent command if no bus reset was performed at startup.  This
	 * ensures that the device is not confused by transfer negotiation
	 * settings left over by loader or BIOS action.
	 */
	if (((ccb->ccb_h.path->device->flags & CAM_DEV_UNCONFIGURED) == 0)
	 && (ccb->ccb_h.target_lun == 0)) {
		softc->action = PROBE_TUR;
	} else if ((cpi.hba_inquiry & (PI_WIDE_32|PI_WIDE_16|PI_SDTR_ABLE)) != 0
	      && (cpi.hba_misc & PIM_NOBUSRESET) != 0) {
		proberequestdefaultnegotiation(periph);
		softc->action = PROBE_INQUIRY;
	} else {
		softc->action = PROBE_INQUIRY;
	}

	if (ccb->crcn.flags & CAM_EXPECT_INQ_CHANGE)
		softc->flags |= PROBE_NO_ANNOUNCE;
	else
		softc->flags &= ~PROBE_NO_ANNOUNCE;

	xpt_schedule(periph, ccb->ccb_h.pinfo.priority);
}

static void
probestart(struct cam_periph *periph, union ccb *start_ccb)
{
	/* Probe the device that our peripheral driver points to */
	struct ccb_scsiio *csio;
	probe_softc *softc;

	CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_TRACE, ("probestart\n"));

	softc = (probe_softc *)periph->softc;
	csio = &start_ccb->csio;

	switch (softc->action) {
	case PROBE_TUR:
	case PROBE_TUR_FOR_NEGOTIATION:
	case PROBE_DV_EXIT:
	{
		scsi_test_unit_ready(csio,
				     /*retries*/4,
				     probedone,
				     MSG_SIMPLE_Q_TAG,
				     SSD_FULL_SIZE,
				     /*timeout*/60000);
		break;
	}
	case PROBE_INQUIRY:
	case PROBE_FULL_INQUIRY:
	case PROBE_INQUIRY_BASIC_DV1:
	case PROBE_INQUIRY_BASIC_DV2:
	{
		u_int inquiry_len;
		struct scsi_inquiry_data *inq_buf;

		inq_buf = &periph->path->device->inq_data;

		/*
		 * If the device is currently configured, we calculate an
		 * MD5 checksum of the inquiry data, and if the serial number
		 * length is greater than 0, add the serial number data
		 * into the checksum as well.  Once the inquiry and the
		 * serial number check finish, we attempt to figure out
		 * whether we still have the same device.
		 */
		if ((periph->path->device->flags & CAM_DEV_UNCONFIGURED) == 0) {
			
			MD5Init(&softc->context);
			MD5Update(&softc->context, (unsigned char *)inq_buf,
				  sizeof(struct scsi_inquiry_data));
			softc->flags |= PROBE_INQUIRY_CKSUM;
			if (periph->path->device->serial_num_len > 0) {
				MD5Update(&softc->context,
					  periph->path->device->serial_num,
					  periph->path->device->serial_num_len);
				softc->flags |= PROBE_SERIAL_CKSUM;
			}
			MD5Final(softc->digest, &softc->context);
		} 

		if (softc->action == PROBE_INQUIRY)
			inquiry_len = SHORT_INQUIRY_LENGTH;
		else
			inquiry_len = SID_ADDITIONAL_LENGTH(inq_buf);

		/*
		 * Some parallel SCSI devices fail to send an
		 * ignore wide residue message when dealing with
		 * odd length inquiry requests.  Round up to be
		 * safe.
		 */
		inquiry_len = roundup2(inquiry_len, 2);
	
		if (softc->action == PROBE_INQUIRY_BASIC_DV1
		 || softc->action == PROBE_INQUIRY_BASIC_DV2) {
			inq_buf = malloc(inquiry_len, M_CAMXPT, M_NOWAIT);
		}
		if (inq_buf == NULL) {
			xpt_print(periph->path, "malloc failure- skipping Basic"
			    "Domain Validation\n");
			softc->action = PROBE_DV_EXIT;
			scsi_test_unit_ready(csio,
					     /*retries*/4,
					     probedone,
					     MSG_SIMPLE_Q_TAG,
					     SSD_FULL_SIZE,
					     /*timeout*/60000);
			break;
		}
		scsi_inquiry(csio,
			     /*retries*/4,
			     probedone,
			     MSG_SIMPLE_Q_TAG,
			     (u_int8_t *)inq_buf,
			     inquiry_len,
			     /*evpd*/FALSE,
			     /*page_code*/0,
			     SSD_MIN_SIZE,
			     /*timeout*/60 * 1000);
		break;
	}
	case PROBE_MODE_SENSE:
	{
		void  *mode_buf;
		int    mode_buf_len;

		mode_buf_len = sizeof(struct scsi_mode_header_6)
			     + sizeof(struct scsi_mode_blk_desc)
			     + sizeof(struct scsi_control_page);
		mode_buf = malloc(mode_buf_len, M_CAMXPT, M_NOWAIT);
		if (mode_buf != NULL) {
	                scsi_mode_sense(csio,
					/*retries*/4,
					probedone,
					MSG_SIMPLE_Q_TAG,
					/*dbd*/FALSE,
					SMS_PAGE_CTRL_CURRENT,
					SMS_CONTROL_MODE_PAGE,
					mode_buf,
					mode_buf_len,
					SSD_FULL_SIZE,
					/*timeout*/60000);
			break;
		}
		xpt_print(periph->path, "Unable to mode sense control page - "
		    "malloc failure\n");
		softc->action = PROBE_SERIAL_NUM;
	}
	/* FALLTHROUGH */
	case PROBE_SERIAL_NUM:
	{
		struct scsi_vpd_unit_serial_number *serial_buf;
		struct cam_ed* device;

		serial_buf = NULL;
		device = periph->path->device;
		device->serial_num = NULL;
		device->serial_num_len = 0;

		if ((device->quirk->quirks & CAM_QUIRK_NOSERIAL) == 0)
			serial_buf = (struct scsi_vpd_unit_serial_number *)
				malloc(sizeof(*serial_buf), M_CAMXPT,
					M_NOWAIT | M_ZERO);

		if (serial_buf != NULL) {
			scsi_inquiry(csio,
				     /*retries*/4,
				     probedone,
				     MSG_SIMPLE_Q_TAG,
				     (u_int8_t *)serial_buf,
				     sizeof(*serial_buf),
				     /*evpd*/TRUE,
				     SVPD_UNIT_SERIAL_NUMBER,
				     SSD_MIN_SIZE,
				     /*timeout*/60 * 1000);
			break;
		}
		/*
		 * We'll have to do without, let our probedone
		 * routine finish up for us.
		 */
		start_ccb->csio.data_ptr = NULL;
		probedone(periph, start_ccb);
		return;
	}
	}
	xpt_action(start_ccb);
}

static void
proberequestdefaultnegotiation(struct cam_periph *periph)
{
	struct ccb_trans_settings cts;

	xpt_setup_ccb(&cts.ccb_h, periph->path, /*priority*/1);
	cts.ccb_h.func_code = XPT_GET_TRAN_SETTINGS;
	cts.type = CTS_TYPE_USER_SETTINGS;
	xpt_action((union ccb *)&cts);
	if ((cts.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		return;
	}
	cts.ccb_h.func_code = XPT_SET_TRAN_SETTINGS;
	cts.type = CTS_TYPE_CURRENT_SETTINGS;
	xpt_action((union ccb *)&cts);
}

/*
 * Backoff Negotiation Code- only pertinent for SPI devices.
 */
static int
proberequestbackoff(struct cam_periph *periph, struct cam_ed *device)
{
	struct ccb_trans_settings cts;
	struct ccb_trans_settings_spi *spi;

	memset(&cts, 0, sizeof (cts));
	xpt_setup_ccb(&cts.ccb_h, periph->path, /*priority*/1);
	cts.ccb_h.func_code = XPT_GET_TRAN_SETTINGS;
	cts.type = CTS_TYPE_CURRENT_SETTINGS;
	xpt_action((union ccb *)&cts);
	if ((cts.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		if (bootverbose) {
			xpt_print(periph->path,
			    "failed to get current device settings\n");
		}
		return (0);
	}
	if (cts.transport != XPORT_SPI) {
		if (bootverbose) {
			xpt_print(periph->path, "not SPI transport\n");
		}
		return (0);
	}
	spi = &cts.xport_specific.spi;

	/*
	 * We cannot renegotiate sync rate if we don't have one.
	 */
	if ((spi->valid & CTS_SPI_VALID_SYNC_RATE) == 0) {
		if (bootverbose) {
			xpt_print(periph->path, "no sync rate known\n");
		}
		return (0);
	}

	/*
	 * We'll assert that we don't have to touch PPR options- the
	 * SIM will see what we do with period and offset and adjust
	 * the PPR options as appropriate.
	 */

	/*
	 * A sync rate with unknown or zero offset is nonsensical.
	 * A sync period of zero means Async.
	 */
	if ((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) == 0
	 || spi->sync_offset == 0 || spi->sync_period == 0) {
		if (bootverbose) {
			xpt_print(periph->path, "no sync rate available\n");
		}
		return (0);
	}

	if (device->flags & CAM_DEV_DV_HIT_BOTTOM) {
		CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
		    ("hit async: giving up on DV\n"));
		return (0);
	}


	/*
	 * Jump sync_period up by one, but stop at 5MHz and fall back to Async.
	 * We don't try to remember 'last' settings to see if the SIM actually
	 * gets into the speed we want to set. We check on the SIM telling
	 * us that a requested speed is bad, but otherwise don't try and
	 * check the speed due to the asynchronous and handshake nature
	 * of speed setting.
	 */
	spi->valid = CTS_SPI_VALID_SYNC_RATE | CTS_SPI_VALID_SYNC_OFFSET;
	for (;;) {
		spi->sync_period++;
		if (spi->sync_period >= 0xf) {
			spi->sync_period = 0;
			spi->sync_offset = 0;
			CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
			    ("setting to async for DV\n"));
			/*
			 * Once we hit async, we don't want to try
			 * any more settings.
			 */
			device->flags |= CAM_DEV_DV_HIT_BOTTOM;
		} else if (bootverbose) {
			CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
			    ("DV: period 0x%x\n", spi->sync_period));
			printf("setting period to 0x%x\n", spi->sync_period);
		}
		cts.ccb_h.func_code = XPT_SET_TRAN_SETTINGS;
		cts.type = CTS_TYPE_CURRENT_SETTINGS;
		xpt_action((union ccb *)&cts);
		if ((cts.ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			break;
		}
		CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
		    ("DV: failed to set period 0x%x\n", spi->sync_period));
		if (spi->sync_period == 0) {
			return (0);
		}
	}
	return (1);
}

static void
probedone(struct cam_periph *periph, union ccb *done_ccb)
{
	probe_softc *softc;
	struct cam_path *path;
	u_int32_t  priority;

	CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_TRACE, ("probedone\n"));

	softc = (probe_softc *)periph->softc;
	path = done_ccb->ccb_h.path;
	priority = done_ccb->ccb_h.pinfo.priority;

	switch (softc->action) {
	case PROBE_TUR:
	{
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {

			if (cam_periph_error(done_ccb, 0,
					     SF_NO_PRINT, NULL) == ERESTART)
				return;
			else if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
				/* Don't wedge the queue */
				xpt_release_devq(done_ccb->ccb_h.path,
						 /*count*/1,
						 /*run_queue*/TRUE);
		}
		softc->action = PROBE_INQUIRY;
		xpt_release_ccb(done_ccb);
		xpt_schedule(periph, priority);
		return;
	}
	case PROBE_INQUIRY:
	case PROBE_FULL_INQUIRY:
	{
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			struct scsi_inquiry_data *inq_buf;
			u_int8_t periph_qual;

			path->device->flags |= CAM_DEV_INQUIRY_DATA_VALID;
			inq_buf = &path->device->inq_data;

			periph_qual = SID_QUAL(inq_buf);
			
			switch(periph_qual) {
			case SID_QUAL_LU_CONNECTED:
			{
				u_int8_t len;

				/*
				 * We conservatively request only
				 * SHORT_INQUIRY_LEN bytes of inquiry
				 * information during our first try
				 * at sending an INQUIRY. If the device
				 * has more information to give,
				 * perform a second request specifying
				 * the amount of information the device
				 * is willing to give.
				 */
				len = inq_buf->additional_length
				    + offsetof(struct scsi_inquiry_data,
                                               additional_length) + 1;
				if (softc->action == PROBE_INQUIRY
				 && len > SHORT_INQUIRY_LENGTH) {
					softc->action = PROBE_FULL_INQUIRY;
					xpt_release_ccb(done_ccb);
					xpt_schedule(periph, priority);
					return;
				}

				xpt_find_quirk(path->device);

				xpt_devise_transport(path);
				if (INQ_DATA_TQ_ENABLED(inq_buf))
					softc->action = PROBE_MODE_SENSE;
				else
					softc->action = PROBE_SERIAL_NUM;

				path->device->flags &= ~CAM_DEV_UNCONFIGURED;

				xpt_release_ccb(done_ccb);
				xpt_schedule(periph, priority);
				return;
			}
			default:
				break;
			}
		} else if (cam_periph_error(done_ccb, 0,
					    done_ccb->ccb_h.target_lun > 0
					    ? SF_RETRY_UA|SF_QUIET_IR
					    : SF_RETRY_UA,
					    &softc->saved_ccb) == ERESTART) {
			return;
		} else if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
			/* Don't wedge the queue */
			xpt_release_devq(done_ccb->ccb_h.path, /*count*/1,
					 /*run_queue*/TRUE);
		}
		/*
		 * If we get to this point, we got an error status back
		 * from the inquiry and the error status doesn't require
		 * automatically retrying the command.  Therefore, the
		 * inquiry failed.  If we had inquiry information before
		 * for this device, but this latest inquiry command failed,
		 * the device has probably gone away.  If this device isn't
		 * already marked unconfigured, notify the peripheral
		 * drivers that this device is no more.
		 */
		if ((path->device->flags & CAM_DEV_UNCONFIGURED) == 0)
			/* Send the async notification. */
			xpt_async(AC_LOST_DEVICE, path, NULL);

		xpt_release_ccb(done_ccb);
		break;
	}
	case PROBE_MODE_SENSE:
	{
		struct ccb_scsiio *csio;
		struct scsi_mode_header_6 *mode_hdr;

		csio = &done_ccb->csio;
		mode_hdr = (struct scsi_mode_header_6 *)csio->data_ptr;
		if ((csio->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			struct scsi_control_page *page;
			u_int8_t *offset;

			offset = ((u_int8_t *)&mode_hdr[1])
			    + mode_hdr->blk_desc_len;
			page = (struct scsi_control_page *)offset;
			path->device->queue_flags = page->queue_flags;
		} else if (cam_periph_error(done_ccb, 0,
					    SF_RETRY_UA|SF_NO_PRINT,
					    &softc->saved_ccb) == ERESTART) {
			return;
		} else if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
			/* Don't wedge the queue */
			xpt_release_devq(done_ccb->ccb_h.path,
					 /*count*/1, /*run_queue*/TRUE);
		}
		xpt_release_ccb(done_ccb);
		free(mode_hdr, M_CAMXPT);
		softc->action = PROBE_SERIAL_NUM;
		xpt_schedule(periph, priority);
		return;
	}
	case PROBE_SERIAL_NUM:
	{
		struct ccb_scsiio *csio;
		struct scsi_vpd_unit_serial_number *serial_buf;
		u_int32_t  priority;
		int changed;
		int have_serialnum;

		changed = 1;
		have_serialnum = 0;
		csio = &done_ccb->csio;
		priority = done_ccb->ccb_h.pinfo.priority;
		serial_buf =
		    (struct scsi_vpd_unit_serial_number *)csio->data_ptr;

		/* Clean up from previous instance of this device */
		if (path->device->serial_num != NULL) {
			free(path->device->serial_num, M_CAMXPT);
			path->device->serial_num = NULL;
			path->device->serial_num_len = 0;
		}

		if (serial_buf == NULL) {
			/*
			 * Don't process the command as it was never sent
			 */
		} else if ((csio->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP
			&& (serial_buf->length > 0)) {

			have_serialnum = 1;
			path->device->serial_num =
				(u_int8_t *)malloc((serial_buf->length + 1),
						   M_CAMXPT, M_NOWAIT);
			if (path->device->serial_num != NULL) {
				bcopy(serial_buf->serial_num,
				      path->device->serial_num,
				      serial_buf->length);
				path->device->serial_num_len =
				    serial_buf->length;
				path->device->serial_num[serial_buf->length]
				    = '\0';
			}
		} else if (cam_periph_error(done_ccb, 0,
					    SF_RETRY_UA|SF_NO_PRINT,
					    &softc->saved_ccb) == ERESTART) {
			return;
		} else if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
			/* Don't wedge the queue */
			xpt_release_devq(done_ccb->ccb_h.path, /*count*/1,
					 /*run_queue*/TRUE);
		}
		
		/*
		 * Let's see if we have seen this device before.
		 */
		if ((softc->flags & PROBE_INQUIRY_CKSUM) != 0) {
			MD5_CTX context;
			u_int8_t digest[16];

			MD5Init(&context);
			
			MD5Update(&context,
				  (unsigned char *)&path->device->inq_data,
				  sizeof(struct scsi_inquiry_data));

			if (have_serialnum)
				MD5Update(&context, serial_buf->serial_num,
					  serial_buf->length);

			MD5Final(digest, &context);
			if (bcmp(softc->digest, digest, 16) == 0)
				changed = 0;

			/*
			 * XXX Do we need to do a TUR in order to ensure
			 *     that the device really hasn't changed???
			 */
			if ((changed != 0)
			 && ((softc->flags & PROBE_NO_ANNOUNCE) == 0))
				xpt_async(AC_LOST_DEVICE, path, NULL);
		}
		if (serial_buf != NULL)
			free(serial_buf, M_CAMXPT);

		if (changed != 0) {
			/*
			 * Now that we have all the necessary
			 * information to safely perform transfer
			 * negotiations... Controllers don't perform
			 * any negotiation or tagged queuing until
			 * after the first XPT_SET_TRAN_SETTINGS ccb is
			 * received.  So, on a new device, just retrieve
			 * the user settings, and set them as the current
			 * settings to set the device up.
			 */
			proberequestdefaultnegotiation(periph);
			xpt_release_ccb(done_ccb);

			/*
			 * Perform a TUR to allow the controller to
			 * perform any necessary transfer negotiation.
			 */
			softc->action = PROBE_TUR_FOR_NEGOTIATION;
			xpt_schedule(periph, priority);
			return;
		}
		xpt_release_ccb(done_ccb);
		break;
	}
	case PROBE_TUR_FOR_NEGOTIATION:
	case PROBE_DV_EXIT:
		if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
			/* Don't wedge the queue */
			xpt_release_devq(done_ccb->ccb_h.path, /*count*/1,
					 /*run_queue*/TRUE);
		}
		/*
		 * Do Domain Validation for lun 0 on devices that claim
		 * to support Synchronous Transfer modes.
		 */
	 	if (softc->action == PROBE_TUR_FOR_NEGOTIATION
		 && done_ccb->ccb_h.target_lun == 0
		 && (path->device->inq_data.flags & SID_Sync) != 0
                 && (path->device->flags & CAM_DEV_IN_DV) == 0) {
			CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
			    ("Begin Domain Validation\n"));
			path->device->flags |= CAM_DEV_IN_DV;
			xpt_release_ccb(done_ccb);
			softc->action = PROBE_INQUIRY_BASIC_DV1;
			xpt_schedule(periph, priority);
			return;
		}
		if (softc->action == PROBE_DV_EXIT) {
			CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
			    ("Leave Domain Validation\n"));
		}
		path->device->flags &=
		    ~(CAM_DEV_UNCONFIGURED|CAM_DEV_IN_DV|CAM_DEV_DV_HIT_BOTTOM);
		if ((softc->flags & PROBE_NO_ANNOUNCE) == 0) {
			/* Inform the XPT that a new device has been found */
			done_ccb->ccb_h.func_code = XPT_GDEV_TYPE;
			xpt_action(done_ccb);
			xpt_async(AC_FOUND_DEVICE, done_ccb->ccb_h.path,
				  done_ccb);
		}
		xpt_release_ccb(done_ccb);
		break;
	case PROBE_INQUIRY_BASIC_DV1:
	case PROBE_INQUIRY_BASIC_DV2:
	{
		struct scsi_inquiry_data *nbuf;
		struct ccb_scsiio *csio;

		if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
			/* Don't wedge the queue */
			xpt_release_devq(done_ccb->ccb_h.path, /*count*/1,
					 /*run_queue*/TRUE);
		}
		csio = &done_ccb->csio;
		nbuf = (struct scsi_inquiry_data *)csio->data_ptr;
		if (bcmp(nbuf, &path->device->inq_data, SHORT_INQUIRY_LENGTH)) {
			xpt_print(path,
			    "inquiry data fails comparison at DV%d step\n",
			    softc->action == PROBE_INQUIRY_BASIC_DV1? 1 : 2);
			if (proberequestbackoff(periph, path->device)) {
				path->device->flags &= ~CAM_DEV_IN_DV;
				softc->action = PROBE_TUR_FOR_NEGOTIATION;
			} else {
				/* give up */
				softc->action = PROBE_DV_EXIT;
			}
			free(nbuf, M_CAMXPT);
			xpt_release_ccb(done_ccb);
			xpt_schedule(periph, priority);
			return;
		}
		free(nbuf, M_CAMXPT);
		if (softc->action == PROBE_INQUIRY_BASIC_DV1) {
			softc->action = PROBE_INQUIRY_BASIC_DV2;
			xpt_release_ccb(done_ccb);
			xpt_schedule(periph, priority);
			return;
		}
		if (softc->action == PROBE_DV_EXIT) {
			CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
			    ("Leave Domain Validation Successfully\n"));
		}
		path->device->flags &=
		    ~(CAM_DEV_UNCONFIGURED|CAM_DEV_IN_DV|CAM_DEV_DV_HIT_BOTTOM);
		if ((softc->flags & PROBE_NO_ANNOUNCE) == 0) {
			/* Inform the XPT that a new device has been found */
			done_ccb->ccb_h.func_code = XPT_GDEV_TYPE;
			xpt_action(done_ccb);
			xpt_async(AC_FOUND_DEVICE, done_ccb->ccb_h.path,
				  done_ccb);
		}
		xpt_release_ccb(done_ccb);
		break;
	}
	}
	done_ccb = (union ccb *)TAILQ_FIRST(&softc->request_ccbs);
	TAILQ_REMOVE(&softc->request_ccbs, &done_ccb->ccb_h, periph_links.tqe);
	done_ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(done_ccb);
	if (TAILQ_FIRST(&softc->request_ccbs) == NULL) {
		cam_periph_invalidate(periph);
		cam_periph_release(periph);
	} else {
		probeschedule(periph);
	}
}

static void
probecleanup(struct cam_periph *periph)
{
	free(periph->softc, M_CAMXPT);
}

static void
xpt_find_quirk(struct cam_ed *device)
{
	caddr_t	match;

	match = cam_quirkmatch((caddr_t)&device->inq_data,
			       (caddr_t)xpt_quirk_table,
			       sizeof(xpt_quirk_table)/sizeof(*xpt_quirk_table),
			       sizeof(*xpt_quirk_table), scsi_inquiry_match);

	if (match == NULL)
		panic("xpt_find_quirk: device didn't match wildcard entry!!");

	device->quirk = (struct xpt_quirk_entry *)match;
}

static int
sysctl_cam_search_luns(SYSCTL_HANDLER_ARGS)
{
	int error, bool;

	bool = cam_srch_hi;
	error = sysctl_handle_int(oidp, &bool, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (bool == 0 || bool == 1) {
		cam_srch_hi = bool;
		return (0);
	} else {
		return (EINVAL);
	}
}


static void
xpt_devise_transport(struct cam_path *path)
{
	struct ccb_pathinq cpi;
	struct ccb_trans_settings cts;
	struct scsi_inquiry_data *inq_buf;

	/* Get transport information from the SIM */
	xpt_setup_ccb(&cpi.ccb_h, path, /*priority*/1);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);

	inq_buf = NULL;
	if ((path->device->flags & CAM_DEV_INQUIRY_DATA_VALID) != 0)
		inq_buf = &path->device->inq_data;
	path->device->protocol = PROTO_SCSI;
	path->device->protocol_version =
	    inq_buf != NULL ? SID_ANSI_REV(inq_buf) : cpi.protocol_version;
	path->device->transport = cpi.transport;
	path->device->transport_version = cpi.transport_version;

	/*
	 * Any device not using SPI3 features should
	 * be considered SPI2 or lower.
	 */
	if (inq_buf != NULL) {
		if (path->device->transport == XPORT_SPI
		 && (inq_buf->spi3data & SID_SPI_MASK) == 0
		 && path->device->transport_version > 2)
			path->device->transport_version = 2;
	} else {
		struct cam_ed* otherdev;

		for (otherdev = TAILQ_FIRST(&path->target->ed_entries);
		     otherdev != NULL;
		     otherdev = TAILQ_NEXT(otherdev, links)) {
			if (otherdev != path->device)
				break;
		}
		    
		if (otherdev != NULL) {
			/*
			 * Initially assume the same versioning as
			 * prior luns for this target.
			 */
			path->device->protocol_version =
			    otherdev->protocol_version;
			path->device->transport_version =
			    otherdev->transport_version;
		} else {
			/* Until we know better, opt for safty */
			path->device->protocol_version = 2;
			if (path->device->transport == XPORT_SPI)
				path->device->transport_version = 2;
			else
				path->device->transport_version = 0;
		}
	}

	/*
	 * XXX
	 * For a device compliant with SPC-2 we should be able
	 * to determine the transport version supported by
	 * scrutinizing the version descriptors in the
	 * inquiry buffer.
	 */

	/* Tell the controller what we think */
	xpt_setup_ccb(&cts.ccb_h, path, /*priority*/1);
	cts.ccb_h.func_code = XPT_SET_TRAN_SETTINGS;
	cts.type = CTS_TYPE_CURRENT_SETTINGS;
	cts.transport = path->device->transport;
	cts.transport_version = path->device->transport_version;
	cts.protocol = path->device->protocol;
	cts.protocol_version = path->device->protocol_version;
	cts.proto_specific.valid = 0;
	cts.xport_specific.valid = 0;
	xpt_action((union ccb *)&cts);
}

static void
xpt_set_transfer_settings(struct ccb_trans_settings *cts, struct cam_ed *device,
			  int async_update)
{
	struct	ccb_pathinq cpi;
	struct	ccb_trans_settings cur_cts;
	struct	ccb_trans_settings_scsi *scsi;
	struct	ccb_trans_settings_scsi *cur_scsi;
	struct	cam_sim *sim;
	struct	scsi_inquiry_data *inq_data;

	if (device == NULL) {
		cts->ccb_h.status = CAM_PATH_INVALID;
		xpt_done((union ccb *)cts);
		return;
	}

	if (cts->protocol == PROTO_UNKNOWN
	 || cts->protocol == PROTO_UNSPECIFIED) {
		cts->protocol = device->protocol;
		cts->protocol_version = device->protocol_version;
	}

	if (cts->protocol_version == PROTO_VERSION_UNKNOWN
	 || cts->protocol_version == PROTO_VERSION_UNSPECIFIED)
		cts->protocol_version = device->protocol_version;

	if (cts->protocol != device->protocol) {
		xpt_print(cts->ccb_h.path, "Uninitialized Protocol %x:%x?\n",
		       cts->protocol, device->protocol);
		cts->protocol = device->protocol;
	}

	if (cts->protocol_version > device->protocol_version) {
		if (bootverbose) {
			xpt_print(cts->ccb_h.path, "Down reving Protocol "
			    "Version from %d to %d?\n", cts->protocol_version,
			    device->protocol_version);
		}
		cts->protocol_version = device->protocol_version;
	}

	if (cts->transport == XPORT_UNKNOWN
	 || cts->transport == XPORT_UNSPECIFIED) {
		cts->transport = device->transport;
		cts->transport_version = device->transport_version;
	}

	if (cts->transport_version == XPORT_VERSION_UNKNOWN
	 || cts->transport_version == XPORT_VERSION_UNSPECIFIED)
		cts->transport_version = device->transport_version;

	if (cts->transport != device->transport) {
		xpt_print(cts->ccb_h.path, "Uninitialized Transport %x:%x?\n",
		    cts->transport, device->transport);
		cts->transport = device->transport;
	}

	if (cts->transport_version > device->transport_version) {
		if (bootverbose) {
			xpt_print(cts->ccb_h.path, "Down reving Transport "
			    "Version from %d to %d?\n", cts->transport_version,
			    device->transport_version);
		}
		cts->transport_version = device->transport_version;
	}

	sim = cts->ccb_h.path->bus->sim;

	/*
	 * Nothing more of interest to do unless
	 * this is a device connected via the
	 * SCSI protocol.
	 */
	if (cts->protocol != PROTO_SCSI) {
		if (async_update == FALSE) 
			(*(sim->sim_action))(sim, (union ccb *)cts);
		return;
	}

	inq_data = &device->inq_data;
	scsi = &cts->proto_specific.scsi;
	xpt_setup_ccb(&cpi.ccb_h, cts->ccb_h.path, /*priority*/1);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);

	/* SCSI specific sanity checking */
	if ((cpi.hba_inquiry & PI_TAG_ABLE) == 0
	 || (INQ_DATA_TQ_ENABLED(inq_data)) == 0
	 || (device->queue_flags & SCP_QUEUE_DQUE) != 0
	 || (device->quirk->mintags == 0)) {
		/*
		 * Can't tag on hardware that doesn't support tags,
		 * doesn't have it enabled, or has broken tag support.
		 */
		scsi->flags &= ~CTS_SCSI_FLAGS_TAG_ENB;
	}

	if (async_update == FALSE) {
		/*
		 * Perform sanity checking against what the
		 * controller and device can do.
		 */
		xpt_setup_ccb(&cur_cts.ccb_h, cts->ccb_h.path, /*priority*/1);
		cur_cts.ccb_h.func_code = XPT_GET_TRAN_SETTINGS;
		cur_cts.type = cts->type;
		xpt_action((union ccb *)&cur_cts);
		if ((cur_cts.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			return;
		}
		cur_scsi = &cur_cts.proto_specific.scsi;
		if ((scsi->valid & CTS_SCSI_VALID_TQ) == 0) {
			scsi->flags &= ~CTS_SCSI_FLAGS_TAG_ENB;
			scsi->flags |= cur_scsi->flags & CTS_SCSI_FLAGS_TAG_ENB;
		}
		if ((cur_scsi->valid & CTS_SCSI_VALID_TQ) == 0)
			scsi->flags &= ~CTS_SCSI_FLAGS_TAG_ENB;
	}

	/* SPI specific sanity checking */
	if (cts->transport == XPORT_SPI && async_update == FALSE) {
		u_int spi3caps;
		struct ccb_trans_settings_spi *spi;
		struct ccb_trans_settings_spi *cur_spi;

		spi = &cts->xport_specific.spi;

		cur_spi = &cur_cts.xport_specific.spi;

		/* Fill in any gaps in what the user gave us */
		if ((spi->valid & CTS_SPI_VALID_SYNC_RATE) == 0)
			spi->sync_period = cur_spi->sync_period;
		if ((cur_spi->valid & CTS_SPI_VALID_SYNC_RATE) == 0)
			spi->sync_period = 0;
		if ((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) == 0)
			spi->sync_offset = cur_spi->sync_offset;
		if ((cur_spi->valid & CTS_SPI_VALID_SYNC_OFFSET) == 0)
			spi->sync_offset = 0;
		if ((spi->valid & CTS_SPI_VALID_PPR_OPTIONS) == 0)
			spi->ppr_options = cur_spi->ppr_options;
		if ((cur_spi->valid & CTS_SPI_VALID_PPR_OPTIONS) == 0)
			spi->ppr_options = 0;
		if ((spi->valid & CTS_SPI_VALID_BUS_WIDTH) == 0)
			spi->bus_width = cur_spi->bus_width;
		if ((cur_spi->valid & CTS_SPI_VALID_BUS_WIDTH) == 0)
			spi->bus_width = 0;
		if ((spi->valid & CTS_SPI_VALID_DISC) == 0) {
			spi->flags &= ~CTS_SPI_FLAGS_DISC_ENB;
			spi->flags |= cur_spi->flags & CTS_SPI_FLAGS_DISC_ENB;
		}
		if ((cur_spi->valid & CTS_SPI_VALID_DISC) == 0)
			spi->flags &= ~CTS_SPI_FLAGS_DISC_ENB;
		if (((device->flags & CAM_DEV_INQUIRY_DATA_VALID) != 0
		  && (inq_data->flags & SID_Sync) == 0
		  && cts->type == CTS_TYPE_CURRENT_SETTINGS)
		 || ((cpi.hba_inquiry & PI_SDTR_ABLE) == 0)
		 || (spi->sync_offset == 0)
		 || (spi->sync_period == 0)) {
			/* Force async */
			spi->sync_period = 0;
			spi->sync_offset = 0;
		}

		switch (spi->bus_width) {
		case MSG_EXT_WDTR_BUS_32_BIT:
			if (((device->flags & CAM_DEV_INQUIRY_DATA_VALID) == 0
			  || (inq_data->flags & SID_WBus32) != 0
			  || cts->type == CTS_TYPE_USER_SETTINGS)
			 && (cpi.hba_inquiry & PI_WIDE_32) != 0)
				break;
			/* Fall Through to 16-bit */
		case MSG_EXT_WDTR_BUS_16_BIT:
			if (((device->flags & CAM_DEV_INQUIRY_DATA_VALID) == 0
			  || (inq_data->flags & SID_WBus16) != 0
			  || cts->type == CTS_TYPE_USER_SETTINGS)
			 && (cpi.hba_inquiry & PI_WIDE_16) != 0) {
				spi->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
				break;
			}
			/* Fall Through to 8-bit */
		default: /* New bus width?? */
		case MSG_EXT_WDTR_BUS_8_BIT:
			/* All targets can do this */
			spi->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
			break;
		}

		spi3caps = cpi.xport_specific.spi.ppr_options;
		if ((device->flags & CAM_DEV_INQUIRY_DATA_VALID) != 0
		 && cts->type == CTS_TYPE_CURRENT_SETTINGS)
			spi3caps &= inq_data->spi3data;

		if ((spi3caps & SID_SPI_CLOCK_DT) == 0)
			spi->ppr_options &= ~MSG_EXT_PPR_DT_REQ;

		if ((spi3caps & SID_SPI_IUS) == 0)
			spi->ppr_options &= ~MSG_EXT_PPR_IU_REQ;

		if ((spi3caps & SID_SPI_QAS) == 0)
			spi->ppr_options &= ~MSG_EXT_PPR_QAS_REQ;

		/* No SPI Transfer settings are allowed unless we are wide */
		if (spi->bus_width == 0)
			spi->ppr_options = 0;

		if ((spi->flags & CTS_SPI_FLAGS_DISC_ENB) == 0) {
			/*
			 * Can't tag queue without disconnection.
			 */
			scsi->flags &= ~CTS_SCSI_FLAGS_TAG_ENB;
			scsi->valid |= CTS_SCSI_VALID_TQ;
		}

		/*
		 * If we are currently performing tagged transactions to
		 * this device and want to change its negotiation parameters,
		 * go non-tagged for a bit to give the controller a chance to
		 * negotiate unhampered by tag messages.
		 */
		if (cts->type == CTS_TYPE_CURRENT_SETTINGS
		 && (device->inq_flags & SID_CmdQue) != 0
		 && (scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) != 0
		 && (spi->flags & (CTS_SPI_VALID_SYNC_RATE|
				   CTS_SPI_VALID_SYNC_OFFSET|
				   CTS_SPI_VALID_BUS_WIDTH)) != 0)
			xpt_toggle_tags(cts->ccb_h.path);
	}

	if (cts->type == CTS_TYPE_CURRENT_SETTINGS
	 && (scsi->valid & CTS_SCSI_VALID_TQ) != 0) {
		int device_tagenb;

		/*
		 * If we are transitioning from tags to no-tags or
		 * vice-versa, we need to carefully freeze and restart
		 * the queue so that we don't overlap tagged and non-tagged
		 * commands.  We also temporarily stop tags if there is
		 * a change in transfer negotiation settings to allow
		 * "tag-less" negotiation.
		 */
		if ((device->flags & CAM_DEV_TAG_AFTER_COUNT) != 0
		 || (device->inq_flags & SID_CmdQue) != 0)
			device_tagenb = TRUE;
		else
			device_tagenb = FALSE;

		if (((scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) != 0
		  && device_tagenb == FALSE)
		 || ((scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) == 0
		  && device_tagenb == TRUE)) {

			if ((scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) != 0) {
				/*
				 * Delay change to use tags until after a
				 * few commands have gone to this device so
				 * the controller has time to perform transfer
				 * negotiations without tagged messages getting
				 * in the way.
				 */
				device->tag_delay_count = CAM_TAG_DELAY_COUNT;
				device->flags |= CAM_DEV_TAG_AFTER_COUNT;
			} else {
				struct ccb_relsim crs;

				xpt_freeze_devq(cts->ccb_h.path, /*count*/1);
		  		device->inq_flags &= ~SID_CmdQue;
				xpt_dev_ccbq_resize(cts->ccb_h.path,
						    sim->max_dev_openings);
				device->flags &= ~CAM_DEV_TAG_AFTER_COUNT;
				device->tag_delay_count = 0;

				xpt_setup_ccb(&crs.ccb_h, cts->ccb_h.path,
					      /*priority*/1);
				crs.ccb_h.func_code = XPT_REL_SIMQ;
				crs.release_flags = RELSIM_RELEASE_AFTER_QEMPTY;
				crs.openings
				    = crs.release_timeout 
				    = crs.qfrozen_cnt
				    = 0;
				xpt_action((union ccb *)&crs);
			}
		}
	}
	if (async_update == FALSE) 
		(*(sim->sim_action))(sim, (union ccb *)cts);
}


static void
xpt_toggle_tags(struct cam_path *path)
{
	struct cam_ed *dev;

	/*
	 * Give controllers a chance to renegotiate
	 * before starting tag operations.  We
	 * "toggle" tagged queuing off then on
	 * which causes the tag enable command delay
	 * counter to come into effect.
	 */
	dev = path->device;
	if ((dev->flags & CAM_DEV_TAG_AFTER_COUNT) != 0
	 || ((dev->inq_flags & SID_CmdQue) != 0
 	  && (dev->inq_flags & (SID_Sync|SID_WBus16|SID_WBus32)) != 0)) {
		struct ccb_trans_settings cts;

		xpt_setup_ccb(&cts.ccb_h, path, 1);
		cts.protocol = PROTO_SCSI;
		cts.protocol_version = PROTO_VERSION_UNSPECIFIED;
		cts.transport = XPORT_UNSPECIFIED;
		cts.transport_version = XPORT_VERSION_UNSPECIFIED;
		cts.proto_specific.scsi.flags = 0;
		cts.proto_specific.scsi.valid = CTS_SCSI_VALID_TQ;
		xpt_set_transfer_settings(&cts, path->device,
					  /*async_update*/TRUE);
		cts.proto_specific.scsi.flags = CTS_SCSI_FLAGS_TAG_ENB;
		xpt_set_transfer_settings(&cts, path->device,
					  /*async_update*/TRUE);
	}
}

static void
xpt_start_tags(struct cam_path *path)
{
	struct ccb_relsim crs;
	struct cam_ed *device;
	struct cam_sim *sim;
	int    newopenings;

	device = path->device;
	sim = path->bus->sim;
	device->flags &= ~CAM_DEV_TAG_AFTER_COUNT;
	xpt_freeze_devq(path, /*count*/1);
	device->inq_flags |= SID_CmdQue;
	if (device->tag_saved_openings != 0)
		newopenings = device->tag_saved_openings;
	else
		newopenings = min(device->quirk->maxtags,
				  sim->max_tagged_dev_openings);
	xpt_dev_ccbq_resize(path, newopenings);
	xpt_setup_ccb(&crs.ccb_h, path, /*priority*/1);
	crs.ccb_h.func_code = XPT_REL_SIMQ;
	crs.release_flags = RELSIM_RELEASE_AFTER_QEMPTY;
	crs.openings
	    = crs.release_timeout 
	    = crs.qfrozen_cnt
	    = 0;
	xpt_action((union ccb *)&crs);
}

static int busses_to_config;
static int busses_to_reset;

static int
xptconfigbuscountfunc(struct cam_eb *bus, void *arg)
{

	mtx_assert(bus->sim->mtx, MA_OWNED);

	if (bus->path_id != CAM_XPT_PATH_ID) {
		struct cam_path path;
		struct ccb_pathinq cpi;
		int can_negotiate;

		busses_to_config++;
		xpt_compile_path(&path, NULL, bus->path_id,
				 CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);
		xpt_setup_ccb(&cpi.ccb_h, &path, /*priority*/1);
		cpi.ccb_h.func_code = XPT_PATH_INQ;
		xpt_action((union ccb *)&cpi);
		can_negotiate = cpi.hba_inquiry;
		can_negotiate &= (PI_WIDE_32|PI_WIDE_16|PI_SDTR_ABLE);
		if ((cpi.hba_misc & PIM_NOBUSRESET) == 0
		 && can_negotiate)
			busses_to_reset++;
		xpt_release_path(&path);
	}

	return(1);
}

static int
xptconfigfunc(struct cam_eb *bus, void *arg)
{
	struct	cam_path *path;
	union	ccb *work_ccb;

	mtx_assert(bus->sim->mtx, MA_OWNED);

	if (bus->path_id != CAM_XPT_PATH_ID) {
		cam_status status;
		int can_negotiate;

		work_ccb = xpt_alloc_ccb_nowait();
		if (work_ccb == NULL) {
			busses_to_config--;
			xpt_finishconfig(xpt_periph, NULL);
			return(0);
		}
		if ((status = xpt_create_path(&path, xpt_periph, bus->path_id,
					      CAM_TARGET_WILDCARD,
					      CAM_LUN_WILDCARD)) !=CAM_REQ_CMP){
			printf("xptconfigfunc: xpt_create_path failed with "
			       "status %#x for bus %d\n", status, bus->path_id);
			printf("xptconfigfunc: halting bus configuration\n");
			xpt_free_ccb(work_ccb);
			busses_to_config--;
			xpt_finishconfig(xpt_periph, NULL);
			return(0);
		}
		xpt_setup_ccb(&work_ccb->ccb_h, path, /*priority*/1);
		work_ccb->ccb_h.func_code = XPT_PATH_INQ;
		xpt_action(work_ccb);
		if (work_ccb->ccb_h.status != CAM_REQ_CMP) {
			printf("xptconfigfunc: CPI failed on bus %d "
			       "with status %d\n", bus->path_id,
			       work_ccb->ccb_h.status);
			xpt_finishconfig(xpt_periph, work_ccb);
			return(1);
		}

		can_negotiate = work_ccb->cpi.hba_inquiry;
		can_negotiate &= (PI_WIDE_32|PI_WIDE_16|PI_SDTR_ABLE);
		if ((work_ccb->cpi.hba_misc & PIM_NOBUSRESET) == 0
		 && (can_negotiate != 0)) {
			xpt_setup_ccb(&work_ccb->ccb_h, path, /*priority*/1);
			work_ccb->ccb_h.func_code = XPT_RESET_BUS;
			work_ccb->ccb_h.cbfcnp = NULL;
			CAM_DEBUG(path, CAM_DEBUG_SUBTRACE,
				  ("Resetting Bus\n"));
			xpt_action(work_ccb);
			xpt_finishconfig(xpt_periph, work_ccb);
		} else {
			/* Act as though we performed a successful BUS RESET */
			work_ccb->ccb_h.func_code = XPT_RESET_BUS;
			xpt_finishconfig(xpt_periph, work_ccb);
		}
	}

	return(1);
}

static void
xpt_config(void *arg)
{
	/*
	 * Now that interrupts are enabled, go find our devices
	 */

#ifdef CAMDEBUG
	/* Setup debugging flags and path */
#ifdef CAM_DEBUG_FLAGS
	cam_dflags = CAM_DEBUG_FLAGS;
#else /* !CAM_DEBUG_FLAGS */
	cam_dflags = CAM_DEBUG_NONE;
#endif /* CAM_DEBUG_FLAGS */
#ifdef CAM_DEBUG_BUS
	if (cam_dflags != CAM_DEBUG_NONE) {
		/*
		 * Locking is specifically omitted here.  No SIMs have
		 * registered yet, so xpt_create_path will only be searching
		 * empty lists of targets and devices.
		 */
		if (xpt_create_path(&cam_dpath, xpt_periph,
				    CAM_DEBUG_BUS, CAM_DEBUG_TARGET,
				    CAM_DEBUG_LUN) != CAM_REQ_CMP) {
			printf("xpt_config: xpt_create_path() failed for debug"
			       " target %d:%d:%d, debugging disabled\n",
			       CAM_DEBUG_BUS, CAM_DEBUG_TARGET, CAM_DEBUG_LUN);
			cam_dflags = CAM_DEBUG_NONE;
		}
	} else
		cam_dpath = NULL;
#else /* !CAM_DEBUG_BUS */
	cam_dpath = NULL;
#endif /* CAM_DEBUG_BUS */
#endif /* CAMDEBUG */

	/*
	 * Scan all installed busses.
	 */
	xpt_for_all_busses(xptconfigbuscountfunc, NULL);

	if (busses_to_config == 0) {
		/* Call manually because we don't have any busses */
		xpt_finishconfig(xpt_periph, NULL);
	} else  {
		if (busses_to_reset > 0 && scsi_delay >= 2000) {
			printf("Waiting %d seconds for SCSI "
			       "devices to settle\n", scsi_delay/1000);
		}
		xpt_for_all_busses(xptconfigfunc, NULL);
	}
}

/*
 * If the given device only has one peripheral attached to it, and if that
 * peripheral is the passthrough driver, announce it.  This insures that the
 * user sees some sort of announcement for every peripheral in their system.
 */
static int
xptpassannouncefunc(struct cam_ed *device, void *arg)
{
	struct cam_periph *periph;
	int i;

	for (periph = SLIST_FIRST(&device->periphs), i = 0; periph != NULL;
	     periph = SLIST_NEXT(periph, periph_links), i++);

	periph = SLIST_FIRST(&device->periphs);
	if ((i == 1)
	 && (strncmp(periph->periph_name, "pass", 4) == 0))
		xpt_announce_periph(periph, NULL);

	return(1);
}

static void
xpt_finishconfig_task(void *context, int pending)
{
	struct	periph_driver **p_drv;
	int	i;

	if (busses_to_config == 0) {
		/* Register all the peripheral drivers */
		/* XXX This will have to change when we have loadable modules */
		p_drv = periph_drivers;
		for (i = 0; p_drv[i] != NULL; i++) {
			(*p_drv[i]->init)();
		}

		/*
		 * Check for devices with no "standard" peripheral driver
		 * attached.  For any devices like that, announce the
		 * passthrough driver so the user will see something.
		 */
		xpt_for_all_devices(xptpassannouncefunc, NULL);

		/* Release our hook so that the boot can continue. */
		config_intrhook_disestablish(xsoftc.xpt_config_hook);
		free(xsoftc.xpt_config_hook, M_CAMXPT);
		xsoftc.xpt_config_hook = NULL;
	}

	free(context, M_CAMXPT);
}

static void
xpt_finishconfig(struct cam_periph *periph, union ccb *done_ccb)
{
	struct	xpt_task *task;

	if (done_ccb != NULL) {
		CAM_DEBUG(done_ccb->ccb_h.path, CAM_DEBUG_TRACE,
			  ("xpt_finishconfig\n"));
		switch(done_ccb->ccb_h.func_code) {
		case XPT_RESET_BUS:
			if (done_ccb->ccb_h.status == CAM_REQ_CMP) {
				done_ccb->ccb_h.func_code = XPT_SCAN_BUS;
				done_ccb->ccb_h.cbfcnp = xpt_finishconfig;
				done_ccb->crcn.flags = 0;
				xpt_action(done_ccb);
				return;
			}
			/* FALLTHROUGH */
		case XPT_SCAN_BUS:
		default:
			xpt_free_path(done_ccb->ccb_h.path);
			busses_to_config--;
			break;
		}
	}

	if (busses_to_config == 0) {
		task = malloc(sizeof(struct xpt_task), M_CAMXPT, M_NOWAIT);
		if (task != NULL) {
			TASK_INIT(&task->task, 0, xpt_finishconfig_task, task);
			taskqueue_enqueue(taskqueue_thread, &task->task);
		}
	}

	if (done_ccb != NULL)
		xpt_free_ccb(done_ccb);
}

cam_status
xpt_register_async(int event, ac_callback_t *cbfunc, void *cbarg,
		   struct cam_path *path)
{
	struct ccb_setasync csa;
	cam_status status;
	int xptpath = 0;

	if (path == NULL) {
		mtx_lock(&xsoftc.xpt_lock);
		status = xpt_create_path(&path, /*periph*/NULL, CAM_XPT_PATH_ID,
					 CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);
		if (status != CAM_REQ_CMP) {
			mtx_unlock(&xsoftc.xpt_lock);
			return (status);
		}
		xptpath = 1;
	}

	xpt_setup_ccb(&csa.ccb_h, path, /*priority*/5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = event;
	csa.callback = cbfunc;
	csa.callback_arg = cbarg;
	xpt_action((union ccb *)&csa);
	status = csa.ccb_h.status;
	if (xptpath) {
		xpt_free_path(path);
		mtx_unlock(&xsoftc.xpt_lock);
	}
	return (status);
}

static void
xptaction(struct cam_sim *sim, union ccb *work_ccb)
{
	CAM_DEBUG(work_ccb->ccb_h.path, CAM_DEBUG_TRACE, ("xptaction\n"));

	switch (work_ccb->ccb_h.func_code) {
	/* Common cases first */
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi;

		cpi = &work_ccb->cpi;
		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = 0;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = 0;
		cpi->max_lun = 0;
		cpi->initiator_id = 0;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "", HBA_IDLEN);
		strncpy(cpi->dev_name, sim->sim_name, DEV_IDLEN);
		cpi->unit_number = sim->unit_number;
		cpi->bus_id = sim->bus_id;
		cpi->base_transfer_speed = 0;
		cpi->protocol = PROTO_UNSPECIFIED;
		cpi->protocol_version = PROTO_VERSION_UNSPECIFIED;
		cpi->transport = XPORT_UNSPECIFIED;
		cpi->transport_version = XPORT_VERSION_UNSPECIFIED;
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(work_ccb);
		break;
	}
	default:
		work_ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(work_ccb);
		break;
	}
}

/*
 * The xpt as a "controller" has no interrupt sources, so polling
 * is a no-op.
 */
static void
xptpoll(struct cam_sim *sim)
{
}

void
xpt_lock_buses(void)
{
	mtx_lock(&xsoftc.xpt_topo_lock);
}

void
xpt_unlock_buses(void)
{
	mtx_unlock(&xsoftc.xpt_topo_lock);
}

static void
camisr(void *dummy)
{
	cam_simq_t queue;
	struct cam_sim *sim;

	mtx_lock(&cam_simq_lock);
	TAILQ_INIT(&queue);
	TAILQ_CONCAT(&queue, &cam_simq, links);
	mtx_unlock(&cam_simq_lock);

	while ((sim = TAILQ_FIRST(&queue)) != NULL) {
		TAILQ_REMOVE(&queue, sim, links);
		CAM_SIM_LOCK(sim);
		sim->flags &= ~CAM_SIM_ON_DONEQ;
		camisr_runqueue(&sim->sim_doneq);
		CAM_SIM_UNLOCK(sim);
	}
}

static void
camisr_runqueue(void *V_queue)
{
	cam_isrq_t *queue = V_queue;
	struct	ccb_hdr *ccb_h;

	while ((ccb_h = TAILQ_FIRST(queue)) != NULL) {
		int	runq;

		TAILQ_REMOVE(queue, ccb_h, sim_links.tqe);
		ccb_h->pinfo.index = CAM_UNQUEUED_INDEX;

		CAM_DEBUG(ccb_h->path, CAM_DEBUG_TRACE,
			  ("camisr\n"));

		runq = FALSE;

		if (ccb_h->flags & CAM_HIGH_POWER) {
			struct highpowerlist	*hphead;
			union ccb		*send_ccb;

			mtx_lock(&xsoftc.xpt_lock);
			hphead = &xsoftc.highpowerq;

			send_ccb = (union ccb *)STAILQ_FIRST(hphead);

			/*
			 * Increment the count since this command is done.
			 */
			xsoftc.num_highpower++;

			/* 
			 * Any high powered commands queued up?
			 */
			if (send_ccb != NULL) {

				STAILQ_REMOVE_HEAD(hphead, xpt_links.stqe);
				mtx_unlock(&xsoftc.xpt_lock);

				xpt_release_devq(send_ccb->ccb_h.path,
						 /*count*/1, /*runqueue*/TRUE);
			} else
				mtx_unlock(&xsoftc.xpt_lock);
		}

		if ((ccb_h->func_code & XPT_FC_USER_CCB) == 0) {
			struct cam_ed *dev;

			dev = ccb_h->path->device;

			cam_ccbq_ccb_done(&dev->ccbq, (union ccb *)ccb_h);

			if (!SIM_DEAD(ccb_h->path->bus->sim)) {
				ccb_h->path->bus->sim->devq->send_active--;
				ccb_h->path->bus->sim->devq->send_openings++;
			}
			
			if (((dev->flags & CAM_DEV_REL_ON_COMPLETE) != 0
			  && (ccb_h->status&CAM_STATUS_MASK) != CAM_REQUEUE_REQ)
			 || ((dev->flags & CAM_DEV_REL_ON_QUEUE_EMPTY) != 0
			  && (dev->ccbq.dev_active == 0))) {
				
				xpt_release_devq(ccb_h->path, /*count*/1,
						 /*run_queue*/TRUE);
			}

			if ((dev->flags & CAM_DEV_TAG_AFTER_COUNT) != 0
			 && (--dev->tag_delay_count == 0))
				xpt_start_tags(ccb_h->path);

			if ((dev->ccbq.queue.entries > 0)
			 && (dev->qfrozen_cnt == 0)
			 && (device_is_send_queued(dev) == 0)) {
				runq = xpt_schedule_dev_sendq(ccb_h->path->bus,
							      dev);
			}
		}

		if (ccb_h->status & CAM_RELEASE_SIMQ) {
			xpt_release_simq(ccb_h->path->bus->sim,
					 /*run_queue*/TRUE);
			ccb_h->status &= ~CAM_RELEASE_SIMQ;
			runq = FALSE;
		} 

		if ((ccb_h->flags & CAM_DEV_QFRZDIS)
		 && (ccb_h->status & CAM_DEV_QFRZN)) {
			xpt_release_devq(ccb_h->path, /*count*/1,
					 /*run_queue*/TRUE);
			ccb_h->status &= ~CAM_DEV_QFRZN;
		} else if (runq) {
			xpt_run_dev_sendq(ccb_h->path->bus);
		}

		/* Call the peripheral driver's callback */
		(*ccb_h->cbfcnp)(ccb_h->path->periph, (union ccb *)ccb_h);
	}
}

static void
dead_sim_action(struct cam_sim *sim, union ccb *ccb)
{

	ccb->ccb_h.status = CAM_DEV_NOT_THERE;
	xpt_done(ccb);
}
 
static void
dead_sim_poll(struct cam_sim *sim)
{
}
