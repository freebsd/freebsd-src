/*****************************************************************************/
/*
 *      auermain.h  --  Auerswald PBX/System Telephone usb driver.
 *
 *      Copyright (C) 2002  Wolfgang Mües (wolfgang@iksw-muees.de)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 /*****************************************************************************/

#ifndef AUERMAIN_H
#define AUERMAIN_H

#include <linux/devfs_fs_kernel.h>
#include <linux/usb.h>
#include "auerchain.h"
#include "auerbuf.h"
#include "auerserv.h"
#include "auerisdn.h"

/*-------------------------------------------------------------------*/
/* Private declarations for Auerswald USB driver                     */

/* Auerswald Vendor ID */
#define ID_AUERSWALD  	0x09BF

#ifndef AUER_MINOR_BASE		/* allow external override */
#define AUER_MINOR_BASE	112	/* auerswald driver minor number */
#endif

/* we can have up to this number of device plugged in at once */
#define AUER_MAX_DEVICES 16

/* prefix for the device descriptors in /dev/usb */
#define AU_PREFIX	"auer"

/* Number of read buffers for each device */
#define AU_RBUFFERS     10

/* Number of chain elements for each control chain */
#define AUCH_ELEMENTS   20

/* Number of retries in communication */
#define AU_RETRIES	10

/*-------------------------------------------------------------------*/
/* vendor specific protocol                                          */
/* Header Byte */
#define AUH_INDIRMASK   0x80	/* mask for direct/indirect bit */
#define AUH_DIRECT      0x00	/* data is for USB device */
#define AUH_INDIRECT    0x80	/* USB device is relay */

#define AUH_SPLITMASK   0x40	/* mask for split bit */
#define AUH_UNSPLIT     0x00	/* data block is full-size */
#define AUH_SPLIT       0x40	/* data block is part of a larger one,
				   split-byte follows */
#define AUH_SYNC	0x40	/* Sync to start of HDLC frame for B1,B2 */

#define AUH_TYPEMASK    0x3F	/* mask for type of data transfer */
#define AUH_TYPESIZE    0x40	/* different types */
#define AUH_DCHANNEL    0x00	/* D channel data */
#define AUH_B1CHANNEL   0x01	/* B1 channel transparent */
#define AUH_B2CHANNEL   0x02	/* B2 channel transparent */
/*                0x03..0x0F       reserved for driver internal use */
#define AUH_COMMAND     0x10	/* Command channel */
#define AUH_BPROT       0x11	/* Configuration block protocol */
#define AUH_DPROTANA    0x12	/* D channel protocol analyzer */
#define AUH_TAPI        0x13	/* telephone api data (ATD) */
/*                0x14..0x3F       reserved for other protocols */
#define AUH_UNASSIGNED  0xFF	/* if char device has no assigned service */
#define AUH_FIRSTUSERCH 0x11	/* first channel which is available for driver users */

#define AUH_SIZE	1	/* Size of Header Byte */

/* Split Byte. Only present if split bit in header byte set.*/
#define AUS_STARTMASK   0x80	/* mask for first block of splitted frame */
#define AUS_FIRST       0x80	/* first block */
#define AUS_FOLLOW      0x00	/* following block */

#define AUS_ENDMASK     0x40	/* mask for last block of splitted frame */
#define AUS_END         0x40	/* last block */
#define AUS_NOEND       0x00	/* not the last block */

#define AUS_LENMASK     0x3F	/* mask for block length information */

/* Request types */
#define AUT_RREQ        (USB_DIR_IN  | USB_TYPE_VENDOR | USB_RECIP_OTHER)	/* Read Request */
#define AUT_WREQ        (USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_OTHER)	/* Write Request */

/* Vendor Requests */
#define AUV_GETINFO     0x00	/* GetDeviceInfo */
#define AUV_WBLOCK      0x01	/* Write Block */
#define AUV_RBLOCK      0x02	/* Read Block */
#define AUV_CHANNELCTL  0x03	/* Channel Control */
#define AUV_DUMMY	0x04	/* Dummy Out for retry */

/* Device Info Types */
#define AUDI_NUMBCH     0x0000	/* Number of supported B channels */
#define AUDI_OUTFSIZE   0x0001	/* Size of OUT B channel fifos */
#define AUDI_MBCTRANS   0x0002	/* max. Blocklength of control transfer */

/* Interrupt endpoint definitions */
#define AU_IRQENDP      1	/* Endpoint number */
#define AU_IRQCMDID     16	/* Command-block ID */
#define AU_BLOCKRDY     0	/* Command: Block data ready on ctl endpoint */
#define AU_IRQMINSIZE	5	/* Nr. of bytes decoded in this driver */

/* B channel Interrupt endpoint definitions */
#define AU_IRQENDPBI	2	/* Input Endpoint number */
#define AU_IRQENDPBO	3	/* Output Endpoint number for 4410, 2206 */
#define AU_IRQENDPBO_2	2	/* Output Endpoint number for 2104 */

/* Device String Descriptors */
#define AUSI_VENDOR   	1	/* "Auerswald GmbH & Co. KG" */
#define AUSI_DEVICE   	2	/* Name of the Device */
#define AUSI_SERIALNR 	3	/* Serial Number */
#define AUSI_MSN      	4	/* "MSN ..." (first) Multiple Subscriber Number */

#define AUSI_DLEN	100	/* Max. Length of Device Description */

#define AUV_RETRY	0x101	/* First Firmware version which can do control retries */

/* ...................................................................*/
/* USB device context */
struct auerswald {
	struct semaphore mutex;		/* protection in user context */
	char name[16];			/* name of the /dev/usb entry */
	unsigned int dtindex;		/* index in the device table */
	devfs_handle_t devfs;		/* devfs device node */
	struct usb_device *usbdev;	/* USB device handle */
	int open_count;			/* count the number of open character channels */
	char dev_desc[AUSI_DLEN];	/* for storing a textual description */
	unsigned int maxControlLength;	/* max. Length of control paket (without header) */
	struct urb *inturbp;		/* interrupt urb */
	char *intbufp;			/* data buffer for interrupt urb */
	unsigned int irqsize;		/* size of interrupt endpoint 1 */
	struct auerchain controlchain;	/* for chaining of control messages */
	struct auerbufctl bufctl;	/* Buffer control for control transfers */
	struct auerscon *services[AUH_TYPESIZE];/* context pointers for each service */
	unsigned int version;		/* Version of the device */
	wait_queue_head_t bufferwait;	/* wait for a control buffer */
	volatile unsigned int disconnecting;/* 1: removal in progress */
	struct auerisdn isdn;		/* ISDN-Related parameters */
};

/* array of pointers to our devices that are currently connected */
extern struct auerswald *auerdev_table[AUER_MAX_DEVICES];

/* lock to protect the auerdev_table structure */
extern struct semaphore auerdev_table_mutex;

void auerswald_removeservice(struct auerswald *cp, struct auerscon *scp);

int auerswald_addservice(struct auerswald *cp, struct auerscon *scp);

void auerchar_ctrlwrite_complete(struct urb *urb);

void auerswald_delete(struct auerswald *cp);

#endif	/* AUERMAIN_H */
