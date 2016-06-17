/*****************************************************************************/
/*
 *      auermain.c  --  Auerswald PBX/System Telephone usb driver.
 *
 *      Copyright (C) 2002-2004  Wolfgang Mües (wolfgang@iksw-muees.de)
 *
 *      Very much code of this driver is borrowed from dabusb.c (Deti Fliegl)
 *      and from the USB Skeleton driver (Greg Kroah-Hartman). Thank you.
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

/* Standard Linux module include files */
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/devfs_fs_kernel.h>
#undef DEBUG			/* include debug macros until it's done */
#include <linux/usb.h>
#include "auerchain.h"
#include "auerbuf.h"
#include "auerchar.h"
#include "auerserv.h"
#include "auermain.h"
#include "auerisdn.h"

/*-------------------------------------------------------------------*/
/* Debug support 						     */
#ifdef DEBUG
#define dump( desc, adr, len) \
do {			\
	unsigned int u;	\
	printk (KERN_DEBUG); \
	printk (desc); \
	for (u = 0; u < len; u++) \
		printk (" %02X", adr[u] & 0xFF); \
	printk ("\n"); \
} while (0)
#else
#define dump( desc, adr, len)
#endif

/*-------------------------------------------------------------------*/
/* Version Information */
#define DRIVER_VERSION "1.2.6"
#define DRIVER_AUTHOR  "Wolfgang Mües <wolfgang@iksw-muees.de>"
#define DRIVER_DESC    "Auerswald PBX/System Telephone usb driver"

/*-------------------------------------------------------------------*/
/* Internal data structures                                          */

/* the global usb devfs handle */
extern devfs_handle_t usb_devfs_handle;

/* array of pointers to our devices that are currently connected */
struct auerswald *auerdev_table[AUER_MAX_DEVICES];

/* lock to protect the auerdev_table structure */
struct semaphore auerdev_table_mutex;

/*-------------------------------------------------------------------*/
/* Forwards */
static void auerswald_ctrlread_complete(struct urb *urb);

/*-------------------------------------------------------------------*/
/* Completion handlers */

/* Values of urb->status or results of usb_submit_urb():
0		Initial, OK
-EINPROGRESS	during submission until end
-ENOENT		if urb is unlinked
-ETIMEDOUT	Transfer timed out, NAK
-ENOMEM		Memory Overflow
-ENODEV		Specified USB-device or bus doesn't exist
-ENXIO		URB already queued
-EINVAL		a) Invalid transfer type specified (or not supported)
		b) Invalid interrupt interval (0n256)
-EAGAIN		a) Specified ISO start frame too early
		b) (using ISO-ASAP) Too much scheduled for the future wait some time and try again.
-EFBIG		Too much ISO frames requested (currently uhci900)
-EPIPE		Specified pipe-handle/Endpoint is already stalled
-EMSGSIZE	Endpoint message size is zero, do interface/alternate setting
-EPROTO		a) Bitstuff error
		b) Unknown USB error
-EILSEQ		CRC mismatch
-ENOSR		Buffer error
-EREMOTEIO	Short packet detected
-EXDEV		ISO transfer only partially completed look at individual frame status for details
-EINVAL		ISO madness, if this happens: Log off and go home
-EOVERFLOW	babble
*/

/* check if a status code allows a retry */
static int auerswald_status_retry(int status)
{
	switch (status) {
	case 0:
	case -ETIMEDOUT:
	case -EOVERFLOW:
	case -EAGAIN:
	case -EPIPE:
	case -EPROTO:
	case -EILSEQ:
	case -ENOSR:
	case -EREMOTEIO:
		return 1;	/* do a retry */
	}
	return 0;		/* no retry possible */
}


/* Completion of asynchronous write block */
void auerchar_ctrlwrite_complete(struct urb *urb)
{
	struct auerbuf *bp = (struct auerbuf *) urb->context;
	struct auerswald *cp =
	    ((struct auerswald *) ((char *) (bp->list) -
				   (unsigned
				    long) (&((struct auerswald *) 0)->
					   bufctl)));
	dbg("auerchar_ctrlwrite_complete called");

	/* reuse the buffer */
	auerbuf_releasebuf(bp);
	/* Wake up all processes waiting for a buffer */
	wake_up(&cp->bufferwait);
}

