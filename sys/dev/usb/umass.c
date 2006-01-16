/*-
 * Copyright (c) 1999 MAEKAWA Masahide <bishop@rr.iij4u.or.jp>,
 *		      Nick Hibma <n_hibma@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 *	$NetBSD: umass.c,v 1.28 2000/04/02 23:46:53 augustss Exp $
 */

/* Also already merged from NetBSD:
 *	$NetBSD: umass.c,v 1.67 2001/11/25 19:05:22 augustss Exp $
 *	$NetBSD: umass.c,v 1.90 2002/11/04 19:17:33 pooka Exp $
 *	$NetBSD: umass.c,v 1.108 2003/11/07 17:03:25 wiz Exp $
 *	$NetBSD: umass.c,v 1.109 2003/12/04 13:57:31 keihan Exp $
 */

/*
 * Universal Serial Bus Mass Storage Class specs:
 * http://www.usb.org/developers/devclass_docs/usb_msc_overview_1.2.pdf
 * http://www.usb.org/developers/devclass_docs/usbmassbulk_10.pdf
 * http://www.usb.org/developers/devclass_docs/usb_msc_cbi_1.1.pdf
 * http://www.usb.org/developers/devclass_docs/usbmass-ufi10.pdf
 */

/*
 * Ported to NetBSD by Lennart Augustsson <augustss@NetBSD.org>.
 * Parts of the code written by Jason R. Thorpe <thorpej@shagadelic.org>.
 */

/*
 * The driver handles 3 Wire Protocols
 * - Command/Bulk/Interrupt (CBI)
 * - Command/Bulk/Interrupt with Command Completion Interrupt (CBI with CCI)
 * - Mass Storage Bulk-Only (BBB)
 *   (BBB refers Bulk/Bulk/Bulk for Command/Data/Status phases)
 *
 * Over these wire protocols it handles the following command protocols
 * - SCSI
 * - UFI (floppy command set)
 * - 8070i (ATAPI)
 *
 * UFI and 8070i (ATAPI) are transformed versions of the SCSI command set. The
 * sc->transform method is used to convert the commands into the appropriate
 * format (if at all necessary). For example, UFI requires all commands to be
 * 12 bytes in length amongst other things.
 *
 * The source code below is marked and can be split into a number of pieces
 * (in this order):
 *
 * - probe/attach/detach
 * - generic transfer routines
 * - BBB
 * - CBI
 * - CBI_I (in addition to functions from CBI)
 * - CAM (Common Access Method)
 * - SCSI
 * - UFI
 * - 8070i (ATAPI)
 *
 * The protocols are implemented using a state machine, for the transfers as
 * well as for the resets. The state machine is contained in umass_*_state.
 * The state machine is started through either umass_*_transfer or
 * umass_*_reset.
 *
 * The reason for doing this is a) CAM performs a lot better this way and b) it
 * avoids using tsleep from interrupt context (for example after a failed
 * transfer).
 */

/*
 * The SCSI related part of this driver has been derived from the
 * dev/ppbus/vpo.c driver, by Nicolas Souchu (nsouch@freebsd.org).
 *
 * The CAM layer uses so called actions which are messages sent to the host
 * adapter for completion. The actions come in through umass_cam_action. The
 * appropriate block of routines is called depending on the transport protocol
 * in use. When the transfer has finished, these routines call
 * umass_cam_cb again to complete the CAM command.
 */

/*
 * XXX Currently CBI with CCI is not supported because it bombs the system
 *     when the device is detached (low frequency interrupts are detached
 *     too late.
 */
#undef CBI_I

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>

#include <cam/cam_periph.h>

#ifdef USB_DEBUG
#define DIF(m, x)	if (umassdebug & (m)) do { x ; } while (0)
#define	DPRINTF(m, x)	if (umassdebug & (m)) logprintf x
#define UDMASS_GEN	0x00010000	/* general */
#define UDMASS_SCSI	0x00020000	/* scsi */
#define UDMASS_UFI	0x00040000	/* ufi command set */
#define UDMASS_ATAPI	0x00080000	/* 8070i command set */
#define UDMASS_CMD	(UDMASS_SCSI|UDMASS_UFI|UDMASS_ATAPI)
#define UDMASS_USB	0x00100000	/* USB general */
#define UDMASS_BBB	0x00200000	/* Bulk-Only transfers */
#define UDMASS_CBI	0x00400000	/* CBI transfers */
#define UDMASS_WIRE	(UDMASS_BBB|UDMASS_CBI)
#define UDMASS_ALL	0xffff0000	/* all of the above */
int umassdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, umass, CTLFLAG_RW, 0, "USB umass");
SYSCTL_INT(_hw_usb_umass, OID_AUTO, debug, CTLFLAG_RW,
	   &umassdebug, 0, "umass debug level");
#else
#define DIF(m, x)	/* nop */
#define	DPRINTF(m, x)	/* nop */
#endif


/* Generic definitions */

/* Direction for umass_*_transfer */
#define DIR_NONE	0
#define DIR_IN		1
#define DIR_OUT		2

/* device name */
#define DEVNAME		"umass"
#define DEVNAME_SIM	"umass-sim"

#define UMASS_MAX_TRANSFER_SIZE		65536
/* Approximate maximum transfer speeds (assumes 33% overhead). */
#define UMASS_FULL_TRANSFER_SPEED	1000
#define UMASS_HIGH_TRANSFER_SPEED	40000
#define UMASS_FLOPPY_TRANSFER_SPEED	20

#define UMASS_TIMEOUT			5000 /* msecs */

/* CAM specific definitions */

#define UMASS_SCSIID_MAX	1	/* maximum number of drives expected */
#define UMASS_SCSIID_HOST	UMASS_SCSIID_MAX

#define MS_TO_TICKS(ms) ((ms) * hz / 1000)


/* Bulk-Only features */

#define UR_BBB_RESET		0xff		/* Bulk-Only reset */
#define UR_BBB_GET_MAX_LUN	0xfe		/* Get maximum lun */

/* Command Block Wrapper */
typedef struct {
	uDWord		dCBWSignature;
#	define CBWSIGNATURE	0x43425355
	uDWord		dCBWTag;
	uDWord		dCBWDataTransferLength;
	uByte		bCBWFlags;
#	define CBWFLAGS_OUT	0x00
#	define CBWFLAGS_IN	0x80
	uByte		bCBWLUN;
	uByte		bCDBLength;
#	define CBWCDBLENGTH	16
	uByte		CBWCDB[CBWCDBLENGTH];
} umass_bbb_cbw_t;
#define	UMASS_BBB_CBW_SIZE	31

/* Command Status Wrapper */
typedef struct {
	uDWord		dCSWSignature;
#	define CSWSIGNATURE	0x53425355
#	define CSWSIGNATURE_IMAGINATION_DBX1	0x43425355
#	define CSWSIGNATURE_OLYMPUS_C1	0x55425355
	uDWord		dCSWTag;
	uDWord		dCSWDataResidue;
	uByte		bCSWStatus;
#	define CSWSTATUS_GOOD	0x0
#	define CSWSTATUS_FAILED	0x1
#	define CSWSTATUS_PHASE	0x2
} umass_bbb_csw_t;
#define	UMASS_BBB_CSW_SIZE	13

/* CBI features */

#define UR_CBI_ADSC	0x00

typedef unsigned char umass_cbi_cbl_t[16];	/* Command block */

typedef union {
	struct {
		unsigned char	type;
		#define IDB_TYPE_CCI		0x00
		unsigned char	value;
		#define IDB_VALUE_PASS		0x00
		#define IDB_VALUE_FAIL		0x01
		#define IDB_VALUE_PHASE		0x02
		#define IDB_VALUE_PERSISTENT	0x03
		#define IDB_VALUE_STATUS_MASK	0x03
	} common;

	struct {
		unsigned char	asc;
		unsigned char	ascq;
	} ufi;
} umass_cbi_sbl_t;



struct umass_softc;		/* see below */

typedef void (*transfer_cb_f)	(struct umass_softc *sc, void *priv,
				int residue, int status);
#define STATUS_CMD_OK		0	/* everything ok */
#define STATUS_CMD_UNKNOWN	1	/* will have to fetch sense */
#define STATUS_CMD_FAILED	2	/* transfer was ok, command failed */
#define STATUS_WIRE_FAILED	3	/* couldn't even get command across */

typedef void (*wire_reset_f)	(struct umass_softc *sc, int status);
typedef void (*wire_transfer_f)	(struct umass_softc *sc, int lun,
				void *cmd, int cmdlen, void *data, int datalen,
				int dir, u_int timeout, transfer_cb_f cb, void *priv);
typedef void (*wire_state_f)	(usbd_xfer_handle xfer,
				usbd_private_handle priv, usbd_status err);

typedef int (*command_transform_f)	(struct umass_softc *sc,
				unsigned char *cmd, int cmdlen,
				unsigned char **rcmd, int *rcmdlen);


struct umass_devdescr_t {
	u_int32_t	vid;
#	define VID_WILDCARD	0xffffffff
#	define VID_EOT		0xfffffffe
	u_int32_t	pid;
#	define PID_WILDCARD	0xffffffff
#	define PID_EOT		0xfffffffe
	u_int32_t	rid;
#	define RID_WILDCARD	0xffffffff
#	define RID_EOT		0xfffffffe

	/* wire and command protocol */
	u_int16_t	proto;
#	define UMASS_PROTO_BBB		0x0001	/* USB wire protocol */
#	define UMASS_PROTO_CBI		0x0002
#	define UMASS_PROTO_CBI_I	0x0004
#	define UMASS_PROTO_WIRE		0x00ff	/* USB wire protocol mask */
#	define UMASS_PROTO_SCSI		0x0100	/* command protocol */
#	define UMASS_PROTO_ATAPI	0x0200
#	define UMASS_PROTO_UFI		0x0400
#	define UMASS_PROTO_RBC		0x0800
#	define UMASS_PROTO_COMMAND	0xff00	/* command protocol mask */

	/* Device specific quirks */
	u_int16_t	quirks;
#	define NO_QUIRKS		0x0000
	/* The drive does not support Test Unit Ready. Convert to Start Unit
	 */
#	define NO_TEST_UNIT_READY	0x0001
	/* The drive does not reset the Unit Attention state after REQUEST
	 * SENSE has been sent. The INQUIRY command does not reset the UA
	 * either, and so CAM runs in circles trying to retrieve the initial
	 * INQUIRY data.
	 */
#	define RS_NO_CLEAR_UA		0x0002
	/* The drive does not support START STOP.  */
#	define NO_START_STOP		0x0004
	/* Don't ask for full inquiry data (255b).  */
#	define FORCE_SHORT_INQUIRY	0x0008
	/* Needs to be initialised the Shuttle way */
#	define SHUTTLE_INIT		0x0010
	/* Drive needs to be switched to alternate iface 1 */
#	define ALT_IFACE_1		0x0020
	/* Drive does not do 1Mb/s, but just floppy speeds (20kb/s) */
#	define FLOPPY_SPEED		0x0040
	/* The device can't count and gets the residue of transfers wrong */
#	define IGNORE_RESIDUE		0x0080
	/* No GetMaxLun call */
#	define NO_GETMAXLUN		0x0100
	/* The device uses a weird CSWSIGNATURE. */
#	define WRONG_CSWSIG		0x0200
	/* Device cannot handle INQUIRY so fake a generic response */
#	define NO_INQUIRY		0x0400
	/* Device cannot handle INQUIRY EVPD, return CHECK CONDITION */
#	define NO_INQUIRY_EVPD		0x0800
	/* Pad all RBC requests to 12 bytes. */
#	define RBC_PAD_TO_12		0x1000
};

Static struct umass_devdescr_t umass_devdescrs[] = {
	{ USB_VENDOR_ASAHIOPTICAL, PID_WILDCARD, RID_WILDCARD,
	  UMASS_PROTO_ATAPI | UMASS_PROTO_CBI_I,
	  RS_NO_CLEAR_UA
	},
	{ USB_VENDOR_FUJIPHOTO, USB_PRODUCT_FUJIPHOTO_MASS0100, RID_WILDCARD,
	  UMASS_PROTO_ATAPI | UMASS_PROTO_CBI_I,
	  RS_NO_CLEAR_UA
	},
	{ USB_VENDOR_GENESYS,  USB_PRODUCT_GENESYS_GL641USB2IDE, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  FORCE_SHORT_INQUIRY | NO_START_STOP | IGNORE_RESIDUE
	},
	{ USB_VENDOR_GENESYS,  USB_PRODUCT_GENESYS_GL641USB2IDE_2, RID_WILDCARD,
	  UMASS_PROTO_ATAPI | UMASS_PROTO_BBB,
	  FORCE_SHORT_INQUIRY | NO_START_STOP | IGNORE_RESIDUE
	},
	{ USB_VENDOR_GENESYS,  USB_PRODUCT_GENESYS_GL641USB, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  FORCE_SHORT_INQUIRY | NO_START_STOP | IGNORE_RESIDUE
	},
	{ USB_VENDOR_HITACHI, USB_PRODUCT_HITACHI_DVDCAM_USB, RID_WILDCARD,
	  UMASS_PROTO_ATAPI | UMASS_PROTO_CBI_I,
	  NO_INQUIRY
	},
	{ USB_VENDOR_HP, USB_PRODUCT_HP_CDW8200, RID_WILDCARD,
	  UMASS_PROTO_ATAPI | UMASS_PROTO_CBI_I,
	  NO_TEST_UNIT_READY | NO_START_STOP
	},
	{ USB_VENDOR_IMAGINATION, USB_PRODUCT_IMAGINATION_DBX1, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  WRONG_CSWSIG
	},
	{ USB_VENDOR_INSYSTEM, USB_PRODUCT_INSYSTEM_USBCABLE, RID_WILDCARD,
	  UMASS_PROTO_ATAPI | UMASS_PROTO_CBI,
	  NO_TEST_UNIT_READY | NO_START_STOP | ALT_IFACE_1
	},
	{ USB_VENDOR_IODATA, USB_PRODUCT_IODATA_IU_CD2, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  NO_QUIRKS
	},
	{ USB_VENDOR_IODATA, USB_PRODUCT_IODATA_DVR_UEH8, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  NO_QUIRKS
	},
	{ USB_VENDOR_IOMEGA, USB_PRODUCT_IOMEGA_ZIP100, RID_WILDCARD,
	  /* XXX This is not correct as there are Zip drives that use ATAPI.
	   */
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  NO_TEST_UNIT_READY
	},
	{ USB_VENDOR_LOGITEC, USB_PRODUCT_LOGITEC_LDR_H443SU2, RID_WILDCARD,
	  UMASS_PROTO_SCSI,
	  NO_QUIRKS
	},
	{ USB_VENDOR_LOGITEC, USB_PRODUCT_LOGITEC_LDR_H443U2, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  NO_QUIRKS
	},
	{ USB_VENDOR_MELCO,  USB_PRODUCT_MELCO_DUBPXXG, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  FORCE_SHORT_INQUIRY | NO_START_STOP | IGNORE_RESIDUE
	},
	{ USB_VENDOR_MICROTECH, USB_PRODUCT_MICROTECH_DPCM, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_CBI,
	  NO_TEST_UNIT_READY | NO_START_STOP
	},
	{ USB_VENDOR_MSYSTEMS, USB_PRODUCT_MSYSTEMS_DISKONKEY, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  IGNORE_RESIDUE | NO_GETMAXLUN | RS_NO_CLEAR_UA
	},
	{ USB_VENDOR_MSYSTEMS, USB_PRODUCT_MSYSTEMS_DISKONKEY2, RID_WILDCARD,
	  UMASS_PROTO_ATAPI | UMASS_PROTO_BBB,
	  NO_QUIRKS
	},
	{ USB_VENDOR_NEODIO, USB_PRODUCT_NEODIO_ND3260, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  FORCE_SHORT_INQUIRY
	},
	{ USB_VENDOR_OLYMPUS, USB_PRODUCT_OLYMPUS_C1, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  WRONG_CSWSIG
	},
	{ USB_VENDOR_ONSPEC, USB_PRODUCT_ONSPEC_UCF100, RID_WILDCARD,
	  UMASS_PROTO_ATAPI | UMASS_PROTO_BBB,
	  NO_INQUIRY | NO_GETMAXLUN
	},
	{ USB_VENDOR_PANASONIC, USB_PRODUCT_PANASONIC_KXLCB20AN, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  NO_QUIRKS
	},
	{ USB_VENDOR_PANASONIC, USB_PRODUCT_PANASONIC_KXLCB35AN, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  NO_QUIRKS
	},
	{ USB_VENDOR_PLEXTOR, USB_PRODUCT_PLEXTOR_40_12_40U, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  NO_TEST_UNIT_READY
	},
	{ USB_VENDOR_PNY, USB_PRODUCT_PNY_ATTACHE, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  IGNORE_RESIDUE
	},
	{ USB_VENDOR_SANDISK, USB_PRODUCT_SANDISK_SDCZ2_256, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  IGNORE_RESIDUE
	},
	{ USB_VENDOR_SCANLOGIC, USB_PRODUCT_SCANLOGIC_SL11R, RID_WILDCARD,
	  UMASS_PROTO_ATAPI | UMASS_PROTO_BBB,
	  NO_INQUIRY
	},
	{ USB_VENDOR_SHUTTLE, USB_PRODUCT_SHUTTLE_EUSB, RID_WILDCARD,
	  UMASS_PROTO_ATAPI | UMASS_PROTO_CBI_I,
	  NO_TEST_UNIT_READY | NO_START_STOP | SHUTTLE_INIT
	},
	{ USB_VENDOR_SIGMATEL, USB_PRODUCT_SIGMATEL_I_BEAD100, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  SHUTTLE_INIT
	},
	{ USB_VENDOR_SIIG, USB_PRODUCT_SIIG_WINTERREADER, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  IGNORE_RESIDUE
	},
	{ USB_VENDOR_SONY, USB_PRODUCT_SONY_DSC, 0x0500,
	  UMASS_PROTO_RBC | UMASS_PROTO_CBI,
	  RBC_PAD_TO_12
	},
	{ USB_VENDOR_SONY, USB_PRODUCT_SONY_DSC, RID_WILDCARD,
	  UMASS_PROTO_RBC | UMASS_PROTO_CBI,
	  NO_QUIRKS
	},
	{ USB_VENDOR_SONY, USB_PRODUCT_SONY_HANDYCAM, RID_WILDCARD,
	  UMASS_PROTO_RBC | UMASS_PROTO_CBI,
	  NO_QUIRKS
	},
	{ USB_VENDOR_SONY, USB_PRODUCT_SONY_MSC, RID_WILDCARD,
	  UMASS_PROTO_RBC | UMASS_PROTO_CBI,
	  NO_QUIRKS
	},
	{ USB_VENDOR_TREK, USB_PRODUCT_TREK_THUMBDRIVE_8MB, RID_WILDCARD,
          UMASS_PROTO_ATAPI | UMASS_PROTO_BBB,
	  IGNORE_RESIDUE
	},
	{ USB_VENDOR_TRUMPION, USB_PRODUCT_TRUMPION_C3310, RID_WILDCARD,
	  UMASS_PROTO_UFI | UMASS_PROTO_CBI,
	  NO_QUIRKS
	},
	{ USB_VENDOR_TWINMOS, USB_PRODUCT_TWINMOS_MDIV, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  NO_QUIRKS
	},
	{ USB_VENDOR_WESTERN,  USB_PRODUCT_WESTERN_EXTHDD, RID_WILDCARD,
	  UMASS_PROTO_SCSI | UMASS_PROTO_BBB,
	  FORCE_SHORT_INQUIRY | NO_START_STOP | IGNORE_RESIDUE
	},
	{ USB_VENDOR_YANO,  USB_PRODUCT_YANO_U640MO, RID_WILDCARD,
	  UMASS_PROTO_ATAPI | UMASS_PROTO_CBI_I,
	  FORCE_SHORT_INQUIRY
	},
	{ VID_EOT, PID_EOT, RID_EOT, 0, 0 }
};


/* the per device structure */
struct umass_softc {
	USBBASEDEVICE		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* USB device */

	struct cam_sim		*umass_sim;	/* SCSI Interface Module */

	unsigned char		flags;		/* various device flags */
#	define UMASS_FLAGS_GONE		0x01	/* devices is no more */

	u_int16_t		proto;		/* wire and cmd protocol */
	u_int16_t		quirks;		/* they got it almost right */

	usbd_interface_handle	iface;		/* Mass Storage interface */
	int			ifaceno;	/* MS iface number */

	u_int8_t		bulkin;		/* bulk-in Endpoint Address */
	u_int8_t		bulkout;	/* bulk-out Endpoint Address */
	u_int8_t		intrin;		/* intr-in Endp. (CBI) */
	usbd_pipe_handle	bulkin_pipe;
	usbd_pipe_handle	bulkout_pipe;
	usbd_pipe_handle	intrin_pipe;

	/* Reset the device in a wire protocol specific way */
	wire_reset_f		reset;

	/* The start of a wire transfer. It prepares the whole transfer (cmd,
	 * data, and status stage) and initiates it. It is up to the state
	 * machine (below) to handle the various stages and errors in these
	 */
	wire_transfer_f		transfer;

	/* The state machine, handling the various states during a transfer */
	wire_state_f		state;

	/* The command transform function is used to conver the SCSI commands
	 * into their derivatives, like UFI, ATAPI, and friends.
	 */
	command_transform_f	transform;	/* command transform */

	/* Bulk specific variables for transfers in progress */
	umass_bbb_cbw_t		cbw;	/* command block wrapper */
	umass_bbb_csw_t		csw;	/* command status wrapper*/
	/* CBI specific variables for transfers in progress */
	umass_cbi_cbl_t		cbl;	/* command block */
	umass_cbi_sbl_t		sbl;	/* status block */

	/* generic variables for transfers in progress */
	/* ctrl transfer requests */
	usb_device_request_t	request;

	/* xfer handles
	 * Most of our operations are initiated from interrupt context, so
	 * we need to avoid using the one that is in use. We want to avoid
	 * allocating them in the interrupt context as well.
	 */
	/* indices into array below */
#	define XFER_BBB_CBW		0	/* Bulk-Only */
#	define XFER_BBB_DATA		1
#	define XFER_BBB_DCLEAR		2
#	define XFER_BBB_CSW1		3
#	define XFER_BBB_CSW2		4
#	define XFER_BBB_SCLEAR		5
#	define XFER_BBB_RESET1		6
#	define XFER_BBB_RESET2		7
#	define XFER_BBB_RESET3		8

#	define XFER_CBI_CB		0	/* CBI */
#	define XFER_CBI_DATA		1
#	define XFER_CBI_STATUS		2
#	define XFER_CBI_DCLEAR		3
#	define XFER_CBI_SCLEAR		4
#	define XFER_CBI_RESET1		5
#	define XFER_CBI_RESET2		6
#	define XFER_CBI_RESET3		7

#	define XFER_NR			9	/* maximum number */

	usbd_xfer_handle	transfer_xfer[XFER_NR];	/* for ctrl xfers */

	int			transfer_dir;		/* data direction */
	void			*transfer_data;		/* data buffer */
	int			transfer_datalen;	/* (maximum) length */
	int			transfer_actlen;	/* actual length */
	transfer_cb_f		transfer_cb;		/* callback */
	void			*transfer_priv;		/* for callback */
	int			transfer_status;

	int			transfer_state;
#	define TSTATE_ATTACH			0	/* in attach */
#	define TSTATE_IDLE			1
#	define TSTATE_BBB_COMMAND		2	/* CBW transfer */
#	define TSTATE_BBB_DATA			3	/* Data transfer */
#	define TSTATE_BBB_DCLEAR		4	/* clear endpt stall */
#	define TSTATE_BBB_STATUS1		5	/* clear endpt stall */
#	define TSTATE_BBB_SCLEAR		6	/* clear endpt stall */
#	define TSTATE_BBB_STATUS2		7	/* CSW transfer */
#	define TSTATE_BBB_RESET1		8	/* reset command */
#	define TSTATE_BBB_RESET2		9	/* in clear stall */
#	define TSTATE_BBB_RESET3		10	/* out clear stall */
#	define TSTATE_CBI_COMMAND		11	/* command transfer */
#	define TSTATE_CBI_DATA			12	/* data transfer */
#	define TSTATE_CBI_STATUS		13	/* status transfer */
#	define TSTATE_CBI_DCLEAR		14	/* clear ep stall */
#	define TSTATE_CBI_SCLEAR		15	/* clear ep stall */
#	define TSTATE_CBI_RESET1		16	/* reset command */
#	define TSTATE_CBI_RESET2		17	/* in clear stall */
#	define TSTATE_CBI_RESET3		18	/* out clear stall */
#	define TSTATE_STATES			19	/* # of states above */


	/* SCSI/CAM specific variables */
	unsigned char 		cam_scsi_command[CAM_MAX_CDBLEN];
	unsigned char 		cam_scsi_command2[CAM_MAX_CDBLEN];
	struct scsi_sense	cam_scsi_sense;
	struct scsi_sense	cam_scsi_test_unit_ready;
	usb_callout_t		cam_scsi_rescan_ch;

	int			timeout;		/* in msecs */

	int			maxlun;			/* maximum LUN number */
};

#ifdef USB_DEBUG
char *states[TSTATE_STATES+1] = {
	/* should be kept in sync with the list at transfer_state */
	"Attach",
	"Idle",
	"BBB CBW",
	"BBB Data",
	"BBB Data bulk-in/-out clear stall",
	"BBB CSW, 1st attempt",
	"BBB CSW bulk-in clear stall",
	"BBB CSW, 2nd attempt",
	"BBB Reset",
	"BBB bulk-in clear stall",
	"BBB bulk-out clear stall",
	"CBI Command",
	"CBI Data",
	"CBI Status",
	"CBI Data bulk-in/-out clear stall",
	"CBI Status intr-in clear stall",
	"CBI Reset",
	"CBI bulk-in clear stall",
	"CBI bulk-out clear stall",
	NULL
};
#endif

/* If device cannot return valid inquiry data, fake it */
Static uint8_t fake_inq_data[SHORT_INQUIRY_LENGTH] = {
	0, /*removable*/ 0x80, SCSI_REV_2, SCSI_REV_2,
	/*additional_length*/ 31, 0, 0, 0
};

/* USB device probe/attach/detach functions */
USB_DECLARE_DRIVER(umass);
Static int umass_match_proto	(struct umass_softc *sc,
				usbd_interface_handle iface,
				usbd_device_handle udev);

/* quirk functions */
Static void umass_init_shuttle	(struct umass_softc *sc);

/* generic transfer functions */
Static usbd_status umass_setup_transfer	(struct umass_softc *sc,
				usbd_pipe_handle pipe,
				void *buffer, int buflen, int flags,
				usbd_xfer_handle xfer);
Static usbd_status umass_setup_ctrl_transfer	(struct umass_softc *sc,
				usbd_device_handle udev,
				usb_device_request_t *req,
				void *buffer, int buflen, int flags,
				usbd_xfer_handle xfer);
Static void umass_clear_endpoint_stall	(struct umass_softc *sc,
				u_int8_t endpt, usbd_pipe_handle pipe,
				int state, usbd_xfer_handle xfer);
Static void umass_reset		(struct umass_softc *sc,
				transfer_cb_f cb, void *priv);

/* Bulk-Only related functions */
Static void umass_bbb_reset	(struct umass_softc *sc, int status);
Static void umass_bbb_transfer	(struct umass_softc *sc, int lun,
				void *cmd, int cmdlen,
		    		void *data, int datalen, int dir, u_int timeout,
				transfer_cb_f cb, void *priv);
Static void umass_bbb_state	(usbd_xfer_handle xfer,
				usbd_private_handle priv,
				usbd_status err);
Static int umass_bbb_get_max_lun
				(struct umass_softc *sc);

/* CBI related functions */
Static int umass_cbi_adsc	(struct umass_softc *sc,
				char *buffer, int buflen,
				usbd_xfer_handle xfer);
Static void umass_cbi_reset	(struct umass_softc *sc, int status);
Static void umass_cbi_transfer	(struct umass_softc *sc, int lun,
				void *cmd, int cmdlen,
		    		void *data, int datalen, int dir, u_int timeout,
				transfer_cb_f cb, void *priv);
Static void umass_cbi_state	(usbd_xfer_handle xfer,
				usbd_private_handle priv, usbd_status err);

/* CAM related functions */
Static void umass_cam_action	(struct cam_sim *sim, union ccb *ccb);
Static void umass_cam_poll	(struct cam_sim *sim);

Static void umass_cam_cb	(struct umass_softc *sc, void *priv,
				int residue, int status);
Static void umass_cam_sense_cb	(struct umass_softc *sc, void *priv,
				int residue, int status);
Static void umass_cam_quirk_cb	(struct umass_softc *sc, void *priv,
				int residue, int status);

Static void umass_cam_rescan_callback
				(struct cam_periph *periph,union ccb *ccb);
Static void umass_cam_rescan	(void *addr);

Static int umass_cam_attach_sim	(struct umass_softc *sc);
Static int umass_cam_attach	(struct umass_softc *sc);
Static int umass_cam_detach_sim	(struct umass_softc *sc);


/* SCSI specific functions */
Static int umass_scsi_transform	(struct umass_softc *sc,
				unsigned char *cmd, int cmdlen,
		     		unsigned char **rcmd, int *rcmdlen);