/* Completion handler for dummy retry packet */
static void auerswald_ctrlread_wretcomplete(struct urb *urb)
{
	struct auerbuf *bp = (struct auerbuf *) urb->context;
	struct auerswald *cp;
	int ret;
	dbg("auerswald_ctrlread_wretcomplete called");
	dbg("complete with status: %d", urb->status);
	cp = ((struct auerswald *) ((char *) (bp->list) -
				    (unsigned
				     long) (&((struct auerswald *) 0)->
					    bufctl)));

	/* check if it is possible to advance */
	if (!auerswald_status_retry(urb->status) || !cp->usbdev) {
		/* reuse the buffer */
		err("control dummy: transmission error %d, can not retry",
		    urb->status);
		auerbuf_releasebuf(bp);
		/* Wake up all processes waiting for a buffer */
		wake_up(&cp->bufferwait);
		return;
	}

	/* fill the control message */
	bp->dr->bRequestType = AUT_RREQ;
	bp->dr->bRequest = AUV_RBLOCK;
	bp->dr->wLength = bp->dr->wValue;	/* temporary stored */
	bp->dr->wValue = cpu_to_le16(1);	/* Retry Flag */
	/* bp->dr->wIndex    = channel id;          remains */
	FILL_CONTROL_URB(bp->urbp, cp->usbdev,
			 usb_rcvctrlpipe(cp->usbdev, 0),
			 (unsigned char *) bp->dr, bp->bufp,
			 le16_to_cpu(bp->dr->wLength),
			 (usb_complete_t) auerswald_ctrlread_complete, bp);

	/* submit the control msg as next paket */
	ret = auerchain_submit_urb_list(&cp->controlchain, bp->urbp, 1);
	if (ret) {
		dbg("auerswald_ctrlread_complete: nonzero result of auerchain_submit_urb_list %d", ret);
		bp->urbp->status = ret;
		auerswald_ctrlread_complete(bp->urbp);
	}
}

/* completion handler for receiving of control messages */
static void auerswald_ctrlread_complete(struct urb *urb)
{
	unsigned int serviceid;
	struct auerswald *cp;
	struct auerscon *scp;
	struct auerbuf *bp = (struct auerbuf *) urb->context;
	int ret;
	dbg("auerswald_ctrlread_complete called");

	cp = ((struct auerswald *) ((char *) (bp->list) -
				    (unsigned
				     long) (&((struct auerswald *) 0)->
					    bufctl)));

	/* check if there is valid data in this urb */
	if (urb->status) {
		dbg("complete with non-zero status: %d", urb->status);
		/* should we do a retry? */
		if (!auerswald_status_retry(urb->status)
		    || !cp->usbdev || (cp->version < AUV_RETRY)
		    || (bp->retries >= AU_RETRIES)) {
			/* reuse the buffer */
			err("control read: transmission error %d, can not retry", urb->status);
			auerbuf_releasebuf(bp);
			/* Wake up all processes waiting for a buffer */
			wake_up(&cp->bufferwait);
			return;
		}
		bp->retries++;
		dbg("Retry count = %d", bp->retries);
		/* send a long dummy control-write-message to allow device firmware to react */
		bp->dr->bRequestType = AUT_WREQ;
		bp->dr->bRequest = AUV_DUMMY;
		bp->dr->wValue = bp->dr->wLength;	/* temporary storage */
		// bp->dr->wIndex    channel ID remains
		bp->dr->wLength = cpu_to_le16(32);	/* >= 8 bytes */
		FILL_CONTROL_URB(bp->urbp, cp->usbdev,
				 usb_sndctrlpipe(cp->usbdev, 0),
				 (unsigned char *) bp->dr, bp->bufp, 32,
				 (usb_complete_t)
				 auerswald_ctrlread_wretcomplete, bp);

		/* submit the control msg as next paket */
		ret =
		    auerchain_submit_urb_list(&cp->controlchain, bp->urbp,
					      1);
		if (ret) {
			dbg("auerswald_ctrlread_complete: nonzero result of auerchain_submit_urb_list %d", ret);
			bp->urbp->status = ret;
			auerswald_ctrlread_wretcomplete(bp->urbp);
		}
		return;
	}

	/* get the actual bytecount (incl. headerbyte) */
	bp->len = urb->actual_length;
	serviceid = bp->bufp[0] & AUH_TYPEMASK;
	dbg("Paket with serviceid %d and %d bytes received", serviceid,
	    bp->len);

	/* dispatch the paket */
	scp = cp->services[serviceid];
	if (scp) {
		/* look, Ma, a listener! */
		scp->dispatch(scp, bp);
	}

	/* release the paket */
	auerbuf_releasebuf(bp);
	/* Wake up all processes waiting for a buffer */
	wake_up(&cp->bufferwait);
}