/* UFI specific functions */
#define UFI_COMMAND_LENGTH	12	/* UFI commands are always 12 bytes */
Static int umass_ufi_transform	(struct umass_softc *sc,
				unsigned char *cmd, int cmdlen,
		    		unsigned char **rcmd, int *rcmdlen);

/* ATAPI (8070i) specific functions */
#define ATAPI_COMMAND_LENGTH	12	/* ATAPI commands are always 12 bytes */
Static int umass_atapi_transform	(struct umass_softc *sc,
				unsigned char *cmd, int cmdlen,
		     		unsigned char **rcmd, int *rcmdlen);

/* RBC specific functions */
Static int umass_rbc_transform	(struct umass_softc *sc,
				unsigned char *cmd, int cmdlen,
		     		unsigned char **rcmd, int *rcmdlen);

#ifdef USB_DEBUG
/* General debugging functions */
Static void umass_bbb_dump_cbw	(struct umass_softc *sc, umass_bbb_cbw_t *cbw);
Static void umass_bbb_dump_csw	(struct umass_softc *sc, umass_bbb_csw_t *csw);
Static void umass_cbi_dump_cmd	(struct umass_softc *sc, void *cmd, int cmdlen);
Static void umass_dump_buffer	(struct umass_softc *sc, u_int8_t *buffer,
				int buflen, int printlen);
#endif

#if defined(__FreeBSD__)
MODULE_DEPEND(umass, cam, 1,1,1);
#endif

/*
 * USB device probe/attach/detach
 */

/*
 * Match the device we are seeing with the devices supported. Fill in the
 * description in the softc accordingly. This function is called from both
 * probe and attach.
 */

Static int
umass_match_proto(struct umass_softc *sc, usbd_interface_handle iface,
		  usbd_device_handle udev)
{
	usb_device_descriptor_t *dd;
	usb_interface_descriptor_t *id;
	int i;
	int found = 0;

	sc->sc_udev = udev;
	sc->proto = 0;
	sc->quirks = 0;

	dd = usbd_get_device_descriptor(udev);

	/* An entry specifically for Y-E Data devices as they don't fit in the
	 * device description table.
	 */
	if (UGETW(dd->idVendor) == USB_VENDOR_YEDATA
	    && UGETW(dd->idProduct) == USB_PRODUCT_YEDATA_FLASHBUSTERU) {

		/* Revisions < 1.28 do not handle the interrupt endpoint
		 * very well.
		 */
		if (UGETW(dd->bcdDevice) < 0x128) {
			sc->proto = UMASS_PROTO_UFI | UMASS_PROTO_CBI;
		} else {
			sc->proto = UMASS_PROTO_UFI | UMASS_PROTO_CBI_I;
		}

		/*
		 * Revisions < 1.28 do not have the TEST UNIT READY command
		 * Revisions == 1.28 have a broken TEST UNIT READY
		 */
		if (UGETW(dd->bcdDevice) <= 0x128)
			sc->quirks |= NO_TEST_UNIT_READY;

		sc->quirks |= RS_NO_CLEAR_UA | FLOPPY_SPEED;
		return(UMATCH_VENDOR_PRODUCT);
	}

	/* Check the list of supported devices for a match. While looking,
	 * check for wildcarded and fully matched. First match wins.
	 */
	for (i = 0; umass_devdescrs[i].vid != VID_EOT && !found; i++) {
		if (umass_devdescrs[i].vid == VID_WILDCARD &&
		    umass_devdescrs[i].pid == PID_WILDCARD &&
		    umass_devdescrs[i].rid == RID_WILDCARD) {
			printf("umass: ignoring invalid wildcard quirk\n");
			continue;
		}
		if ((umass_devdescrs[i].vid == UGETW(dd->idVendor) ||
		     umass_devdescrs[i].vid == VID_WILDCARD)
		 && (umass_devdescrs[i].pid == UGETW(dd->idProduct) ||
		     umass_devdescrs[i].pid == PID_WILDCARD)) {
		    	if (umass_devdescrs[i].rid == RID_WILDCARD) {
				sc->proto = umass_devdescrs[i].proto;
				sc->quirks = umass_devdescrs[i].quirks;
				return (UMATCH_VENDOR_PRODUCT);
			} else if (umass_devdescrs[i].rid ==
			    UGETW(dd->bcdDevice)) {
				sc->proto = umass_devdescrs[i].proto;
				sc->quirks = umass_devdescrs[i].quirks;
				return (UMATCH_VENDOR_PRODUCT_REV);
			} /* else RID does not match */
		}
	}

	/* Check for a standards compliant device */
	id = usbd_get_interface_descriptor(iface);
	if (id == NULL || id->bInterfaceClass != UICLASS_MASS)
		return(UMATCH_NONE);

	switch (id->bInterfaceSubClass) {
	case UISUBCLASS_SCSI:
		sc->proto |= UMASS_PROTO_SCSI;
		break;
	case UISUBCLASS_UFI:
		sc->proto |= UMASS_PROTO_UFI;
		break;
	case UISUBCLASS_RBC:
		sc->proto |= UMASS_PROTO_RBC;
		break;
	case UISUBCLASS_SFF8020I:
	case UISUBCLASS_SFF8070I:
		sc->proto |= UMASS_PROTO_ATAPI;
		break;
	default:
		DPRINTF(UDMASS_GEN, ("%s: Unsupported command protocol %d\n",
			USBDEVNAME(sc->sc_dev), id->bInterfaceSubClass));
		return(UMATCH_NONE);
	}

	switch (id->bInterfaceProtocol) {
	case UIPROTO_MASS_CBI:
		sc->proto |= UMASS_PROTO_CBI;
		break;
	case UIPROTO_MASS_CBI_I:
		sc->proto |= UMASS_PROTO_CBI_I;
		break;
	case UIPROTO_MASS_BBB_OLD:
	case UIPROTO_MASS_BBB:
		sc->proto |= UMASS_PROTO_BBB;
		break;
	default:
		DPRINTF(UDMASS_GEN, ("%s: Unsupported wire protocol %d\n",
			USBDEVNAME(sc->sc_dev), id->bInterfaceProtocol));
		return(UMATCH_NONE);
	}

	return(UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO);
}

USB_MATCH(umass)
{
	USB_MATCH_START(umass, uaa);
	struct umass_softc *sc = device_get_softc(self);

	USB_MATCH_SETUP;

	if (uaa->iface == NULL)
		return(UMATCH_NONE);

	return(umass_match_proto(sc, uaa->iface, uaa->device));
}

USB_ATTACH(umass)
{
	USB_ATTACH_START(umass, sc, uaa);
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char devinfo[1024];
	int i;
	int err;

	/*
	 * the softc struct is bzero-ed in device_set_driver. We can safely
	 * call umass_detach without specifically initialising the struct.
	 */

	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;

	sc->iface = uaa->iface;
	sc->ifaceno = uaa->ifaceno;
	usb_callout_init(sc->cam_scsi_rescan_ch);

	/* initialise the proto and drive values in the umass_softc (again) */
	(void) umass_match_proto(sc, sc->iface, uaa->device);

	id = usbd_get_interface_descriptor(sc->iface);
#ifdef USB_DEBUG
	printf("%s: ", USBDEVNAME(sc->sc_dev));
	switch (sc->proto&UMASS_PROTO_COMMAND) {
	case UMASS_PROTO_SCSI:
		printf("SCSI");
		break;
	case UMASS_PROTO_ATAPI:
		printf("8070i (ATAPI)");
		break;
	case UMASS_PROTO_UFI:
		printf("UFI");
		break;
	case UMASS_PROTO_RBC:
		printf("RBC");
		break;
	default:
		printf("(unknown 0x%02x)", sc->proto&UMASS_PROTO_COMMAND);
		break;
	}
	printf(" over ");
	switch (sc->proto&UMASS_PROTO_WIRE) {
	case UMASS_PROTO_BBB:
		printf("Bulk-Only");
		break;
	case UMASS_PROTO_CBI:			/* uses Comand/Bulk pipes */
		printf("CBI");
		break;
	case UMASS_PROTO_CBI_I:		/* uses Comand/Bulk/Interrupt pipes */
		printf("CBI with CCI");
#ifndef CBI_I
		printf(" (using CBI)");
#endif
		break;
	default:
		printf("(unknown 0x%02x)", sc->proto&UMASS_PROTO_WIRE);
	}
	printf("; quirks = 0x%04x\n", sc->quirks);
#endif

#ifndef CBI_I
	if (sc->proto & UMASS_PROTO_CBI_I) {
		/* See beginning of file for comment on the use of CBI with CCI */
		sc->proto = (sc->proto & ~UMASS_PROTO_CBI_I) | UMASS_PROTO_CBI;
	}
#endif

	if (sc->quirks & ALT_IFACE_1) {
		err = usbd_set_interface(uaa->iface, 1);
		if (err) {
			DPRINTF(UDMASS_USB, ("%s: could not switch to "
				"Alt Interface %d\n",
				USBDEVNAME(sc->sc_dev), 1));
			umass_detach(self);
			USB_ATTACH_ERROR_RETURN;
		}
	}

	/*
	 * In addition to the Control endpoint the following endpoints
	 * are required:
	 * a) bulk-in endpoint.
	 * b) bulk-out endpoint.
	 * and for Control/Bulk/Interrupt with CCI (CBI_I)
	 * c) intr-in
	 *
	 * The endpoint addresses are not fixed, so we have to read them
	 * from the device descriptors of the current interface.
	 */
	for (i = 0 ; i < id->bNumEndpoints ; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->iface, i);
		if (!ed) {
			printf("%s: could not read endpoint descriptor\n",
			       USBDEVNAME(sc->sc_dev));
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->bulkin = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->bulkout = ed->bEndpointAddress;
		} else if (sc->proto & UMASS_PROTO_CBI_I
		    && UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
			sc->intrin = ed->bEndpointAddress;
#ifdef USB_DEBUG
			if (UGETW(ed->wMaxPacketSize) > 2) {
				DPRINTF(UDMASS_CBI, ("%s: intr size is %d\n",
					USBDEVNAME(sc->sc_dev),
					UGETW(ed->wMaxPacketSize)));
			}
#endif
		}
	}

	/* check whether we found all the endpoints we need */
	if (!sc->bulkin || !sc->bulkout
	    || (sc->proto & UMASS_PROTO_CBI_I && !sc->intrin) ) {
	    	DPRINTF(UDMASS_USB, ("%s: endpoint not found %d/%d/%d\n",
			USBDEVNAME(sc->sc_dev),
			sc->bulkin, sc->bulkout, sc->intrin));
		umass_detach(self);
		USB_ATTACH_ERROR_RETURN;
	}

	/* Open the bulk-in and -out pipe */
	err = usbd_open_pipe(sc->iface, sc->bulkout,
				USBD_EXCLUSIVE_USE, &sc->bulkout_pipe);
	if (err) {
		DPRINTF(UDMASS_USB, ("%s: cannot open %d-out pipe (bulk)\n",
			USBDEVNAME(sc->sc_dev), sc->bulkout));
		umass_detach(self);
		USB_ATTACH_ERROR_RETURN;
	}
	err = usbd_open_pipe(sc->iface, sc->bulkin,
				USBD_EXCLUSIVE_USE, &sc->bulkin_pipe);
	if (err) {
		DPRINTF(UDMASS_USB, ("%s: could not open %d-in pipe (bulk)\n",
			USBDEVNAME(sc->sc_dev), sc->bulkin));
		umass_detach(self);
		USB_ATTACH_ERROR_RETURN;
	}
	/* Open the intr-in pipe if the protocol is CBI with CCI.
	 * Note: early versions of the Zip drive do have an interrupt pipe, but
	 * this pipe is unused.
	 *
	 * We do not open the interrupt pipe as an interrupt pipe, but as a
	 * normal bulk endpoint. We send an IN transfer down the wire at the
	 * appropriate time, because we know exactly when to expect data on
	 * that endpoint. This saves bandwidth, but more important, makes the
	 * code for handling the data on that endpoint simpler. No data
	 * arriving concurrently.
	 */
	if (sc->proto & UMASS_PROTO_CBI_I) {
		err = usbd_open_pipe(sc->iface, sc->intrin,
				USBD_EXCLUSIVE_USE, &sc->intrin_pipe);
		if (err) {
			DPRINTF(UDMASS_USB, ("%s: couldn't open %d-in (intr)\n",
				USBDEVNAME(sc->sc_dev), sc->intrin));
			umass_detach(self);
			USB_ATTACH_ERROR_RETURN;
		}
	}

	/* initialisation of generic part */
	sc->transfer_state = TSTATE_ATTACH;

	/* request a sufficient number of xfer handles */
	for (i = 0; i < XFER_NR; i++) {
		sc->transfer_xfer[i] = usbd_alloc_xfer(uaa->device);
		if (!sc->transfer_xfer[i]) {
			DPRINTF(UDMASS_USB, ("%s: Out of memory\n",
				USBDEVNAME(sc->sc_dev)));
			umass_detach(self);
			USB_ATTACH_ERROR_RETURN;
		}
	}

	/* Initialise the wire protocol specific methods */
	if (sc->proto & UMASS_PROTO_BBB) {
		sc->reset = umass_bbb_reset;
		sc->transfer = umass_bbb_transfer;
		sc->state = umass_bbb_state;
	} else if (sc->proto & (UMASS_PROTO_CBI|UMASS_PROTO_CBI_I)) {
		sc->reset = umass_cbi_reset;
		sc->transfer = umass_cbi_transfer;
		sc->state = umass_cbi_state;
#ifdef USB_DEBUG
	} else {
		panic("%s:%d: Unknown proto 0x%02x",
		      __FILE__, __LINE__, sc->proto);
#endif
	}

	if (sc->proto & UMASS_PROTO_SCSI)
		sc->transform = umass_scsi_transform;
	else if (sc->proto & UMASS_PROTO_UFI)
		sc->transform = umass_ufi_transform;
	else if (sc->proto & UMASS_PROTO_ATAPI)
		sc->transform = umass_atapi_transform;
	else if (sc->proto & UMASS_PROTO_RBC)
		sc->transform = umass_rbc_transform;
#ifdef USB_DEBUG
	else
		panic("No transformation defined for command proto 0x%02x",
		      sc->proto & UMASS_PROTO_COMMAND);