/*-------------------------------------------------------------------*/
/* Handling of Interrupt Endpoint                                    */
/* This interrupt Endpoint is used to inform the host about waiting
   messages from the USB device.
*/
/* int completion handler. */
static void auerswald_int_complete(struct urb *urb)
{
	unsigned int channelid;
	unsigned int bytecount;
	int ret;
	struct auerbuf *bp = NULL;
	struct auerswald *cp = (struct auerswald *) urb->context;

	dbg("auerswald_int_complete called");

	/* do not respond to an error condition */
	if (urb->status != 0) {
		dbg("nonzero URB status = %d", urb->status);
		return;
	}

	/* check if all needed data was received */
	if (urb->actual_length < AU_IRQMINSIZE) {
		dbg("invalid data length received: %d bytes",
		    urb->actual_length);
		return;
	}

	/* check the command code */
	if (cp->intbufp[0] != AU_IRQCMDID) {
		dbg("invalid command received: %d", cp->intbufp[0]);
		return;
	}

	/* check the command type */
	if (cp->intbufp[1] != AU_BLOCKRDY) {
		dbg("invalid command type received: %d", cp->intbufp[1]);
		return;
	}

	/* now extract the information */
	channelid = cp->intbufp[2];
	bytecount = le16_to_cpup(&cp->intbufp[3]);

	/* check the channel id */
	if (channelid >= AUH_TYPESIZE) {
		dbg("invalid channel id received: %d", channelid);
		return;
	}

	/* check the byte count */
	if (bytecount > (cp->maxControlLength + AUH_SIZE)) {
		dbg("invalid byte count received: %d", bytecount);
		return;
	}
	dbg("Service Channel = %d", channelid);
	dbg("Byte Count = %d", bytecount);

	/* get a buffer for the next data paket */
	bp = auerbuf_getbuf(&cp->bufctl);
	/* if no buffer available: skip it */
	if (!bp) {
		dbg("auerswald_int_complete: no data buffer available");
		/* can we do something more?
		   This is a big problem: if this int packet is ignored, the
		   device will wait forever and not signal any more data.
		   The only real solution is: having enought buffers!
		   Or perhaps temporary disabling the int endpoint?
		 */
		return;
	}

	/* fill the control message */
	bp->dr->bRequestType = AUT_RREQ;
	bp->dr->bRequest = AUV_RBLOCK;
	bp->dr->wValue = cpu_to_le16(0);
	bp->dr->wIndex = cpu_to_le16(channelid | AUH_DIRECT | AUH_UNSPLIT);
	bp->dr->wLength = cpu_to_le16(bytecount);
	FILL_CONTROL_URB(bp->urbp, cp->usbdev,
			 usb_rcvctrlpipe(cp->usbdev, 0),
			 (unsigned char *) bp->dr, bp->bufp, bytecount,
			 (usb_complete_t) auerswald_ctrlread_complete, bp);

	/* submit the control msg */
	ret = auerchain_submit_urb(&cp->controlchain, bp->urbp);
	if (ret) {
		dbg("auerswald_int_complete: nonzero result of auerchain_submit_urb %d", ret);
		bp->urbp->status = ret;
		auerswald_ctrlread_complete(bp->urbp);
		/* here applies the same problem as above: device locking! */
	}
}

/* int memory deallocation
   NOTE: no mutex please!
*/
static void auerswald_int_free(struct auerswald *cp)
{
	if (cp->inturbp) {
		usb_free_urb(cp->inturbp);
		cp->inturbp = NULL;
	}
	kfree(cp->intbufp);
}