#endif

	/* From here onwards the device can be used. */

	if (sc->quirks & SHUTTLE_INIT)
		umass_init_shuttle(sc);

	/* Get the maximum LUN supported by the device.
	 */
	if (((sc->proto & UMASS_PROTO_WIRE) == UMASS_PROTO_BBB) &&
	    !(sc->quirks & NO_GETMAXLUN))
		sc->maxlun = umass_bbb_get_max_lun(sc);
	else
		sc->maxlun = 0;

	if ((sc->proto & UMASS_PROTO_SCSI) ||
	    (sc->proto & UMASS_PROTO_ATAPI) ||
	    (sc->proto & UMASS_PROTO_UFI) ||
	    (sc->proto & UMASS_PROTO_RBC)) {
		/* Prepare the SCSI command block */
		sc->cam_scsi_sense.opcode = REQUEST_SENSE;
		sc->cam_scsi_test_unit_ready.opcode = TEST_UNIT_READY;

		/* register the SIM */
		err = umass_cam_attach_sim(sc);
		if (err) {
			umass_detach(self);
			USB_ATTACH_ERROR_RETURN;
		}
		/* scan the new sim */
		err = umass_cam_attach(sc);
		if (err) {
			umass_cam_detach_sim(sc);
			umass_detach(self);
			USB_ATTACH_ERROR_RETURN;
		}
	} else {
		panic("%s:%d: Unknown proto 0x%02x",
		      __FILE__, __LINE__, sc->proto);
	}

	sc->transfer_state = TSTATE_IDLE;
	DPRINTF(UDMASS_GEN, ("%s: Attach finished\n", USBDEVNAME(sc->sc_dev)));

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(umass)
{
	USB_DETACH_START(umass, sc);
	int err = 0;
	int i;

	DPRINTF(UDMASS_USB, ("%s: detached\n", USBDEVNAME(sc->sc_dev)));

	sc->flags |= UMASS_FLAGS_GONE;

	/* abort all the pipes in case there are transfers active. */
	usbd_abort_default_pipe(sc->sc_udev);
	if (sc->bulkout_pipe)
		usbd_abort_pipe(sc->bulkout_pipe);
	if (sc->bulkin_pipe)
		usbd_abort_pipe(sc->bulkin_pipe);
	if (sc->intrin_pipe)
		usbd_abort_pipe(sc->intrin_pipe);

	usb_uncallout_drain(sc->cam_scsi_rescan_ch, umass_cam_rescan, sc);
	if ((sc->proto & UMASS_PROTO_SCSI) ||
	    (sc->proto & UMASS_PROTO_ATAPI) ||
	    (sc->proto & UMASS_PROTO_UFI) ||
	    (sc->proto & UMASS_PROTO_RBC))
		/* detach the SCSI host controller (SIM) */
		err = umass_cam_detach_sim(sc);

	for (i = 0; i < XFER_NR; i++)
		if (sc->transfer_xfer[i])
			usbd_free_xfer(sc->transfer_xfer[i]);

	/* remove all the pipes */
	if (sc->bulkout_pipe)
		usbd_close_pipe(sc->bulkout_pipe);
	if (sc->bulkin_pipe)
		usbd_close_pipe(sc->bulkin_pipe);
	if (sc->intrin_pipe)
		usbd_close_pipe(sc->intrin_pipe);

	return(err);
}

Static void
umass_init_shuttle(struct umass_softc *sc)
{
	usb_device_request_t req;
	u_char status[2];

	/* The Linux driver does this, but no one can tell us what the
	 * command does.
	 */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = 1;	/* XXX unknown command */
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->ifaceno);
	USETW(req.wLength, sizeof status);
	(void) usbd_do_request(sc->sc_udev, &req, &status);

	DPRINTF(UDMASS_GEN, ("%s: Shuttle init returned 0x%02x%02x\n",
		USBDEVNAME(sc->sc_dev), status[0], status[1]));
}

 /*
 * Generic functions to handle transfers
 */

Static usbd_status
umass_setup_transfer(struct umass_softc *sc, usbd_pipe_handle pipe,
			void *buffer, int buflen, int flags,
			usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Initialise a USB transfer and then schedule it */

	(void) usbd_setup_xfer(xfer, pipe, (void *) sc, buffer, buflen, flags,
			sc->timeout, sc->state);

	err = usbd_transfer(xfer);
	if (err && err != USBD_IN_PROGRESS) {
		DPRINTF(UDMASS_BBB, ("%s: failed to setup transfer, %s\n",
			USBDEVNAME(sc->sc_dev), usbd_errstr(err)));
		return(err);
	}

	return (USBD_NORMAL_COMPLETION);
}


Static usbd_status
umass_setup_ctrl_transfer(struct umass_softc *sc, usbd_device_handle udev,
	 usb_device_request_t *req,
	 void *buffer, int buflen, int flags,
	 usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Initialise a USB control transfer and then schedule it */

	(void) usbd_setup_default_xfer(xfer, udev, (void *) sc,
			sc->timeout, req, buffer, buflen, flags, sc->state);

	err = usbd_transfer(xfer);
	if (err && err != USBD_IN_PROGRESS) {
		DPRINTF(UDMASS_BBB, ("%s: failed to setup ctrl transfer, %s\n",
			 USBDEVNAME(sc->sc_dev), usbd_errstr(err)));

		/* do not reset, as this would make us loop */
		return(err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static void
umass_clear_endpoint_stall(struct umass_softc *sc,
				u_int8_t endpt, usbd_pipe_handle pipe,
				int state, usbd_xfer_handle xfer)
{
	usbd_device_handle udev;

	DPRINTF(UDMASS_BBB, ("%s: Clear endpoint 0x%02x stall\n",
		USBDEVNAME(sc->sc_dev), endpt));

	usbd_interface2device_handle(sc->iface, &udev);

	sc->transfer_state = state;

	usbd_clear_endpoint_toggle(pipe);

	sc->request.bmRequestType = UT_WRITE_ENDPOINT;
	sc->request.bRequest = UR_CLEAR_FEATURE;
	USETW(sc->request.wValue, UF_ENDPOINT_HALT);
	USETW(sc->request.wIndex, endpt);
	USETW(sc->request.wLength, 0);
	umass_setup_ctrl_transfer(sc, udev, &sc->request, NULL, 0, 0, xfer);
}

Static void
umass_reset(struct umass_softc *sc, transfer_cb_f cb, void *priv)
{
	sc->transfer_cb = cb;
	sc->transfer_priv = priv;

	/* The reset is a forced reset, so no error (yet) */
	sc->reset(sc, STATUS_CMD_OK);
}

/*
 * Bulk protocol specific functions
 */

Static void
umass_bbb_reset(struct umass_softc *sc, int status)
{
	usbd_device_handle udev;

	KASSERT(sc->proto & UMASS_PROTO_BBB,
		("%s: umass_bbb_reset: wrong sc->proto 0x%02x\n",
			USBDEVNAME(sc->sc_dev), sc->proto));

	/*
	 * Reset recovery (5.3.4 in Universal Serial Bus Mass Storage Class)
	 *
	 * For Reset Recovery the host shall issue in the following order:
	 * a) a Bulk-Only Mass Storage Reset
	 * b) a Clear Feature HALT to the Bulk-In endpoint
	 * c) a Clear Feature HALT to the Bulk-Out endpoint
	 *
	 * This is done in 3 steps, states:
	 * TSTATE_BBB_RESET1
	 * TSTATE_BBB_RESET2
	 * TSTATE_BBB_RESET3
	 *
	 * If the reset doesn't succeed, the device should be port reset.
	 */

	DPRINTF(UDMASS_BBB, ("%s: Bulk Reset\n",
		USBDEVNAME(sc->sc_dev)));

	sc->transfer_state = TSTATE_BBB_RESET1;
	sc->transfer_status = status;

	usbd_interface2device_handle(sc->iface, &udev);

	/* reset is a class specific interface write */
	sc->request.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	sc->request.bRequest = UR_BBB_RESET;
	USETW(sc->request.wValue, 0);
	USETW(sc->request.wIndex, sc->ifaceno);
	USETW(sc->request.wLength, 0);
	umass_setup_ctrl_transfer(sc, udev, &sc->request, NULL, 0, 0,
				  sc->transfer_xfer[XFER_BBB_RESET1]);
}

Static void
umass_bbb_transfer(struct umass_softc *sc, int lun, void *cmd, int cmdlen,
		    void *data, int datalen, int dir, u_int timeout,
		    transfer_cb_f cb, void *priv)
{
	KASSERT(sc->proto & UMASS_PROTO_BBB,
		("%s: umass_bbb_transfer: wrong sc->proto 0x%02x\n",
			USBDEVNAME(sc->sc_dev), sc->proto));

	/* Be a little generous. */
	sc->timeout = timeout + UMASS_TIMEOUT;

	/*
	 * Do a Bulk-Only transfer with cmdlen bytes from cmd, possibly
	 * a data phase of datalen bytes from/to the device and finally a
	 * csw read phase.
	 * If the data direction was inbound a maximum of datalen bytes
	 * is stored in the buffer pointed to by data.
	 *
	 * umass_bbb_transfer initialises the transfer and lets the state
	 * machine in umass_bbb_state handle the completion. It uses the
	 * following states:
	 * TSTATE_BBB_COMMAND
	 *   -> TSTATE_BBB_DATA
	 *   -> TSTATE_BBB_STATUS
	 *   -> TSTATE_BBB_STATUS2
	 *   -> TSTATE_BBB_IDLE
	 *
	 * An error in any of those states will invoke
	 * umass_bbb_reset.
	 */

	/* check the given arguments */
	KASSERT(datalen == 0 || data != NULL,
		("%s: datalen > 0, but no buffer",USBDEVNAME(sc->sc_dev)));
	KASSERT(cmdlen <= CBWCDBLENGTH,
		("%s: cmdlen exceeds CDB length in CBW (%d > %d)",
			USBDEVNAME(sc->sc_dev), cmdlen, CBWCDBLENGTH));
	KASSERT(dir == DIR_NONE || datalen > 0,
		("%s: datalen == 0 while direction is not NONE\n",
			USBDEVNAME(sc->sc_dev)));
	KASSERT(datalen == 0 || dir != DIR_NONE,
		("%s: direction is NONE while datalen is not zero\n",
			USBDEVNAME(sc->sc_dev)));
	KASSERT(sizeof(umass_bbb_cbw_t) == UMASS_BBB_CBW_SIZE,
		("%s: CBW struct does not have the right size (%ld vs. %d)\n",
			USBDEVNAME(sc->sc_dev),
			(long)sizeof(umass_bbb_cbw_t), UMASS_BBB_CBW_SIZE));
	KASSERT(sizeof(umass_bbb_csw_t) == UMASS_BBB_CSW_SIZE,
		("%s: CSW struct does not have the right size (%ld vs. %d)\n",
			USBDEVNAME(sc->sc_dev),
			(long)sizeof(umass_bbb_csw_t), UMASS_BBB_CSW_SIZE));

	/*
	 * Determine the direction of the data transfer and the length.
	 *
	 * dCBWDataTransferLength (datalen) :
	 *   This field indicates the number of bytes of data that the host
	 *   intends to transfer on the IN or OUT Bulk endpoint(as indicated by
	 *   the Direction bit) during the execution of this command. If this
	 *   field is set to 0, the device will expect that no data will be
	 *   transferred IN or OUT during this command, regardless of the value
	 *   of the Direction bit defined in dCBWFlags.
	 *
	 * dCBWFlags (dir) :
	 *   The bits of the Flags field are defined as follows:
	 *     Bits 0-6  reserved
	 *     Bit  7    Direction - this bit shall be ignored if the
	 *                           dCBWDataTransferLength field is zero.
	 *               0 = data Out from host to device
	 *               1 = data In from device to host
	 */

	/* Fill in the Command Block Wrapper
	 * We fill in all the fields, so there is no need to bzero it first.
	 */
	USETDW(sc->cbw.dCBWSignature, CBWSIGNATURE);
	/* We don't care about the initial value, as long as the values are unique */
	USETDW(sc->cbw.dCBWTag, UGETDW(sc->cbw.dCBWTag) + 1);
	USETDW(sc->cbw.dCBWDataTransferLength, datalen);
	/* DIR_NONE is treated as DIR_OUT (0x00) */
	sc->cbw.bCBWFlags = (dir == DIR_IN? CBWFLAGS_IN:CBWFLAGS_OUT);
	sc->cbw.bCBWLUN = lun;
	sc->cbw.bCDBLength = cmdlen;
	bcopy(cmd, sc->cbw.CBWCDB, cmdlen);

	DIF(UDMASS_BBB, umass_bbb_dump_cbw(sc, &sc->cbw));

	/* store the details for the data transfer phase */
	sc->transfer_dir = dir;
	sc->transfer_data = data;
	sc->transfer_datalen = datalen;
	sc->transfer_actlen = 0;
	sc->transfer_cb = cb;
	sc->transfer_priv = priv;
	sc->transfer_status = STATUS_CMD_OK;

	/* move from idle to the command state */
	sc->transfer_state = TSTATE_BBB_COMMAND;

	/* Send the CBW from host to device via bulk-out endpoint. */
	if (umass_setup_transfer(sc, sc->bulkout_pipe,
			&sc->cbw, UMASS_BBB_CBW_SIZE, 0,
			sc->transfer_xfer[XFER_BBB_CBW])) {
		umass_bbb_reset(sc, STATUS_WIRE_FAILED);
	}
}


Static void
umass_bbb_state(usbd_xfer_handle xfer, usbd_private_handle priv,
		usbd_status err)
{
	struct umass_softc *sc = (struct umass_softc *) priv;
	usbd_xfer_handle next_xfer;

	KASSERT(sc->proto & UMASS_PROTO_BBB,
		("%s: umass_bbb_state: wrong sc->proto 0x%02x\n",
			USBDEVNAME(sc->sc_dev), sc->proto));

	/*
	 * State handling for BBB transfers.
	 *
	 * The subroutine is rather long. It steps through the states given in
	 * Annex A of the Bulk-Only specification.
	 * Each state first does the error handling of the previous transfer
	 * and then prepares the next transfer.
	 * Each transfer is done asynchronously so after the request/transfer
	 * has been submitted you will find a 'return;'.
	 */

	DPRINTF(UDMASS_BBB, ("%s: Handling BBB state %d (%s), xfer=%p, %s\n",
		USBDEVNAME(sc->sc_dev), sc->transfer_state,
		states[sc->transfer_state], xfer, usbd_errstr(err)));

	/* Give up if the device has detached. */
	if (sc->flags & UMASS_FLAGS_GONE) {
		sc->transfer_state = TSTATE_IDLE;
		sc->transfer_cb(sc, sc->transfer_priv, sc->transfer_datalen,
		    STATUS_CMD_FAILED);
		return;
	}

	switch (sc->transfer_state) {

	/***** Bulk Transfer *****/
	case TSTATE_BBB_COMMAND:
		/* Command transport phase, error handling */
		if (err) {
			DPRINTF(UDMASS_BBB, ("%s: failed to send CBW\n",
				USBDEVNAME(sc->sc_dev)));
			/* If the device detects that the CBW is invalid, then
			 * the device may STALL both bulk endpoints and require
			 * a Bulk-Reset
			 */
			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		}

		/* Data transport phase, setup transfer */
		sc->transfer_state = TSTATE_BBB_DATA;
		if (sc->transfer_dir == DIR_IN) {
			if (umass_setup_transfer(sc, sc->bulkin_pipe,
					sc->transfer_data, sc->transfer_datalen,
					USBD_SHORT_XFER_OK,
					sc->transfer_xfer[XFER_BBB_DATA]))
				umass_bbb_reset(sc, STATUS_WIRE_FAILED);

			return;
		} else if (sc->transfer_dir == DIR_OUT) {
			if (umass_setup_transfer(sc, sc->bulkout_pipe,
					sc->transfer_data, sc->transfer_datalen,
					0,	/* fixed length transfer */
					sc->transfer_xfer[XFER_BBB_DATA]))
				umass_bbb_reset(sc, STATUS_WIRE_FAILED);

			return;
		} else {
			DPRINTF(UDMASS_BBB, ("%s: no data phase\n",
				USBDEVNAME(sc->sc_dev)));
		}

		/* FALLTHROUGH if no data phase, err == 0 */
	case TSTATE_BBB_DATA:
		/* Command transport phase, error handling (ignored if no data
		 * phase (fallthrough from previous state)) */
		if (sc->transfer_dir != DIR_NONE) {
			/* retrieve the length of the transfer that was done */
			usbd_get_xfer_status(xfer, NULL, NULL,
						&sc->transfer_actlen, NULL);

			if (err) {
				DPRINTF(UDMASS_BBB, ("%s: Data-%s %db failed, "
					"%s\n", USBDEVNAME(sc->sc_dev),
					(sc->transfer_dir == DIR_IN?"in":"out"),
					sc->transfer_datalen,usbd_errstr(err)));

				if (err == USBD_STALLED) {
					umass_clear_endpoint_stall(sc,
					  (sc->transfer_dir == DIR_IN?
					    sc->bulkin:sc->bulkout),
					  (sc->transfer_dir == DIR_IN?
					    sc->bulkin_pipe:sc->bulkout_pipe),
					  TSTATE_BBB_DCLEAR,
					  sc->transfer_xfer[XFER_BBB_DCLEAR]);
					return;
				} else {
					/* Unless the error is a pipe stall the
					 * error is fatal.
					 */
					umass_bbb_reset(sc,STATUS_WIRE_FAILED);
					return;
				}
			}
		}

		DIF(UDMASS_BBB, if (sc->transfer_dir == DIR_IN)
					umass_dump_buffer(sc, sc->transfer_data,
						sc->transfer_datalen, 48));



		/* FALLTHROUGH, err == 0 (no data phase or successfull) */
	case TSTATE_BBB_DCLEAR:	/* stall clear after data phase */
	case TSTATE_BBB_SCLEAR:	/* stall clear after status phase */
		/* Reading of CSW after bulk stall condition in data phase
		 * (TSTATE_BBB_DATA2) or bulk-in stall condition after
		 * reading CSW (TSTATE_BBB_SCLEAR).
		 * In the case of no data phase or successfull data phase,
		 * err == 0 and the following if block is passed.
		 */
		if (err) {	/* should not occur */
			/* try the transfer below, even if clear stall failed */
			DPRINTF(UDMASS_BBB, ("%s: bulk-%s stall clear failed"
				", %s\n", USBDEVNAME(sc->sc_dev),
				(sc->transfer_dir == DIR_IN? "in":"out"),
				usbd_errstr(err)));
			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		}

		/* Status transport phase, setup transfer */
		if (sc->transfer_state == TSTATE_BBB_COMMAND ||
		    sc->transfer_state == TSTATE_BBB_DATA ||
		    sc->transfer_state == TSTATE_BBB_DCLEAR) {
		    	/* After no data phase, successfull data phase and
			 * after clearing bulk-in/-out stall condition
			 */
			sc->transfer_state = TSTATE_BBB_STATUS1;
			next_xfer = sc->transfer_xfer[XFER_BBB_CSW1];
		} else {
			/* After first attempt of fetching CSW */
			sc->transfer_state = TSTATE_BBB_STATUS2;
			next_xfer = sc->transfer_xfer[XFER_BBB_CSW2];
		}

		/* Read the Command Status Wrapper via bulk-in endpoint. */
		if (umass_setup_transfer(sc, sc->bulkin_pipe,
				&sc->csw, UMASS_BBB_CSW_SIZE, 0,
				next_xfer)) {
			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		}

		return;
	case TSTATE_BBB_STATUS1:	/* first attempt */
	case TSTATE_BBB_STATUS2:	/* second attempt */
		/* Status transfer, error handling */
		if (err) {
			DPRINTF(UDMASS_BBB, ("%s: Failed to read CSW, %s%s\n",
				USBDEVNAME(sc->sc_dev), usbd_errstr(err),
				(sc->transfer_state == TSTATE_BBB_STATUS1?
					", retrying":"")));

			/* If this was the first attempt at fetching the CSW
			 * retry it, otherwise fail.
			 */
			if (sc->transfer_state == TSTATE_BBB_STATUS1) {
				umass_clear_endpoint_stall(sc,
					    sc->bulkin, sc->bulkin_pipe,
					    TSTATE_BBB_SCLEAR,
					    sc->transfer_xfer[XFER_BBB_SCLEAR]);
				return;
			} else {
				umass_bbb_reset(sc, STATUS_WIRE_FAILED);
				return;
			}
		}

		DIF(UDMASS_BBB, umass_bbb_dump_csw(sc, &sc->csw));

		/* Translate weird command-status signatures. */
		if (sc->quirks & WRONG_CSWSIG) {
			u_int32_t dCSWSignature = UGETDW(sc->csw.dCSWSignature);
			if (dCSWSignature == CSWSIGNATURE_OLYMPUS_C1 ||
			    dCSWSignature == CSWSIGNATURE_IMAGINATION_DBX1)
				USETDW(sc->csw.dCSWSignature, CSWSIGNATURE);
		}

		int Residue;
		Residue = UGETDW(sc->csw.dCSWDataResidue);
		if (Residue == 0 &&
		    sc->transfer_datalen - sc->transfer_actlen != 0)
			Residue = sc->transfer_datalen - sc->transfer_actlen;

		/* Check CSW and handle any error */
		if (UGETDW(sc->csw.dCSWSignature) != CSWSIGNATURE) {
			/* Invalid CSW: Wrong signature or wrong tag might
			 * indicate that the device is confused -> reset it.
			 */
			printf("%s: Invalid CSW: sig 0x%08x should be 0x%08x\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(sc->csw.dCSWSignature),
				CSWSIGNATURE);

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		} else if (UGETDW(sc->csw.dCSWTag)
				!= UGETDW(sc->cbw.dCBWTag)) {
			printf("%s: Invalid CSW: tag %d should be %d\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(sc->csw.dCSWTag),
				UGETDW(sc->cbw.dCBWTag));

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;

		/* CSW is valid here */
		} else if (sc->csw.bCSWStatus > CSWSTATUS_PHASE) {
			printf("%s: Invalid CSW: status %d > %d\n",
				USBDEVNAME(sc->sc_dev),
				sc->csw.bCSWStatus,
				CSWSTATUS_PHASE);

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		} else if (sc->csw.bCSWStatus == CSWSTATUS_PHASE) {
			printf("%s: Phase Error, residue = %d\n",
				USBDEVNAME(sc->sc_dev), Residue);

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;

		} else if (sc->transfer_actlen > sc->transfer_datalen) {
			/* Buffer overrun! Don't let this go by unnoticed */
			panic("%s: transferred %db instead of %db",
				USBDEVNAME(sc->sc_dev),
				sc->transfer_actlen, sc->transfer_datalen);

		} else if (sc->csw.bCSWStatus == CSWSTATUS_FAILED) {
			DPRINTF(UDMASS_BBB, ("%s: Command Failed, res = %d\n",
				USBDEVNAME(sc->sc_dev), Residue));

			/* SCSI command failed but transfer was succesful */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv, Residue,
					STATUS_CMD_FAILED);
			return;

		} else {	/* success */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv, Residue,
					STATUS_CMD_OK);

			return;
		}

	/***** Bulk Reset *****/
	case TSTATE_BBB_RESET1:
		if (err)
			printf("%s: BBB reset failed, %s\n",
				USBDEVNAME(sc->sc_dev), usbd_errstr(err));

		umass_clear_endpoint_stall(sc,
			sc->bulkin, sc->bulkin_pipe, TSTATE_BBB_RESET2,
			sc->transfer_xfer[XFER_BBB_RESET2]);

		return;
	case TSTATE_BBB_RESET2:
		if (err)	/* should not occur */
			printf("%s: BBB bulk-in clear stall failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		umass_clear_endpoint_stall(sc,
			sc->bulkout, sc->bulkout_pipe, TSTATE_BBB_RESET3,
			sc->transfer_xfer[XFER_BBB_RESET3]);

		return;
	case TSTATE_BBB_RESET3:
		if (err)	/* should not occur */
			printf("%s: BBB bulk-out clear stall failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		sc->transfer_state = TSTATE_IDLE;
		if (sc->transfer_priv) {
			sc->transfer_cb(sc, sc->transfer_priv,
					sc->transfer_datalen,
					sc->transfer_status);
		}

		return;

	/***** Default *****/
	default:
		panic("%s: Unknown state %d",
		      USBDEVNAME(sc->sc_dev), sc->transfer_state);
	}
}

Static int
umass_bbb_get_max_lun(struct umass_softc *sc)
{
	usbd_device_handle udev;
	usb_device_request_t req;
	usbd_status err;
	usb_interface_descriptor_t *id;
	int maxlun = 0;
	u_int8_t buf = 0;

	usbd_interface2device_handle(sc->iface, &udev);
	id = usbd_get_interface_descriptor(sc->iface);

	/* The Get Max Lun command is a class-specific request. */
	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_BBB_GET_MAX_LUN;
	USETW(req.wValue, 0);
	USETW(req.wIndex, id->bInterfaceNumber);
	USETW(req.wLength, 1);

	err = usbd_do_request(udev, &req, &buf);
	switch (err) {
	case USBD_NORMAL_COMPLETION:
		maxlun = buf;
		DPRINTF(UDMASS_BBB, ("%s: Max Lun is %d\n",
		    USBDEVNAME(sc->sc_dev), maxlun));
		break;
	case USBD_STALLED:
	case USBD_SHORT_XFER:
	default:
		/* Device doesn't support Get Max Lun request. */
		printf("%s: Get Max Lun not supported (%s)\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		/* XXX Should we port_reset the device? */
		break;
	}

	return(maxlun);
}

/*
 * Command/Bulk/Interrupt (CBI) specific functions
 */

Static int
umass_cbi_adsc(struct umass_softc *sc, char *buffer, int buflen,
	       usbd_xfer_handle xfer)
{
	usbd_device_handle udev;

	KASSERT(sc->proto & (UMASS_PROTO_CBI|UMASS_PROTO_CBI_I),
		("%s: umass_cbi_adsc: wrong sc->proto 0x%02x\n",
			USBDEVNAME(sc->sc_dev), sc->proto));

	usbd_interface2device_handle(sc->iface, &udev);

	sc->request.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	sc->request.bRequest = UR_CBI_ADSC;
	USETW(sc->request.wValue, 0);
	USETW(sc->request.wIndex, sc->ifaceno);
	USETW(sc->request.wLength, buflen);
	return umass_setup_ctrl_transfer(sc, udev, &sc->request, buffer,
					 buflen, 0, xfer);
}


Static void
umass_cbi_reset(struct umass_softc *sc, int status)
{
	int i;
#	define SEND_DIAGNOSTIC_CMDLEN	12

	KASSERT(sc->proto & (UMASS_PROTO_CBI|UMASS_PROTO_CBI_I),
		("%s: umass_cbi_reset: wrong sc->proto 0x%02x\n",
			USBDEVNAME(sc->sc_dev), sc->proto));

	/*
	 * Command Block Reset Protocol
	 *
	 * First send a reset request to the device. Then clear
	 * any possibly stalled bulk endpoints.
	 *
	 * This is done in 3 steps, states:
	 * TSTATE_CBI_RESET1
	 * TSTATE_CBI_RESET2
	 * TSTATE_CBI_RESET3
	 *
	 * If the reset doesn't succeed, the device should be port reset.
	 */

	DPRINTF(UDMASS_CBI, ("%s: CBI Reset\n",
		USBDEVNAME(sc->sc_dev)));

	KASSERT(sizeof(sc->cbl) >= SEND_DIAGNOSTIC_CMDLEN,
		("%s: CBL struct is too small (%ld < %d)\n",
			USBDEVNAME(sc->sc_dev),
			(long)sizeof(sc->cbl), SEND_DIAGNOSTIC_CMDLEN));

	sc->transfer_state = TSTATE_CBI_RESET1;
	sc->transfer_status = status;

	/* The 0x1d code is the SEND DIAGNOSTIC command. To distinguish between
	 * the two the last 10 bytes of the cbl is filled with 0xff (section
	 * 2.2 of the CBI spec).
	 */
	sc->cbl[0] = 0x1d;	/* Command Block Reset */
	sc->cbl[1] = 0x04;
	for (i = 2; i < SEND_DIAGNOSTIC_CMDLEN; i++)
		sc->cbl[i] = 0xff;

	umass_cbi_adsc(sc, sc->cbl, SEND_DIAGNOSTIC_CMDLEN,
		       sc->transfer_xfer[XFER_CBI_RESET1]);
	/* XXX if the command fails we should reset the port on the hub */
}

Static void
umass_cbi_transfer(struct umass_softc *sc, int lun,
		void *cmd, int cmdlen, void *data, int datalen, int dir,
		u_int timeout, transfer_cb_f cb, void *priv)
{
	KASSERT(sc->proto & (UMASS_PROTO_CBI|UMASS_PROTO_CBI_I),
		("%s: umass_cbi_transfer: wrong sc->proto 0x%02x\n",
			USBDEVNAME(sc->sc_dev), sc->proto));

	/* Be a little generous. */
	sc->timeout = timeout + UMASS_TIMEOUT;

	/*
	 * Do a CBI transfer with cmdlen bytes from cmd, possibly
	 * a data phase of datalen bytes from/to the device and finally a
	 * csw read phase.
	 * If the data direction was inbound a maximum of datalen bytes
	 * is stored in the buffer pointed to by data.
	 *
	 * umass_cbi_transfer initialises the transfer and lets the state
	 * machine in umass_cbi_state handle the completion. It uses the
	 * following states:
	 * TSTATE_CBI_COMMAND
	 *   -> XXX fill in
	 *
	 * An error in any of those states will invoke
	 * umass_cbi_reset.
	 */

	/* check the given arguments */
	KASSERT(datalen == 0 || data != NULL,
		("%s: datalen > 0, but no buffer",USBDEVNAME(sc->sc_dev)));
	KASSERT(datalen == 0 || dir != DIR_NONE,
		("%s: direction is NONE while datalen is not zero\n",
			USBDEVNAME(sc->sc_dev)));

	/* store the details for the data transfer phase */
	sc->transfer_dir = dir;
	sc->transfer_data = data;
	sc->transfer_datalen = datalen;
	sc->transfer_actlen = 0;
	sc->transfer_cb = cb;
	sc->transfer_priv = priv;
	sc->transfer_status = STATUS_CMD_OK;

	/* move from idle to the command state */
	sc->transfer_state = TSTATE_CBI_COMMAND;

	DIF(UDMASS_CBI, umass_cbi_dump_cmd(sc, cmd, cmdlen));

	/* Send the Command Block from host to device via control endpoint. */
	if (umass_cbi_adsc(sc, cmd, cmdlen, sc->transfer_xfer[XFER_CBI_CB]))
		umass_cbi_reset(sc, STATUS_WIRE_FAILED);
}

Static void
umass_cbi_state(usbd_xfer_handle xfer, usbd_private_handle priv,
		usbd_status err)
{
	struct umass_softc *sc = (struct umass_softc *) priv;

	KASSERT(sc->proto & (UMASS_PROTO_CBI|UMASS_PROTO_CBI_I),
		("%s: umass_cbi_state: wrong sc->proto 0x%02x\n",
			USBDEVNAME(sc->sc_dev), sc->proto));

	/*
	 * State handling for CBI transfers.
	 */

	DPRINTF(UDMASS_CBI, ("%s: Handling CBI state %d (%s), xfer=%p, %s\n",
		USBDEVNAME(sc->sc_dev), sc->transfer_state,
		states[sc->transfer_state], xfer, usbd_errstr(err)));

	/* Give up if the device has detached. */
	if (sc->flags & UMASS_FLAGS_GONE) {
		sc->transfer_state = TSTATE_IDLE;
		sc->transfer_cb(sc, sc->transfer_priv, sc->transfer_datalen,
		    STATUS_CMD_FAILED);
		return;
	}

	switch (sc->transfer_state) {

	/***** CBI Transfer *****/
	case TSTATE_CBI_COMMAND:
		if (err == USBD_STALLED) {
			DPRINTF(UDMASS_CBI, ("%s: Command Transport failed\n",
				USBDEVNAME(sc->sc_dev)));
			/* Status transport by control pipe (section 2.3.2.1).
			 * The command contained in the command block failed.
			 *
			 * The control pipe has already been unstalled by the
			 * USB stack.
			 * Section 2.4.3.1.1 states that the bulk in endpoints
			 * should not be stalled at this point.
			 */

			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
					sc->transfer_datalen,
					STATUS_CMD_FAILED);

			return;
		} else if (err) {
			DPRINTF(UDMASS_CBI, ("%s: failed to send ADSC\n",
				USBDEVNAME(sc->sc_dev)));
			umass_cbi_reset(sc, STATUS_WIRE_FAILED);

			return;
		}

		sc->transfer_state = TSTATE_CBI_DATA;
		if (sc->transfer_dir == DIR_IN) {
			if (umass_setup_transfer(sc, sc->bulkin_pipe,
					sc->transfer_data, sc->transfer_datalen,
					USBD_SHORT_XFER_OK,
					sc->transfer_xfer[XFER_CBI_DATA]))
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);

		} else if (sc->transfer_dir == DIR_OUT) {
			if (umass_setup_transfer(sc, sc->bulkout_pipe,
					sc->transfer_data, sc->transfer_datalen,
					0,	/* fixed length transfer */
					sc->transfer_xfer[XFER_CBI_DATA]))
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);

		} else if (sc->proto & UMASS_PROTO_CBI_I) {
			DPRINTF(UDMASS_CBI, ("%s: no data phase\n",
				USBDEVNAME(sc->sc_dev)));
			sc->transfer_state = TSTATE_CBI_STATUS;
			if (umass_setup_transfer(sc, sc->intrin_pipe,
					&sc->sbl, sizeof(sc->sbl),
					0,	/* fixed length transfer */
					sc->transfer_xfer[XFER_CBI_STATUS])){
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);
			}
		} else {
			DPRINTF(UDMASS_CBI, ("%s: no data phase\n",
				USBDEVNAME(sc->sc_dev)));
			/* No command completion interrupt. Request
			 * sense data.
			 */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
			       0, STATUS_CMD_UNKNOWN);
		}

		return;

	case TSTATE_CBI_DATA:
		/* retrieve the length of the transfer that was done */
		usbd_get_xfer_status(xfer,NULL,NULL,&sc->transfer_actlen,NULL);

		if (err) {
			DPRINTF(UDMASS_CBI, ("%s: Data-%s %db failed, "
				"%s\n", USBDEVNAME(sc->sc_dev),
				(sc->transfer_dir == DIR_IN?"in":"out"),
				sc->transfer_datalen,usbd_errstr(err)));

			if (err == USBD_STALLED) {
				umass_clear_endpoint_stall(sc,
					sc->bulkin, sc->bulkin_pipe,
					TSTATE_CBI_DCLEAR,
					sc->transfer_xfer[XFER_CBI_DCLEAR]);
			} else {
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);
			}
			return;
		}

		DIF(UDMASS_CBI, if (sc->transfer_dir == DIR_IN)
					umass_dump_buffer(sc, sc->transfer_data,
						sc->transfer_actlen, 48));

		if (sc->proto & UMASS_PROTO_CBI_I) {
			sc->transfer_state = TSTATE_CBI_STATUS;
			if (umass_setup_transfer(sc, sc->intrin_pipe,
				    &sc->sbl, sizeof(sc->sbl),
				    0,	/* fixed length transfer */
				    sc->transfer_xfer[XFER_CBI_STATUS])){
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);
			}
		} else {
			/* No command completion interrupt. Request
			 * sense to get status of command.
			 */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
				sc->transfer_datalen - sc->transfer_actlen,
				STATUS_CMD_UNKNOWN);
		}
		return;

	case TSTATE_CBI_STATUS:
		if (err) {
			DPRINTF(UDMASS_CBI, ("%s: Status Transport failed\n",
				USBDEVNAME(sc->sc_dev)));
			/* Status transport by interrupt pipe (section 2.3.2.2).
			 */

			if (err == USBD_STALLED) {
				umass_clear_endpoint_stall(sc,
					sc->intrin, sc->intrin_pipe,
					TSTATE_CBI_SCLEAR,
					sc->transfer_xfer[XFER_CBI_SCLEAR]);
			} else {
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);
			}
			return;
		}

		/* Dissect the information in the buffer */

		if (sc->proto & UMASS_PROTO_UFI) {
			int status;

			/* Section 3.4.3.1.3 specifies that the UFI command
			 * protocol returns an ASC and ASCQ in the interrupt
			 * data block.
			 */

			DPRINTF(UDMASS_CBI, ("%s: UFI CCI, ASC = 0x%02x, "
				"ASCQ = 0x%02x\n",
				USBDEVNAME(sc->sc_dev),
				sc->sbl.ufi.asc, sc->sbl.ufi.ascq));

			if (sc->sbl.ufi.asc == 0 && sc->sbl.ufi.ascq == 0)
				status = STATUS_CMD_OK;
			else
				status = STATUS_CMD_FAILED;

			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
				sc->transfer_datalen - sc->transfer_actlen,
				status);
		} else {
			/* Command Interrupt Data Block */
			DPRINTF(UDMASS_CBI, ("%s: type=0x%02x, value=0x%02x\n",
				USBDEVNAME(sc->sc_dev),
				sc->sbl.common.type, sc->sbl.common.value));

			if (sc->sbl.common.type == IDB_TYPE_CCI) {
				int err;

				if ((sc->sbl.common.value&IDB_VALUE_STATUS_MASK)
							== IDB_VALUE_PASS) {
					err = STATUS_CMD_OK;
				} else if ((sc->sbl.common.value & IDB_VALUE_STATUS_MASK)
							== IDB_VALUE_FAIL ||
					   (sc->sbl.common.value & IDB_VALUE_STATUS_MASK)
						== IDB_VALUE_PERSISTENT) {
					err = STATUS_CMD_FAILED;
				} else {
					err = STATUS_WIRE_FAILED;
				}

				sc->transfer_state = TSTATE_IDLE;
				sc->transfer_cb(sc, sc->transfer_priv,
				       sc->transfer_datalen-sc->transfer_actlen,
				       err);
			}
		}
		return;

	case TSTATE_CBI_DCLEAR:
		if (err) {	/* should not occur */
			printf("%s: CBI bulk-in/out stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			umass_cbi_reset(sc, STATUS_WIRE_FAILED);
		}

		sc->transfer_state = TSTATE_IDLE;
		sc->transfer_cb(sc, sc->transfer_priv,
				sc->transfer_datalen,
				STATUS_CMD_FAILED);
		return;

	case TSTATE_CBI_SCLEAR:
		if (err)	/* should not occur */
			printf("%s: CBI intr-in stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));

		/* Something really bad is going on. Reset the device */
		umass_cbi_reset(sc, STATUS_CMD_FAILED);
		return;

	/***** CBI Reset *****/
	case TSTATE_CBI_RESET1:
		if (err)
			printf("%s: CBI reset failed, %s\n",
				USBDEVNAME(sc->sc_dev), usbd_errstr(err));

		umass_clear_endpoint_stall(sc,
			sc->bulkin, sc->bulkin_pipe, TSTATE_CBI_RESET2,
			sc->transfer_xfer[XFER_CBI_RESET2]);

		return;
	case TSTATE_CBI_RESET2:
		if (err)	/* should not occur */
			printf("%s: CBI bulk-in stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		umass_clear_endpoint_stall(sc,
			sc->bulkout, sc->bulkout_pipe, TSTATE_CBI_RESET3,
			sc->transfer_xfer[XFER_CBI_RESET3]);

		return;
	case TSTATE_CBI_RESET3:
		if (err)	/* should not occur */
			printf("%s: CBI bulk-out stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		sc->transfer_state = TSTATE_IDLE;
		if (sc->transfer_priv) {
			sc->transfer_cb(sc, sc->transfer_priv,
					sc->transfer_datalen,
					sc->transfer_status);
		}

		return;


	/***** Default *****/
	default:
		panic("%s: Unknown state %d",
		      USBDEVNAME(sc->sc_dev), sc->transfer_state);
	}
}




/*
 * CAM specific functions (used by SCSI, UFI, 8070i (ATAPI))
 */

Static int
umass_cam_attach_sim(struct umass_softc *sc)
{
	struct cam_devq	*devq;		/* Per device Queue */

	/* A HBA is attached to the CAM layer.
	 *
	 * The CAM layer will then after a while start probing for
	 * devices on the bus. The number of SIMs is limited to one.
	 */

	devq = cam_simq_alloc(1 /*maximum openings*/);
	if (devq == NULL)
		return(ENOMEM);

	sc->umass_sim = cam_sim_alloc(umass_cam_action, umass_cam_poll,
				DEVNAME_SIM,
				sc /*priv*/,
				USBDEVUNIT(sc->sc_dev) /*unit number*/,
				1 /*maximum device openings*/,
				0 /*maximum tagged device openings*/,
				devq);
	if (sc->umass_sim == NULL) {
		cam_simq_free(devq);
		return(ENOMEM);
	}

	if(xpt_bus_register(sc->umass_sim, USBDEVUNIT(sc->sc_dev)) !=
	    CAM_SUCCESS)
		return(ENOMEM);

	return(0);
}

Static void
umass_cam_rescan_callback(struct cam_periph *periph, union ccb *ccb)
{
#ifdef USB_DEBUG
	if (ccb->ccb_h.status != CAM_REQ_CMP) {
		DPRINTF(UDMASS_SCSI, ("%s:%d Rescan failed, 0x%04x\n",
			periph->periph_name, periph->unit_number,
			ccb->ccb_h.status));
	} else {
		DPRINTF(UDMASS_SCSI, ("%s%d: Rescan succeeded\n",
			periph->periph_name, periph->unit_number));
	}
#endif

	xpt_free_path(ccb->ccb_h.path);
	free(ccb, M_USBDEV);
}

Static void
umass_cam_rescan(void *addr)
{
	struct umass_softc *sc = (struct umass_softc *) addr;
	struct cam_path *path;
	union ccb *ccb;

	DPRINTF(UDMASS_SCSI, ("scbus%d: scanning for %s:%d:%d:%d\n",
		cam_sim_path(sc->umass_sim),
		USBDEVNAME(sc->sc_dev), cam_sim_path(sc->umass_sim),
		USBDEVUNIT(sc->sc_dev), CAM_LUN_WILDCARD));

	ccb = malloc(sizeof(union ccb), M_USBDEV, M_NOWAIT | M_ZERO);
	if (ccb == NULL)
		return;
	if (xpt_create_path(&path, xpt_periph, cam_sim_path(sc->umass_sim),
			    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD)
	    != CAM_REQ_CMP) {
		free(ccb, M_USBDEV);
		return;
	}

	xpt_setup_ccb(&ccb->ccb_h, path, 5/*priority (low)*/);
	ccb->ccb_h.func_code = XPT_SCAN_BUS;
	ccb->ccb_h.cbfcnp = umass_cam_rescan_callback;
	ccb->crcn.flags = CAM_FLAG_NONE;
	xpt_action(ccb);

	/* The scan is in progress now. */
}

Static int
umass_cam_attach(struct umass_softc *sc)
{
#ifndef USB_DEBUG
	if (bootverbose)
#endif
		printf("%s:%d:%d:%d: Attached to scbus%d\n",
			USBDEVNAME(sc->sc_dev), cam_sim_path(sc->umass_sim),
			USBDEVUNIT(sc->sc_dev), CAM_LUN_WILDCARD,
			cam_sim_path(sc->umass_sim));

	if (!cold) {
		/* Notify CAM of the new device after a short delay. Any
		 * failure is benign, as the user can still do it by hand
		 * (camcontrol rescan <busno>). Only do this if we are not
		 * booting, because CAM does a scan after booting has
		 * completed, when interrupts have been enabled.
		 */

		usb_callout(sc->cam_scsi_rescan_ch, MS_TO_TICKS(200),
		    umass_cam_rescan, sc);
	}

	return(0);	/* always succesfull */
}

/* umass_cam_detach
 *	detach from the CAM layer
 */

Static int
umass_cam_detach_sim(struct umass_softc *sc)
{
	if (sc->umass_sim) {
		if (xpt_bus_deregister(cam_sim_path(sc->umass_sim)))
			cam_sim_free(sc->umass_sim, /*free_devq*/TRUE);
		else
			return(EBUSY);

		sc->umass_sim = NULL;
	}

	return(0);
}

/* umass_cam_action
 * 	CAM requests for action come through here
 */

Static void
umass_cam_action(struct cam_sim *sim, union ccb *ccb)
{
	struct umass_softc *sc = (struct umass_softc *)sim->softc;

	/* The softc is still there, but marked as going away. umass_cam_detach
	 * has not yet notified CAM of the lost device however.
	 */
	if (sc && (sc->flags & UMASS_FLAGS_GONE)) {
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:func_code 0x%04x: "
			"Invalid target (gone)\n",
			USBDEVNAME(sc->sc_dev), cam_sim_path(sc->umass_sim),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
			ccb->ccb_h.func_code));
		ccb->ccb_h.status = CAM_TID_INVALID;
		xpt_done(ccb);
		return;
	}

	/* Verify, depending on the operation to perform, that we either got a
	 * valid sc, because an existing target was referenced, or otherwise
	 * the SIM is addressed.
	 *
	 * This avoids bombing out at a printf and does give the CAM layer some
	 * sensible feedback on errors.
	 */
	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	case XPT_RESET_DEV:
	case XPT_GET_TRAN_SETTINGS:
	case XPT_SET_TRAN_SETTINGS:
	case XPT_CALC_GEOMETRY:
		/* the opcodes requiring a target. These should never occur. */
		if (sc == NULL) {
			printf("%s:%d:%d:%d:func_code 0x%04x: "
				"Invalid target (target needed)\n",
				DEVNAME_SIM, cam_sim_path(sc->umass_sim),
				ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
				ccb->ccb_h.func_code);

			ccb->ccb_h.status = CAM_TID_INVALID;
			xpt_done(ccb);
			return;
		}
		break;
	case XPT_PATH_INQ:
	case XPT_NOOP:
		/* The opcodes sometimes aimed at a target (sc is valid),
		 * sometimes aimed at the SIM (sc is invalid and target is
		 * CAM_TARGET_WILDCARD)
		 */
		if (sc == NULL && ccb->ccb_h.target_id != CAM_TARGET_WILDCARD) {
			DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:func_code 0x%04x: "
				"Invalid target (no wildcard)\n",
				DEVNAME_SIM, cam_sim_path(sc->umass_sim),
				ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
				ccb->ccb_h.func_code));

			ccb->ccb_h.status = CAM_TID_INVALID;
			xpt_done(ccb);
			return;
		}
		break;
	default:
		/* XXX Hm, we should check the input parameters */
		break;
	}

	/* Perform the requested action */
	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	{
		struct ccb_scsiio *csio = &ccb->csio;	/* deref union */
		int dir;
		unsigned char *cmd;
		int cmdlen;
		unsigned char *rcmd;
		int rcmdlen;

		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_SCSI_IO: "
			"cmd: 0x%02x, flags: 0x%02x, "
			"%db cmd/%db data/%db sense\n",
			USBDEVNAME(sc->sc_dev), cam_sim_path(sc->umass_sim),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
			csio->cdb_io.cdb_bytes[0],
			ccb->ccb_h.flags & CAM_DIR_MASK,
			csio->cdb_len, csio->dxfer_len,
			csio->sense_len));

		/* clear the end of the buffer to make sure we don't send out
		 * garbage.
		 */
		DIF(UDMASS_SCSI, if ((ccb->ccb_h.flags & CAM_DIR_MASK)
				     == CAM_DIR_OUT)
					umass_dump_buffer(sc, csio->data_ptr,
						csio->dxfer_len, 48));

		if (sc->transfer_state != TSTATE_IDLE) {
			DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_SCSI_IO: "
				"I/O in progress, deferring (state %d, %s)\n",
				USBDEVNAME(sc->sc_dev), cam_sim_path(sc->umass_sim),
				ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
				sc->transfer_state,states[sc->transfer_state]));
			ccb->ccb_h.status = CAM_SCSI_BUSY;
			xpt_done(ccb);
			return;
		}

		switch(ccb->ccb_h.flags&CAM_DIR_MASK) {
		case CAM_DIR_IN:
			dir = DIR_IN;
			break;
		case CAM_DIR_OUT:
			dir = DIR_OUT;
			break;
		default:
			dir = DIR_NONE;
		}

		ccb->ccb_h.status = CAM_REQ_INPROG | CAM_SIM_QUEUED;


		if (csio->ccb_h.flags & CAM_CDB_POINTER) {
			cmd = (unsigned char *) csio->cdb_io.cdb_ptr;
		} else {
			cmd = (unsigned char *) &csio->cdb_io.cdb_bytes;
		}
		cmdlen = csio->cdb_len;
		rcmd = (unsigned char *) &sc->cam_scsi_command;
		rcmdlen = sizeof(sc->cam_scsi_command);

		/* sc->transform will convert the command to the command
		 * (format) needed by the specific command set and return
		 * the converted command in a buffer pointed to be rcmd.
		 * We pass in a buffer, but if the command does not
		 * have to be transformed it returns a ptr to the original
		 * buffer (see umass_scsi_transform).
		 */

		if (sc->transform(sc, cmd, cmdlen, &rcmd, &rcmdlen)) {
			/*
			 * Handle EVPD inquiry for broken devices first
			 * NO_INQUIRY also implies NO_INQUIRY_EVPD
			 */
			if ((sc->quirks & (NO_INQUIRY_EVPD | NO_INQUIRY)) &&
			    rcmd[0] == INQUIRY && (rcmd[1] & SI_EVPD)) {
				struct scsi_sense_data *sense;

				sense = &ccb->csio.sense_data;
				bzero(sense, sizeof(*sense));
				sense->error_code = SSD_CURRENT_ERROR;
				sense->flags = SSD_KEY_ILLEGAL_REQUEST;
				sense->add_sense_code = 0x24;
				sense->extra_len = 10;
 				ccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
				ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR |
				    CAM_AUTOSNS_VALID;
				xpt_done(ccb);
				return;
			}
			/* Return fake inquiry data for broken devices */
			if ((sc->quirks & NO_INQUIRY) && rcmd[0] == INQUIRY) {
				struct ccb_scsiio *csio = &ccb->csio;

				memcpy(csio->data_ptr, &fake_inq_data,
				    sizeof(fake_inq_data));
				csio->scsi_status = SCSI_STATUS_OK;
				ccb->ccb_h.status = CAM_REQ_CMP;
				xpt_done(ccb);
				return;
			}
			if ((sc->quirks & FORCE_SHORT_INQUIRY) &&
			    rcmd[0] == INQUIRY) {
				csio->dxfer_len = SHORT_INQUIRY_LENGTH;
			}
			sc->transfer(sc, ccb->ccb_h.target_lun, rcmd, rcmdlen,
				     csio->data_ptr,
				     csio->dxfer_len, dir, ccb->ccb_h.timeout,
				     umass_cam_cb, (void *) ccb);
		} else {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
		}

		break;
	}
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_PATH_INQ:.\n",
			(sc == NULL? DEVNAME_SIM:USBDEVNAME(sc->sc_dev)),
			cam_sim_path(sc->umass_sim),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun));

		/* host specific information */
		cpi->version_num = 1;
		cpi->hba_inquiry = 0;
		cpi->target_sprt = 0;
		cpi->hba_misc = PIM_NO_6_BYTE;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = UMASS_SCSIID_MAX;	/* one target */
		cpi->initiator_id = UMASS_SCSIID_HOST;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "USB SCSI", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->bus_id = USBDEVUNIT(sc->sc_dev);

		if (sc == NULL) {
			cpi->base_transfer_speed = 0;
			cpi->max_lun = 0;
		} else {
			if (sc->quirks & FLOPPY_SPEED) {
				cpi->base_transfer_speed =
				    UMASS_FLOPPY_TRANSFER_SPEED;
			} else if (usbd_get_speed(sc->sc_udev) ==
			    USB_SPEED_HIGH) {
				cpi->base_transfer_speed =
				    UMASS_HIGH_TRANSFER_SPEED;
			} else {
				cpi->base_transfer_speed =
				    UMASS_FULL_TRANSFER_SPEED;
			}
			cpi->max_lun = sc->maxlun;
		}

		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_DEV:
	{
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_RESET_DEV:.\n",
			USBDEVNAME(sc->sc_dev), cam_sim_path(sc->umass_sim),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun));

		ccb->ccb_h.status = CAM_REQ_INPROG;
		umass_reset(sc, umass_cam_cb, (void *) ccb);
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings *cts = &ccb->cts;

		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_GET_TRAN_SETTINGS:.\n",
			USBDEVNAME(sc->sc_dev), cam_sim_path(sc->umass_sim),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun));

		cts->valid = 0;
		cts->flags = 0;		/* no disconnection, tagging */

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_SET_TRAN_SETTINGS:
	{
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_SET_TRAN_SETTINGS:.\n",
			USBDEVNAME(sc->sc_dev), cam_sim_path(sc->umass_sim),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun));

		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		cam_calc_geometry(&ccb->ccg, /*extended*/1);
		xpt_done(ccb);
		break;
	}
	case XPT_NOOP:
	{
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_NOOP:.\n",
			(sc == NULL? DEVNAME_SIM:USBDEVNAME(sc->sc_dev)),
			cam_sim_path(sc->umass_sim),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun));

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:func_code 0x%04x: "
			"Not implemented\n",
			(sc == NULL? DEVNAME_SIM:USBDEVNAME(sc->sc_dev)),
			cam_sim_path(sc->umass_sim),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
			ccb->ccb_h.func_code));

		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		xpt_done(ccb);
		break;
	}
}