/* This function is called to activate the interrupt
   endpoint. This function returns 0 if successfull or an error code.
   NOTE: no mutex please!
*/
static int auerswald_int_open(struct auerswald *cp)
{
	int ret;
	struct usb_endpoint_descriptor *ep;
	int irqsize;
	dbg("auerswald_int_open");

	ep = usb_epnum_to_ep_desc(cp->usbdev, USB_DIR_IN | AU_IRQENDP);
	if (!ep) {
		ret = -EFAULT;
		goto intoend;
	}
	irqsize = ep->wMaxPacketSize;
	cp->irqsize = irqsize;

	/* allocate the urb and data buffer */
	if (!cp->inturbp) {
		cp->inturbp = usb_alloc_urb(0);
		if (!cp->inturbp) {
			ret = -ENOMEM;
			goto intoend;
		}
	}
	if (!cp->intbufp) {
		cp->intbufp = (char *) kmalloc(irqsize, GFP_KERNEL);
		if (!cp->intbufp) {
			ret = -ENOMEM;
			goto intoend;
		}
	}
	/* setup urb */
	FILL_INT_URB(cp->inturbp, cp->usbdev,
		     usb_rcvintpipe(cp->usbdev, AU_IRQENDP), cp->intbufp,
		     irqsize, auerswald_int_complete, cp, ep->bInterval);
	/* start the urb */
	cp->inturbp->status = 0;	/* needed! */
	ret = usb_submit_urb(cp->inturbp);

      intoend:
	if (ret < 0) {
		/* activation of interrupt endpoint has failed. Now clean up. */
		dbg("auerswald_int_open: activation of int endpoint failed");

		/* deallocate memory */
		auerswald_int_free(cp);
	}
	return ret;
}

/* This function is called to deactivate the interrupt
   endpoint. This function returns 0 if successfull or an error code.
   NOTE: no mutex please!
*/
static int auerswald_int_release(struct auerswald *cp)
{
	int ret = 0;
	dbg("auerswald_int_release");

	/* stop the int endpoint */
	if (cp->inturbp) {
		ret = usb_unlink_urb(cp->inturbp);
		if (ret)
			dbg("nonzero int unlink result received: %d", ret);
	}

	/* deallocate memory */
	auerswald_int_free(cp);

	return ret;
}

/* --------------------------------------------------------------------- */
/* Helper functions                                                      */

/* Delete an auerswald driver context */
void auerswald_delete(struct auerswald *cp)
{
	dbg("auerswald_delete");
	if (cp == NULL)
		return;

	/* Wake up all processes waiting for a buffer */
	wake_up(&cp->bufferwait);

	/* Cleaning up */
	auerisdn_disconnect(cp);
	auerswald_int_release(cp);
	auerchain_free(&cp->controlchain);
	auerbuf_free_buffers(&cp->bufctl);

	/* release the memory */
	kfree(cp);
}


/* add a new service to the device
   scp->id must be set!
   return: 0 if OK, else error code
*/
int auerswald_addservice(struct auerswald *cp, struct auerscon *scp)
{
	int ret;

	/* is the device available? */
	if (!cp->usbdev) {
		dbg("usbdev == NULL");
		return -EIO;	/*no: can not add a service, sorry */
	}

	/* is the service available? */
	if (cp->services[scp->id]) {
		dbg("service is busy");
		return -EBUSY;
	}

	/* device is available, service is free */
	cp->services[scp->id] = scp;

	/* register service in device */
	ret = auerchain_control_msg(&cp->controlchain,			/* pointer to control chain */
				    cp->usbdev,				/* pointer to device */
				    usb_sndctrlpipe(cp->usbdev, 0),	/* pipe to control endpoint */
				    AUV_CHANNELCTL,			/* USB message request value */
				    AUT_WREQ,				/* USB message request type value */
				    0x01,	/* open */		/* USB message value */
				    scp->id,				/* USB message index value */
				    NULL,				/* pointer to the data to send */
				    0,					/* length in bytes of the data to send */
				    HZ * 2);				/* time to wait for the message to complete before timing out */
	if (ret < 0) {
		dbg("auerswald_addservice: auerchain_control_msg returned error code %d", ret);
		/* undo above actions */
		cp->services[scp->id] = NULL;
		return ret;
	}

	dbg("auerswald_addservice: channel open OK");
	return 0;
}


/* remove a service from the the device
   scp->id must be set! */