/* umass_cam_poll
 *	all requests are handled through umass_cam_action, requests
 *	are never pending. So, nothing to do here.
 */
Static void
umass_cam_poll(struct cam_sim *sim)
{
#ifdef USB_DEBUG
	struct umass_softc *sc = (struct umass_softc *) sim->softc;

	DPRINTF(UDMASS_SCSI, ("%s: CAM poll\n",
		USBDEVNAME(sc->sc_dev)));
#endif

	/* nop */
}


/* umass_cam_cb
 *	finalise a completed CAM command
 */

Static void
umass_cam_cb(struct umass_softc *sc, void *priv, int residue, int status)
{
	union ccb *ccb = (union ccb *) priv;
	struct ccb_scsiio *csio = &ccb->csio;		/* deref union */

	/* If the device is gone, just fail the request. */
	if (sc->flags & UMASS_FLAGS_GONE) {
		ccb->ccb_h.status = CAM_TID_INVALID;
		xpt_done(ccb);
		return;
	}

	csio->resid = residue;

	switch (status) {
	case STATUS_CMD_OK:
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case STATUS_CMD_UNKNOWN:
	case STATUS_CMD_FAILED:
		switch (ccb->ccb_h.func_code) {
		case XPT_SCSI_IO:
		{
			unsigned char *rcmd;
			int rcmdlen;

			/* fetch sense data */
			/* the rest of the command was filled in at attach */
			sc->cam_scsi_sense.length = csio->sense_len;

			DPRINTF(UDMASS_SCSI,("%s: Fetching %db sense data\n",
				USBDEVNAME(sc->sc_dev), csio->sense_len));

			rcmd = (unsigned char *) &sc->cam_scsi_command;
			rcmdlen = sizeof(sc->cam_scsi_command);

			if (sc->transform(sc,
				    (unsigned char *) &sc->cam_scsi_sense,
				    sizeof(sc->cam_scsi_sense),
				    &rcmd, &rcmdlen)) {
				if ((sc->quirks & FORCE_SHORT_INQUIRY) && (rcmd[0] == INQUIRY)) {
					csio->sense_len = SHORT_INQUIRY_LENGTH;
				}
				sc->transfer(sc, ccb->ccb_h.target_lun,
					     rcmd, rcmdlen,
					     &csio->sense_data,
					     csio->sense_len, DIR_IN, ccb->ccb_h.timeout,
					     umass_cam_sense_cb, (void *) ccb);
			} else {
				panic("transform(REQUEST_SENSE) failed");
			}
			break;
		}
		case XPT_RESET_DEV: /* Reset failed */
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			xpt_done(ccb);
			break;
		default:
			panic("umass_cam_cb called for func_code %d",
			      ccb->ccb_h.func_code);
		}
		break;

	case STATUS_WIRE_FAILED:
		/* the wire protocol failed and will have recovered
		 * (hopefully).  We return an error to CAM and let CAM retry
		 * the command if necessary.
		 */
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		xpt_done(ccb);
		break;
	default:
		panic("%s: Unknown status %d in umass_cam_cb",
			USBDEVNAME(sc->sc_dev), status);
	}
}