void auerswald_removeservice(struct auerswald *cp, struct auerscon *scp)
{
	dbg("auerswald_removeservice called");

	/* check if we have a service allocated */
	if (scp->id == AUH_UNASSIGNED)
		return;

	/* If there is a device: close the channel */
	if (cp->usbdev && !cp->disconnecting) {
		/* Close the service channel inside the device */
		int ret = auerchain_control_msg(&cp->controlchain,		/* pointer to control chain */
						cp->usbdev,			/* pointer to device */
						usb_sndctrlpipe(cp->usbdev, 0),	/* pipe to control endpoint */
						AUV_CHANNELCTL,			/* USB message request value */
						AUT_WREQ,			/* USB message request type value */
						0x00,	/* close */		/* USB message value */
						scp->id,			/* USB message index value */
						NULL,				/* pointer to the data to send */
						0,				/* length in bytes of the data to send */
						HZ * 2);			/* time to wait for the message to complete before timing out */
		if (ret < 0) {
			dbg("auerswald_removeservice: auerchain_control_msg returned error code %d", ret);
		} else {
			dbg("auerswald_removeservice: channel close OK");
		}
	}

	/* remove the service from the device */
	cp->services[scp->id] = NULL;
	scp->id = AUH_UNASSIGNED;
}


/*----------------------------------------------------------------------*/
/* File operation structure                                             */
static struct file_operations auerswald_fops = {
	owner:THIS_MODULE,
	llseek:auerchar_llseek,
	read:auerchar_read,
	write:auerchar_write,
	ioctl:auerchar_ioctl,
	open:auerchar_open,
	release:auerchar_release,
};

/* --------------------------------------------------------------------- */
/* Special USB driver functions                                          */

/* Probe if this driver wants to serve an USB device

   This entry point is called whenever a new device is attached to the bus.
   Then the device driver has to create a new instance of its internal data
   structures for the new device.

   The  dev argument specifies the device context, which contains pointers
   to all USB descriptors. The  interface argument specifies the interface
   number. If a USB driver wants to bind itself to a particular device and
   interface it has to return a pointer. This pointer normally references
   the device driver's context structure.

   Probing normally is done by checking the vendor and product identifications
   or the class and subclass definitions. If they match the interface number
   is compared with the ones supported by the driver. When probing is done
   class based it might be necessary to parse some more USB descriptors because
   the device properties can differ in a wide range.
*/
static void *auerswald_probe(struct usb_device *usbdev, unsigned int ifnum,
			     const struct usb_device_id *id)
{
	struct auerswald *cp = NULL;
	DECLARE_WAIT_QUEUE_HEAD(wqh);
	unsigned int dtindex;
	unsigned int u = 0;
	char *pbuf;
	int ret;

	dbg("probe: vendor id 0x%x, device id 0x%x ifnum:%d",
	    usbdev->descriptor.idVendor, usbdev->descriptor.idProduct,
	    ifnum);

	/* See if the device offered us matches that we can accept */
	if (usbdev->descriptor.idVendor != ID_AUERSWALD)
		return NULL;

	/* we use only the first -and only- interface */
	if (ifnum != 0)
		return NULL;

	/* prevent module unloading while sleeping */
	MOD_INC_USE_COUNT;

	/* allocate memory for our device and intialize it */
	cp = kmalloc(sizeof(struct auerswald), GFP_KERNEL);
	if (cp == NULL) {
		err("out of memory");
		goto pfail;
	}

	/* Initialize device descriptor */
	memset(cp, 0, sizeof(struct auerswald));
	init_MUTEX(&cp->mutex);
	cp->usbdev = usbdev;
	auerchain_init(&cp->controlchain);
	auerbuf_init(&cp->bufctl);
	init_waitqueue_head(&cp->bufferwait);
	auerisdn_init_dev(cp);

	/* find a free slot in the device table */
	down(&auerdev_table_mutex);
	for (dtindex = 0; dtindex < AUER_MAX_DEVICES; ++dtindex) {
		if (auerdev_table[dtindex] == NULL)
			break;
	}
	if (dtindex >= AUER_MAX_DEVICES) {
		err("more than %d devices plugged in, can not handle this device", AUER_MAX_DEVICES);
		up(&auerdev_table_mutex);
		goto pfail;
	}

	/* Give the device a name */
	sprintf(cp->name, AU_PREFIX "%d", dtindex);

	/* Store the index */
	cp->dtindex = dtindex;
	auerdev_table[dtindex] = cp;
	up(&auerdev_table_mutex);

	/* initialize the devfs node for this device and register it */
	cp->devfs = devfs_register(usb_devfs_handle, cp->name,
				   DEVFS_FL_DEFAULT, USB_MAJOR,
				   AUER_MINOR_BASE + dtindex,
				   S_IFCHR | S_IRUGO | S_IWUGO,
				   &auerswald_fops, NULL);

	/* Get the usb version of the device */
	cp->version = cp->usbdev->descriptor.bcdDevice;
	dbg("Version is %X", cp->version);

	/* allow some time to settle the device */
	sleep_on_timeout(&wqh, HZ / 3);

	/* Try to get a suitable textual description of the device */
	/* Device name: */
	ret =
	    usb_string(cp->usbdev, AUSI_DEVICE, cp->dev_desc,
		       AUSI_DLEN - 1);
	if (ret >= 0) {
		u += ret;
		/* Append Serial Number */
		memcpy(&cp->dev_desc[u], ",Ser# ", 6);
		u += 6;
		ret =
		    usb_string(cp->usbdev, AUSI_SERIALNR, &cp->dev_desc[u],
			       AUSI_DLEN - u - 1);
		if (ret >= 0) {
			u += ret;
			/* Append subscriber number */
			memcpy(&cp->dev_desc[u], ", ", 2);
			u += 2;
			ret =
			    usb_string(cp->usbdev, AUSI_MSN,
				       &cp->dev_desc[u],
				       AUSI_DLEN - u - 1);
			if (ret >= 0) {
				u += ret;
			}
		}
	}
	cp->dev_desc[u] = '\0';
	info("device is a %s", cp->dev_desc);

	/* get the maximum allowed control transfer length */
	pbuf = (char *) kmalloc(2, GFP_KERNEL);	/* use an allocated buffer because of urb target */
	if (!pbuf) {
		err("out of memory");
		goto pfail;
	}
	ret = usb_control_msg(cp->usbdev,			/* pointer to device */
			      usb_rcvctrlpipe(cp->usbdev, 0),	/* pipe to control endpoint */
			      AUV_GETINFO,			/* USB message request value */
			      AUT_RREQ,				/* USB message request type value */
			      0,				/* USB message value */
			      AUDI_MBCTRANS,			/* USB message index value */
			      pbuf,				/* pointer to the receive buffer */
			      2,				/* length of the buffer */
			      HZ * 2);				/* time to wait for the message to complete before timing out */
	if (ret == 2) {
		cp->maxControlLength = le16_to_cpup(pbuf);
		kfree(pbuf);
		dbg("setup: max. allowed control transfersize is %d bytes",
		    cp->maxControlLength);
	} else {
		kfree(pbuf);
		err("setup: getting max. allowed control transfer length failed with error %d", ret);
		goto pfail;
	}
	/* allocate a chain for the control messages */
	if (auerchain_setup(&cp->controlchain, AUCH_ELEMENTS)) {
		err("out of memory");
		goto pfail;
	}

	/* allocate buffers for control messages */
	if (auerbuf_setup
	    (&cp->bufctl, AU_RBUFFERS * 2,
	     cp->maxControlLength + AUH_SIZE)) {
		err("out of memory");
		goto pfail;
	}

	/* start the interrupt endpoint */
	if (auerswald_int_open(cp)) {
		err("int endpoint failed");
		goto pfail;
	}

	/* Try to connect to hisax interface */
	if (auerisdn_probe(cp)) {
		err("hisax connect failed");
		goto pfail;
	}

	/* all OK */
	return cp;

	/* Error exit: clean up the memory */
      pfail:auerswald_delete(cp);
	MOD_DEC_USE_COUNT;
	return NULL;
}