/* Finalise a completed autosense operation
 */
Static void
umass_cam_sense_cb(struct umass_softc *sc, void *priv, int residue, int status)
{
	union ccb *ccb = (union ccb *) priv;
	struct ccb_scsiio *csio = &ccb->csio;		/* deref union */
	unsigned char *rcmd;
	int rcmdlen;

	if (sc->flags & UMASS_FLAGS_GONE) {
		ccb->ccb_h.status = CAM_AUTOSENSE_FAIL;
		xpt_done(ccb);
		return;
	}

	switch (status) {
	case STATUS_CMD_OK:
	case STATUS_CMD_UNKNOWN:
	case STATUS_CMD_FAILED:
		/* Getting sense data always succeeds (apart from wire
		 * failures).
		 */
		if ((sc->quirks & RS_NO_CLEAR_UA)
		    && csio->cdb_io.cdb_bytes[0] == INQUIRY
		    && (csio->sense_data.flags & SSD_KEY)
		    				== SSD_KEY_UNIT_ATTENTION) {
			/* Ignore unit attention errors in the case where
			 * the Unit Attention state is not cleared on
			 * REQUEST SENSE. They will appear again at the next
			 * command.
			 */
			ccb->ccb_h.status = CAM_REQ_CMP;
		} else if ((csio->sense_data.flags & SSD_KEY)
						== SSD_KEY_NO_SENSE) {
			/* No problem after all (in the case of CBI without
			 * CCI)
			 */
			ccb->ccb_h.status = CAM_REQ_CMP;
		} else if ((sc->quirks & RS_NO_CLEAR_UA) &&
			   (csio->cdb_io.cdb_bytes[0] == READ_CAPACITY) &&
			   ((csio->sense_data.flags & SSD_KEY)
			    == SSD_KEY_UNIT_ATTENTION)) {
			/*
			 * Some devices do not clear the unit attention error
			 * on request sense. We insert a test unit ready
			 * command to make sure we clear the unit attention
			 * condition, then allow the retry to proceed as
			 * usual.
			 */

			ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR
					    | CAM_AUTOSNS_VALID;
			csio->scsi_status = SCSI_STATUS_CHECK_COND;

#if 0
			DELAY(300000);
#endif

			DPRINTF(UDMASS_SCSI,("%s: Doing a sneaky"
					     "TEST_UNIT_READY\n",
				USBDEVNAME(sc->sc_dev)));

			/* the rest of the command was filled in at attach */

			rcmd = (unsigned char *) &sc->cam_scsi_command2;
			rcmdlen = sizeof(sc->cam_scsi_command2);

			if (sc->transform(sc,
					(unsigned char *)
					&sc->cam_scsi_test_unit_ready,
					sizeof(sc->cam_scsi_test_unit_ready),
					&rcmd, &rcmdlen)) {
				sc->transfer(sc, ccb->ccb_h.target_lun,
					     rcmd, rcmdlen,
					     NULL, 0, DIR_NONE, ccb->ccb_h.timeout,
					     umass_cam_quirk_cb, (void *) ccb);
			} else {
				panic("transform(TEST_UNIT_READY) failed");
			}
			break;
		} else {
			ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR
					    | CAM_AUTOSNS_VALID;
			csio->scsi_status = SCSI_STATUS_CHECK_COND;
		}
		xpt_done(ccb);
		break;

	default:
		DPRINTF(UDMASS_SCSI, ("%s: Autosense failed, status %d\n",
			USBDEVNAME(sc->sc_dev), status));
		ccb->ccb_h.status = CAM_AUTOSENSE_FAIL;
		xpt_done(ccb);
	}
}