/* Disconnect driver from a served device

   This function is called whenever a device which was served by this driver
   is disconnected.

   The argument  dev specifies the device context and the  driver_context
   returns a pointer to the previously registered  driver_context of the
   probe function. After returning from the disconnect function the USB
   framework completly deallocates all data structures associated with
   this device. So especially the usb_device structure must not be used
   any longer by the usb driver.
*/
static void auerswald_disconnect(struct usb_device *usbdev,
				 void *driver_context)
{
	struct auerswald *cp = (struct auerswald *) driver_context;
	unsigned int u;

	/* all parallel tasks can react on disconnect ASAP */
	cp->disconnecting = 1;
	down(&cp->mutex);
	info("device /dev/usb/%s now disconnecting", cp->name);

	/* remove from device table */
	/* Nobody can open() this device any more */
	down(&auerdev_table_mutex);
	auerdev_table[cp->dtindex] = NULL;
	up(&auerdev_table_mutex);

	/* remove our devfs node */
	/* Nobody can see this device any more */
	devfs_unregister(cp->devfs);

	/* stop the ISDN connection */
	auerisdn_disconnect(cp);

	/* Stop the interrupt endpoint 0 */
	auerswald_int_release(cp);

	/* remove the control chain allocated in auerswald_probe
	   This has the benefit of
	   a) all pending (a)synchronous urbs are unlinked
	   b) all buffers dealing with urbs are reclaimed
	 */
	auerchain_free(&cp->controlchain);

	if (cp->open_count == 0) {
		struct auerscon *scp;
		/* nobody is using this device. So we can clean up now */
		up(&cp->mutex);	/* up() is possible here because no other task
				   can open the device (see above). I don't want
				   to kfree() a locked mutex. */
		/* disconnect the D channel */
		scp = cp->services[AUH_DCHANNEL];
		if (scp)
			scp->disconnect(scp);
		auerswald_delete(cp);
	} else {
		/* device is used. Remove the pointer to the
		   usb device (it's not valid any more). The last
		   release() will do the clean up */
		cp->usbdev = NULL;
		up(&cp->mutex);
		/* Terminate waiting writers */
		wake_up(&cp->bufferwait);
		/* Inform all waiting readers */
		for (u = 0; u < AUH_TYPESIZE; u++) {
			struct auerscon *scp = cp->services[u];
			if (scp)
				scp->disconnect(scp);
		}
	}

	/* The device releases this module */
	MOD_DEC_USE_COUNT;
}

/* Descriptor for the devices which are served by this driver.
   NOTE: this struct is parsed by the usbmanager install scripts.
         Don't change without caution!
*/
static struct usb_device_id auerswald_ids[] = {
	{USB_DEVICE(ID_AUERSWALD, 0x00C0)},	/* COMpact 2104 USB/DSL */
	{USB_DEVICE(ID_AUERSWALD, 0x00DB)},	/* COMpact 4410/2206 USB */
	{USB_DEVICE(ID_AUERSWALD, 0x00DC)},	/* COMpact 4406 DSL */
	{USB_DEVICE(ID_AUERSWALD, 0x00DD)},	/* COMpact 2204 USB */
	{USB_DEVICE(ID_AUERSWALD, 0x00F1)},	/* COMfort 2000 System Telephone */
	{USB_DEVICE(ID_AUERSWALD, 0x00F2)},	/* COMfort 1200 System Telephone */
	{}					/* Terminating entry */
};

/* Standard module device table */
MODULE_DEVICE_TABLE(usb, auerswald_ids);

/* Standard usb driver struct */
static struct usb_driver auerswald_driver = {
	name:"auerswald",
	probe:auerswald_probe,
	disconnect:auerswald_disconnect,
	fops:&auerswald_fops,
	minor:AUER_MINOR_BASE,
	id_table:auerswald_ids,
};


/* --------------------------------------------------------------------- */
/* Module loading/unloading                                              */

/* Driver initialisation. Called after module loading.
   NOTE: there is no concurrency at _init
*/
static int __init auerswald_init(void)
{
	int result;
	dbg("init");

	/* initialize the device table */
	memset(&auerdev_table, 0, sizeof(auerdev_table));
	init_MUTEX(&auerdev_table_mutex);
	auerisdn_init();

	/* register driver at the USB subsystem */
	/* NOTE: usb_register() may call probe()! */
	result = usb_register(&auerswald_driver);
	if (result < 0) {
		err("driver could not be registered");
		return -1;
	}
	return 0;
}

/* Driver deinit. Called before module removal.
   NOTE: there is no concurrency at _cleanup
*/
static void __exit auerswald_cleanup(void)
{
	dbg("cleanup");
	auerisdn_cleanup();
	usb_deregister(&auerswald_driver);
}

/* --------------------------------------------------------------------- */
/* Linux device driver module description                                */

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

module_init(auerswald_init);
module_exit(auerswald_cleanup);

/* --------------------------------------------------------------------- */