/*
 * This completion code just handles the fact that we sent a test-unit-ready
 * after having previously failed a READ CAPACITY with CHECK_COND.  Even
 * though this command succeeded, we have to tell CAM to retry.
 */
Static void
umass_cam_quirk_cb(struct umass_softc *sc, void *priv, int residue, int status)
{
	union ccb *ccb = (union ccb *) priv;

	DPRINTF(UDMASS_SCSI, ("%s: Test unit ready returned status %d\n",
	USBDEVNAME(sc->sc_dev), status));

	if (sc->flags & UMASS_FLAGS_GONE) {
		ccb->ccb_h.status = CAM_TID_INVALID;
		xpt_done(ccb);
		return;
	}
#if 0
	ccb->ccb_h.status = CAM_REQ_CMP;
#endif
	ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR
			    | CAM_AUTOSNS_VALID;
	ccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
	xpt_done(ccb);
}

Static int
umass_driver_load(module_t mod, int what, void *arg)
{
	switch (what) {
	case MOD_UNLOAD:
	case MOD_LOAD:
	default:
		return(usbd_driver_load(mod, what, arg));
	}
}

/*
 * SCSI specific functions
 */

Static int
umass_scsi_transform(struct umass_softc *sc, unsigned char *cmd, int cmdlen,
		     unsigned char **rcmd, int *rcmdlen)
{
	switch (cmd[0]) {
	case TEST_UNIT_READY:
		if (sc->quirks & NO_TEST_UNIT_READY) {
			KASSERT(*rcmdlen >= sizeof(struct scsi_start_stop_unit),
				("rcmdlen = %d < %ld, buffer too small",
				 *rcmdlen,
				 (long)sizeof(struct scsi_start_stop_unit)));
			DPRINTF(UDMASS_SCSI, ("%s: Converted TEST_UNIT_READY "
				"to START_UNIT\n", USBDEVNAME(sc->sc_dev)));
			memset(*rcmd, 0, *rcmdlen);
			(*rcmd)[0] = START_STOP_UNIT;
			(*rcmd)[4] = SSS_START;
			return 1;
		}
		/* fallthrough */
	case INQUIRY:
		/* some drives wedge when asked for full inquiry information. */
		if (sc->quirks & FORCE_SHORT_INQUIRY) {
			memcpy(*rcmd, cmd, cmdlen);
			*rcmdlen = cmdlen;
			(*rcmd)[4] = SHORT_INQUIRY_LENGTH;
			return 1;
		}
		/* fallthrough */
	default:
		*rcmd = cmd;		/* We don't need to copy it */
		*rcmdlen = cmdlen;
	}

	return 1;
}
/* RBC specific functions */
Static int
umass_rbc_transform(struct umass_softc *sc, unsigned char *cmd, int cmdlen,
		     unsigned char **rcmd, int *rcmdlen)
{
	switch (cmd[0]) {
	/* these commands are defined in RBC: */
	case READ_10:
	case READ_CAPACITY:
	case START_STOP_UNIT:
	case SYNCHRONIZE_CACHE:
	case WRITE_10:
	case 0x2f: /* VERIFY_10 is absent from scsi_all.h??? */
	case INQUIRY:
	case MODE_SELECT_10:
	case MODE_SENSE_10:
	case TEST_UNIT_READY:
	case WRITE_BUFFER:
	 /* The following commands are not listed in my copy of the RBC specs.
	  * CAM however seems to want those, and at least the Sony DSC device
	  * appears to support those as well */
	case REQUEST_SENSE:
	case PREVENT_ALLOW:
		if ((sc->quirks & RBC_PAD_TO_12) && cmdlen < 12) {
			*rcmdlen = 12;
			bcopy(cmd, *rcmd, cmdlen);
			bzero(*rcmd + cmdlen, 12 - cmdlen);
		} else {
			*rcmd = cmd;		/* We don't need to copy it */
			*rcmdlen = cmdlen;
		}
		return 1;
	/* All other commands are not legal in RBC */
	default:
		printf("%s: Unsupported RBC command 0x%02x",
			USBDEVNAME(sc->sc_dev), cmd[0]);
		printf("\n");
		return 0;	/* failure */
	}
}

/*
 * UFI specific functions
 */
Static int
umass_ufi_transform(struct umass_softc *sc, unsigned char *cmd, int cmdlen,
		    unsigned char **rcmd, int *rcmdlen)
{
	/* A UFI command is always 12 bytes in length */
	KASSERT(*rcmdlen >= UFI_COMMAND_LENGTH,
		("rcmdlen = %d < %d, buffer too small",
		 *rcmdlen, UFI_COMMAND_LENGTH));

	*rcmdlen = UFI_COMMAND_LENGTH;
	memset(*rcmd, 0, UFI_COMMAND_LENGTH);

	switch (cmd[0]) {
	/* Commands of which the format has been verified. They should work.
	 * Copy the command into the (zeroed out) destination buffer.
	 */
	case TEST_UNIT_READY:
		if (sc->quirks &  NO_TEST_UNIT_READY) {
			/* Some devices do not support this command.
			 * Start Stop Unit should give the same results
			 */
			DPRINTF(UDMASS_UFI, ("%s: Converted TEST_UNIT_READY "
				"to START_UNIT\n", USBDEVNAME(sc->sc_dev)));
			(*rcmd)[0] = START_STOP_UNIT;
			(*rcmd)[4] = SSS_START;
		} else {
			memcpy(*rcmd, cmd, cmdlen);
		}
		return 1;

	case REZERO_UNIT:
	case REQUEST_SENSE:
	case FORMAT_UNIT:
	case INQUIRY:
	case START_STOP_UNIT:
	case SEND_DIAGNOSTIC:
	case PREVENT_ALLOW:
	case READ_CAPACITY:
	case READ_10:
	case WRITE_10:
	case POSITION_TO_ELEMENT:	/* SEEK_10 */
	case WRITE_AND_VERIFY:
	case VERIFY:
	case MODE_SELECT_10:
	case MODE_SENSE_10:
	case READ_12:
	case WRITE_12:
	case READ_FORMAT_CAPACITIES:
		memcpy(*rcmd, cmd, cmdlen);
		return 1;

	default:
		printf("%s: Unsupported UFI command 0x%02x\n",
			USBDEVNAME(sc->sc_dev), cmd[0]);
		return 0;	/* failure */
	}
}

/*
 * 8070i (ATAPI) specific functions
 */
Static int
umass_atapi_transform(struct umass_softc *sc, unsigned char *cmd, int cmdlen,
		      unsigned char **rcmd, int *rcmdlen)
{
	/* An ATAPI command is always 12 bytes in length. */
	KASSERT(*rcmdlen >= ATAPI_COMMAND_LENGTH,
		("rcmdlen = %d < %d, buffer too small",
		 *rcmdlen, ATAPI_COMMAND_LENGTH));

	*rcmdlen = ATAPI_COMMAND_LENGTH;
	memset(*rcmd, 0, ATAPI_COMMAND_LENGTH);

	switch (cmd[0]) {
	/* Commands of which the format has been verified. They should work.
	 * Copy the command into the (zeroed out) destination buffer.
	 */
	case INQUIRY:
		memcpy(*rcmd, cmd, cmdlen);
		/* some drives wedge when asked for full inquiry information. */
		if (sc->quirks & FORCE_SHORT_INQUIRY)
			(*rcmd)[4] = SHORT_INQUIRY_LENGTH;
		return 1;

	case TEST_UNIT_READY:
		if (sc->quirks & NO_TEST_UNIT_READY) {
			KASSERT(*rcmdlen >= sizeof(struct scsi_start_stop_unit),
				("rcmdlen = %d < %ld, buffer too small",
				 *rcmdlen,
				 (long)sizeof(struct scsi_start_stop_unit)));
			DPRINTF(UDMASS_SCSI, ("%s: Converted TEST_UNIT_READY "
				"to START_UNIT\n", USBDEVNAME(sc->sc_dev)));
			memset(*rcmd, 0, *rcmdlen);
			(*rcmd)[0] = START_STOP_UNIT;
			(*rcmd)[4] = SSS_START;
			return 1;
		}
		/* fallthrough */
	case REZERO_UNIT:
	case REQUEST_SENSE:
	case START_STOP_UNIT:
	case SEND_DIAGNOSTIC:
	case PREVENT_ALLOW:
	case READ_CAPACITY:
	case READ_10:
	case WRITE_10:
	case POSITION_TO_ELEMENT:	/* SEEK_10 */
	case SYNCHRONIZE_CACHE:
	case MODE_SELECT_10:
	case MODE_SENSE_10:
	case READ_BUFFER:
	case 0x42: /* READ_SUBCHANNEL */
	case 0x43: /* READ_TOC */
	case 0x44: /* READ_HEADER */
	case 0x47: /* PLAY_MSF (Play Minute/Second/Frame) */
	case 0x48: /* PLAY_TRACK */
	case 0x49: /* PLAY_TRACK_REL */
	case 0x4b: /* PAUSE */
	case 0x51: /* READ_DISK_INFO */
	case 0x52: /* READ_TRACK_INFO */
	case 0x54: /* SEND_OPC */
	case 0x59: /* READ_MASTER_CUE */
	case 0x5b: /* CLOSE_TR_SESSION */
	case 0x5c: /* READ_BUFFER_CAP */
	case 0x5d: /* SEND_CUE_SHEET */
	case 0xa1: /* BLANK */
	case 0xa5: /* PLAY_12 */
	case 0xa6: /* EXCHANGE_MEDIUM */
	case 0xad: /* READ_DVD_STRUCTURE */
	case 0xbb: /* SET_CD_SPEED */
	case 0xe5: /* READ_TRACK_INFO_PHILIPS */
		memcpy(*rcmd, cmd, cmdlen);
		return 1;

	case READ_12:
	case WRITE_12:
	default:
		printf("%s: Unsupported ATAPI command 0x%02x\n",
			USBDEVNAME(sc->sc_dev), cmd[0]);
		return 0;	/* failure */
	}
}


/* (even the comment is missing) */

DRIVER_MODULE(umass, uhub, umass_driver, umass_devclass, umass_driver_load, 0);



#ifdef USB_DEBUG
Static void
umass_bbb_dump_cbw(struct umass_softc *sc, umass_bbb_cbw_t *cbw)
{
	int clen = cbw->bCDBLength;
	int dlen = UGETDW(cbw->dCBWDataTransferLength);
	u_int8_t *c = cbw->CBWCDB;
	int tag = UGETDW(cbw->dCBWTag);
	int flags = cbw->bCBWFlags;

	DPRINTF(UDMASS_BBB, ("%s: CBW %d: cmd = %db "
		"(0x%02x%02x%02x%02x%02x%02x%s), "
		"data = %db, dir = %s\n",
		USBDEVNAME(sc->sc_dev), tag, clen,
		c[0], c[1], c[2], c[3], c[4], c[5], (clen > 6? "...":""),
		dlen, (flags == CBWFLAGS_IN? "in":
		       (flags == CBWFLAGS_OUT? "out":"<invalid>"))));
}

Static void
umass_bbb_dump_csw(struct umass_softc *sc, umass_bbb_csw_t *csw)
{
	int sig = UGETDW(csw->dCSWSignature);
	int tag = UGETW(csw->dCSWTag);
	int res = UGETDW(csw->dCSWDataResidue);
	int status = csw->bCSWStatus;

	DPRINTF(UDMASS_BBB, ("%s: CSW %d: sig = 0x%08x (%s), tag = %d, "
		"res = %d, status = 0x%02x (%s)\n", USBDEVNAME(sc->sc_dev),
		tag, sig, (sig == CSWSIGNATURE?  "valid":"invalid"),
		tag, res,
		status, (status == CSWSTATUS_GOOD? "good":
			 (status == CSWSTATUS_FAILED? "failed":
			  (status == CSWSTATUS_PHASE? "phase":"<invalid>")))));
}

Static void
umass_cbi_dump_cmd(struct umass_softc *sc, void *cmd, int cmdlen)
{
	u_int8_t *c = cmd;
	int dir = sc->transfer_dir;

	DPRINTF(UDMASS_BBB, ("%s: cmd = %db "
		"(0x%02x%02x%02x%02x%02x%02x%s), "
		"data = %db, dir = %s\n",
		USBDEVNAME(sc->sc_dev), cmdlen,
		c[0], c[1], c[2], c[3], c[4], c[5], (cmdlen > 6? "...":""),
		sc->transfer_datalen,
		(dir == DIR_IN? "in":
		 (dir == DIR_OUT? "out":
		  (dir == DIR_NONE? "no data phase": "<invalid>")))));
}

Static void
umass_dump_buffer(struct umass_softc *sc, u_int8_t *buffer, int buflen,
		  int printlen)
{
	int i, j;
	char s1[40];
	char s2[40];
	char s3[5];

	s1[0] = '\0';
	s3[0] = '\0';

	sprintf(s2, " buffer=%p, buflen=%d", buffer, buflen);
	for (i = 0; i < buflen && i < printlen; i++) {
		j = i % 16;
		if (j == 0 && i != 0) {
			DPRINTF(UDMASS_GEN, ("%s: 0x %s%s\n",
				USBDEVNAME(sc->sc_dev), s1, s2));
			s2[0] = '\0';
		}
		sprintf(&s1[j*2], "%02x", buffer[i] & 0xff);
	}
	if (buflen > printlen)
		sprintf(s3, " ...");
	DPRINTF(UDMASS_GEN, ("%s: 0x %s%s%s\n",
		USBDEVNAME(sc->sc_dev), s1, s2, s3));
}
#endif
