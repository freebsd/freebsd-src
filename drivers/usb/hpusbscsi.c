/*
 * hpusbscsi
 * (C) Copyright 2001 Oliver Neukum 
 * Sponsored by the Linux Usb Project
 * Large parts based on or taken from code by John Fremlin and Matt Dharm
 * 
 * This driver is known to work with the following scanners (VID, PID)
 *    (0x03f0, 0x0701)  HP 53xx 
 *    (0x03f0, 0x0801)  HP 7400 
 *    (0x0638, 0x026a)  Minolta Scan Dual II
 *    (0x0686, 0x4004)  Minolta Elite II
 * To load with full debugging load with "insmod hpusbscsi debug=2"
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Contributors:
 *   Oliver Neukum
 *   John Fremlin
 *   Matt Dharm
 *   .
 *   .
 *   Timothy Jedlicka <bonzo@lucent.com>
 *
 * History
 *
 * 22-Apr-2002
 *
 * - Added Elite II scanner - bonzo
 * - Cleaned up the debug statements and made them optional at load time - bonzo
 *
 * 20020618
 *
 * - Confirm to stupid 2.4 rules on io_request_lock
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/usb.h>
#include <asm/atomic.h>
#include <linux/blk.h>
#include "../scsi/scsi.h"
#include "../scsi/hosts.h"
#include "../scsi/sd.h"

#include "hpusbscsi.h"

static char *states[]={"FREE", "BEGINNING", "WORKING", "ERROR", "WAIT", "PREMATURE"};

/* DEBUG related parts */
#define HPUSBSCSI_DEBUG

#ifdef HPUSBSCSI_DEBUG
#  define PDEBUG(level, fmt, args...) \
          if (debug >= (level)) info("[%s:%d] " fmt, __PRETTY_FUNCTION__, __LINE__ , \
                 ## args)
#else
#  define PDEBUG(level, fmt, args...) do {} while(0)
#endif


/* 0=no debug messages
 * 1=everything but trace states
 * 2=trace states
 */
static int debug; /* = 0 */

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug level: 0=none, 1=no trace states, 2=trace states");

/* global variables */

struct list_head hpusbscsi_devices;
//LIST_HEAD(hpusbscsi_devices);

/* USB related parts */

static void *
hpusbscsi_usb_probe (struct usb_device *dev, unsigned int interface,
		     const struct usb_device_id *id)
{
	struct hpusbscsi *new;
	struct usb_interface_descriptor *altsetting =
		&(dev->actconfig->interface[interface].altsetting[0]);

	int i, result;

	/* basic check */

	if (altsetting->bNumEndpoints != 3) {
		printk (KERN_ERR "Wrong number of endpoints\n");
		return NULL;
	}

	/* descriptor allocation */

	new =
		(struct hpusbscsi *) kmalloc (sizeof (struct hpusbscsi),
					      GFP_KERNEL);
	if (new == NULL)
		return NULL;
	PDEBUG (1, "Allocated memory");
	memset (new, 0, sizeof (struct hpusbscsi));
	spin_lock_init (&new->dataurb.lock);
	spin_lock_init (&new->controlurb.lock);
	new->dev = dev;
	init_waitqueue_head (&new->pending);
	init_waitqueue_head (&new->deathrow);
	init_MUTEX(&new->lock);
	INIT_LIST_HEAD (&new->lh);
	
	if (id->idVendor == 0x0686 && id->idProduct == 0x4004)
		new->need_short_workaround = 1;



	/* finding endpoints */

	for (i = 0; i < altsetting->bNumEndpoints; i++) {
		if (
		    (altsetting->endpoint[i].
		     bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
		    USB_ENDPOINT_XFER_BULK) {
			if (altsetting->endpoint[i].
			    bEndpointAddress & USB_DIR_IN) {
				new->ep_in =
					altsetting->endpoint[i].
					bEndpointAddress &
					USB_ENDPOINT_NUMBER_MASK;
			} else {
				new->ep_out =
					altsetting->endpoint[i].
					bEndpointAddress &
					USB_ENDPOINT_NUMBER_MASK;
			}
		} else {
			new->ep_int =
				altsetting->endpoint[i].
				bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
			new->interrupt_interval= altsetting->endpoint[i].bInterval;
		}
	}

	/* USB initialisation magic for the simple case */

	result = usb_set_interface (dev, altsetting->bInterfaceNumber, 0);

	switch (result) {
	case 0:		/* no error */
		break;

	case -EPIPE:
		usb_clear_halt (dev, usb_sndctrlpipe (dev, 0));
		break;

	default:
		printk (KERN_ERR "unknown error %d from usb_set_interface\n",
			 result);
		goto err_out;
	}

	/* making a template for the scsi layer to fake detection of a scsi device */

	memcpy (&(new->ctempl), &hpusbscsi_scsi_host_template,
		sizeof (hpusbscsi_scsi_host_template));
	(struct hpusbscsi *) new->ctempl.proc_dir = new;
	new->ctempl.module = THIS_MODULE;

	if (scsi_register_module (MODULE_SCSI_HA, &(new->ctempl)))
		goto err_out;

	new->sense_command[0] = REQUEST_SENSE;
	new->sense_command[4] = HPUSBSCSI_SENSE_LENGTH;

	/* adding to list for module unload */
	list_add (&hpusbscsi_devices, &new->lh);

	return new;

      err_out:
	kfree (new);
	return NULL;
}

static void
hpusbscsi_usb_disconnect (struct usb_device *dev, void *ptr)
{
	struct hpusbscsi *hp = (struct hpusbscsi *)ptr;

	down(&hp->lock);
	usb_unlink_urb(&hp->controlurb);
	usb_unlink_urb(&hp->dataurb);

	hp->dev = NULL;
	up(&hp->lock);
}

static struct usb_device_id hpusbscsi_usb_ids[] = {
	{USB_DEVICE (0x03f0, 0x0701)},	/* HP 53xx */
	{USB_DEVICE (0x03f0, 0x0801)},	/* HP 7400 */
	{USB_DEVICE (0x0638, 0x0268)},  /*iVina 1200U */
	{USB_DEVICE (0x0638, 0x026a)},	/*Scan Dual II */
	{USB_DEVICE (0x0638, 0x0A13)},  /*Avision AV600U */
	{USB_DEVICE (0x0638, 0x0A16)},  /*Avision DS610CU Scancopier */
	{USB_DEVICE (0x0638, 0x0A18)},  /*Avision AV600U Plus */
	{USB_DEVICE (0x0638, 0x0A23)},  /*Avision AV220 */
	{USB_DEVICE (0x0638, 0x0A24)},  /*Avision AV210 */
	{USB_DEVICE (0x0686, 0x4004)},  /*Minolta Elite II */
	{}			/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, hpusbscsi_usb_ids);
MODULE_LICENSE("GPL");


static struct usb_driver hpusbscsi_usb_driver = {
	name:"hpusbscsi",
	probe:hpusbscsi_usb_probe,
	disconnect:hpusbscsi_usb_disconnect,
	id_table:hpusbscsi_usb_ids,
};

/* module initialisation */

int __init
hpusbscsi_init (void)
{
	int result;

	INIT_LIST_HEAD (&hpusbscsi_devices);
	PDEBUG(0, "driver loaded, DebugLvel=%d", debug);
 
	if ((result = usb_register (&hpusbscsi_usb_driver)) < 0) {
		printk (KERN_ERR "hpusbscsi: driver registration failed\n");
		return -1;
	} else {
		return 0;
	}
}

void __exit
hpusbscsi_exit (void)
{
	struct list_head *tmp;
	struct list_head *old;
	struct hpusbscsi * o;

	for (tmp = hpusbscsi_devices.next; tmp != &hpusbscsi_devices;/*nothing */) {
		old = tmp;
		tmp = tmp->next;
		o = (struct hpusbscsi *)old;
		usb_unlink_urb(&o->controlurb);
		if(scsi_unregister_module(MODULE_SCSI_HA,&o->ctempl)<0)
			printk(KERN_CRIT"Deregistering failed!\n");
		kfree(old);
	}

	usb_deregister (&hpusbscsi_usb_driver);
}

module_init (hpusbscsi_init);
module_exit (hpusbscsi_exit);

/* interface to the scsi layer */

static int
hpusbscsi_scsi_detect (struct SHT *sht)
{
	/* Whole function stolen from usb-storage */

	struct hpusbscsi *desc = (struct hpusbscsi *) sht->proc_dir;
	/* What a hideous hack! */

	char local_name[48];
	spin_unlock_irq(&io_request_lock);


	/* set up the name of our subdirectory under /proc/scsi/ */
	sprintf (local_name, "hpusbscsi-%d", desc->number);
	sht->proc_name = kmalloc (strlen (local_name) + 1, GFP_KERNEL);
	/* FIXME: where is this freed ? */

	if (!sht->proc_name) {
		spin_lock_irq(&io_request_lock);
		return 0;
	}

	strcpy (sht->proc_name, local_name);

	sht->proc_dir = NULL;

	/* build and submit an interrupt URB for status byte handling */
 	FILL_INT_URB(&desc->controlurb,
			desc->dev,
			usb_rcvintpipe(desc->dev,desc->ep_int),
			&desc->scsi_state_byte,
			1,
			control_interrupt_callback,
			desc,
			desc->interrupt_interval
	);

	if ( 0  >  usb_submit_urb(&desc->controlurb)) {
		kfree(sht->proc_name);
		spin_lock_irq(&io_request_lock);
		return 0;
	}

	/* In host->hostdata we store a pointer to desc */
	desc->host = scsi_register (sht, sizeof (desc));
	if (desc->host == NULL) {
		kfree (sht->proc_name);
		usb_unlink_urb(&desc->controlurb);
		spin_lock_irq(&io_request_lock);
		return 0;
	}
	desc->host->hostdata[0] = (unsigned long) desc;
	spin_lock_irq(&io_request_lock);

	return 1;
}

static int hpusbscsi_scsi_queuecommand (Scsi_Cmnd *srb, scsi_callback callback)
{
	struct hpusbscsi* hpusbscsi = (struct hpusbscsi*)(srb->host->hostdata[0]);
	usb_urb_callback usb_callback;
	int res, passed_length;

	spin_unlock_irq(&io_request_lock);

	/* we don't answer for anything but our single device on any faked host controller */
	if ( srb->device->lun || srb->device->id || srb->device->channel ) {
		srb->result = DID_BAD_TARGET;
		callback(srb);
		goto out_nolock;
	}

	/* to prevent a race with removal */
	down(&hpusbscsi->lock);

	if (hpusbscsi->dev == NULL) {
		srb->result = DID_ERROR;
		callback(srb);
		goto out;
	}
	
	/* otto fix - the Scan Elite II has a 5 second
	* delay anytime the srb->cmd_len=6
	* This causes it to run very slowly unless we
	* pad the command length to 10 */
        
	if (hpusbscsi -> need_short_workaround && srb->cmd_len < 10) {
		memset(srb->cmnd + srb->cmd_len, 0, 10 - srb->cmd_len);
		passed_length = 10;
	} else {
		passed_length = srb->cmd_len;
	}
        

	/* Now we need to decide which callback to give to the urb we send the command with */

	if (!srb->bufflen) {
		if (srb->cmnd[0] == REQUEST_SENSE){
			/* the usual buffer is not used, needs a special case */
			hpusbscsi->current_data_pipe = usb_rcvbulkpipe(hpusbscsi->dev, hpusbscsi->ep_in);
			usb_callback = request_sense_callback;
		} else {
			usb_callback = simple_command_callback;
		}
	} else {
        	if (srb->use_sg) {
			usb_callback = scatter_gather_callback;
			hpusbscsi->fragment = 0;
		} else {
                	usb_callback = simple_payload_callback;
		}
		/* Now we find out which direction data is to be transfered in */
		hpusbscsi->current_data_pipe = DIRECTION_IS_IN(srb->cmnd[0]) ?
			usb_rcvbulkpipe(hpusbscsi->dev, hpusbscsi->ep_in)
		:
			usb_sndbulkpipe(hpusbscsi->dev, hpusbscsi->ep_out)
		;
	}


	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
	if (hpusbscsi->state != HP_STATE_FREE) {
		printk(KERN_CRIT"hpusbscsi - Ouch: queueing violation!\n");
		return 1; /* This must not happen */
	}

        /* We zero the sense buffer to avoid confusing user space */
        memset(srb->sense_buffer, 0, SCSI_SENSE_BUFFERSIZE);

	hpusbscsi->state = HP_STATE_BEGINNING;
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);

	/* We prepare the urb for writing out the scsi command */
	FILL_BULK_URB(
		&hpusbscsi->dataurb,
		hpusbscsi->dev,
		usb_sndbulkpipe(hpusbscsi->dev,hpusbscsi->ep_out),
		srb->cmnd,
		passed_length,
		usb_callback,
		hpusbscsi
	);
	hpusbscsi->scallback = callback;
	hpusbscsi->srb = srb;


	res = usb_submit_urb(&hpusbscsi->dataurb);
	if (res) {
		hpusbscsi->state = HP_STATE_FREE;
		PDEBUG(2, "state= %s", states[hpusbscsi->state]);
		srb->result = DID_ERROR;
		callback(srb);

	}

out:
	up(&hpusbscsi->lock);
out_nolock:
	spin_lock_irq(&io_request_lock);
	return 0;
}

static int hpusbscsi_scsi_host_reset (Scsi_Cmnd *srb)
{
	struct hpusbscsi* hpusbscsi = (struct hpusbscsi*)(srb->host->hostdata[0]);

	PDEBUG(1, "SCSI reset requested");
	//usb_reset_device(hpusbscsi->dev);
	//PDEBUG(1, "SCSI reset completed");
	hpusbscsi->state = HP_STATE_FREE;

	return 0;
}

static int hpusbscsi_scsi_abort (Scsi_Cmnd *srb)
{
	struct hpusbscsi* hpusbscsi = (struct hpusbscsi*)(srb->host->hostdata[0]);
	PDEBUG(1, "Request is canceled");

	spin_unlock_irq(&io_request_lock);
	usb_unlink_urb(&hpusbscsi->dataurb);
	hpusbscsi->state = HP_STATE_FREE;

	spin_lock_irq(&io_request_lock);

	return SCSI_ABORT_PENDING;
}

/* usb interrupt handlers - they are all running IN INTERRUPT ! */

static void handle_usb_error (struct hpusbscsi *hpusbscsi)
{
	if (hpusbscsi->scallback != NULL) {
		hpusbscsi->srb->result = DID_ERROR;
		hpusbscsi->scallback(hpusbscsi->srb);
	}
	hpusbscsi->state = HP_STATE_FREE;
}

static void  control_interrupt_callback (struct urb *u)
{
	struct hpusbscsi * hpusbscsi = (struct hpusbscsi *)u->context;
	u8 scsi_state;

	PDEBUG(1, "Getting status byte %d",hpusbscsi->scsi_state_byte);
	if(u->status < 0) {
                if (hpusbscsi->state != HP_STATE_FREE)
                        handle_usb_error(hpusbscsi);
		return;
	}

	scsi_state = hpusbscsi->scsi_state_byte;
        if (hpusbscsi->state != HP_STATE_ERROR) {
                hpusbscsi->srb->result &= SCSI_ERR_MASK;
                hpusbscsi->srb->result |= scsi_state;
        }

	if (scsi_state == CHECK_CONDITION << 1) {
		if (hpusbscsi->state == HP_STATE_WAIT) {
			issue_request_sense(hpusbscsi);
		} else {
			/* we request sense after an eventual data transfer */
			hpusbscsi->state = HP_STATE_ERROR;
		}
	}

	if (hpusbscsi->scallback != NULL && hpusbscsi->state == HP_STATE_WAIT && scsi_state != CHECK_CONDITION <<1)
		/* we do a callback to the scsi layer if and only if all data has been transfered */
		hpusbscsi->scallback(hpusbscsi->srb);

	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
	switch (hpusbscsi->state) {
	case HP_STATE_WAIT:
		hpusbscsi->state = HP_STATE_FREE;
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
		break;
	case HP_STATE_WORKING:
	case HP_STATE_BEGINNING:
		hpusbscsi->state = HP_STATE_PREMATURE;
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
		break;
	case HP_STATE_ERROR:
		break;
	default:
		printk(KERN_ERR"hpusbscsi: Unexpected status report.\n");
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
		hpusbscsi->state = HP_STATE_FREE;
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
		break;
	}
}

static void simple_command_callback(struct urb *u)
{
	struct hpusbscsi * hpusbscsi = (struct hpusbscsi *)u->context;
	if (u->status<0) {
		handle_usb_error(hpusbscsi);
		return;
        }
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
	if (hpusbscsi->state != HP_STATE_PREMATURE) {
	        PDEBUG(2, "state= %s", states[hpusbscsi->state]);
		hpusbscsi->state = HP_STATE_WAIT;
	} else {
		if (hpusbscsi->scallback != NULL)
			hpusbscsi->scallback(hpusbscsi->srb);
		hpusbscsi->state = HP_STATE_FREE;
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
	}
}

static void scatter_gather_callback(struct urb *u)
{
	struct hpusbscsi * hpusbscsi = (struct hpusbscsi *)u->context;
        struct scatterlist *sg = hpusbscsi->srb->buffer;
        usb_urb_callback callback;
        int res;

        PDEBUG(1, "Going through scatter/gather"); // bonzo - this gets hit a lot - maybe make it a 2
        if (u->status < 0) {
                handle_usb_error(hpusbscsi);
                return;
        }

        if (hpusbscsi->fragment + 1 != hpusbscsi->srb->use_sg)
                callback = scatter_gather_callback;
        else
                callback = simple_done;

	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
        if (hpusbscsi->state != HP_STATE_PREMATURE)
		hpusbscsi->state = HP_STATE_WORKING;
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);

        FILL_BULK_URB(
                u,
                hpusbscsi->dev,
                hpusbscsi->current_data_pipe,
                sg[hpusbscsi->fragment].address,
                sg[hpusbscsi->fragment++].length,
                callback,
                hpusbscsi
        );

        res = usb_submit_urb(u);
        if (res)
        	handle_usb_error(hpusbscsi);
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
}

static void simple_done (struct urb *u)
{
	struct hpusbscsi * hpusbscsi = (struct hpusbscsi *)u->context;

        if (u->status < 0) {
                handle_usb_error(hpusbscsi);
                return;
        }
	PDEBUG(1, "Data transfer done");
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
	if (hpusbscsi->state != HP_STATE_PREMATURE) {
		if (u->status < 0) {
			handle_usb_error(hpusbscsi);
		} else {
			if (hpusbscsi->state != HP_STATE_ERROR) {
				hpusbscsi->state = HP_STATE_WAIT;
			} else {
				issue_request_sense(hpusbscsi);
			}
		PDEBUG(2, "state= %s", states[hpusbscsi->state]);
		}
	} else {
		if (hpusbscsi->scallback != NULL)
			hpusbscsi->scallback(hpusbscsi->srb);
		hpusbscsi->state = HP_STATE_FREE;
	}
}

static void simple_payload_callback (struct urb *u)
{
	struct hpusbscsi * hpusbscsi = (struct hpusbscsi *)u->context;
	int res;

	if (u->status<0) {
                handle_usb_error(hpusbscsi);
		return;
        }

	FILL_BULK_URB(
		u,
		hpusbscsi->dev,
		hpusbscsi->current_data_pipe,
		hpusbscsi->srb->buffer,
		hpusbscsi->srb->bufflen,
		simple_done,
		hpusbscsi
	);

	res = usb_submit_urb(u);
	if (res) {
                handle_usb_error(hpusbscsi);
		return;
        }
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
	if (hpusbscsi->state != HP_STATE_PREMATURE) {
		hpusbscsi->state = HP_STATE_WORKING;
	PDEBUG(2, "state= %s", states[hpusbscsi->state]);
	}
}

static void request_sense_callback (struct urb *u)
{
	struct hpusbscsi * hpusbscsi = (struct hpusbscsi *)u->context;

	if (u->status<0) {
                handle_usb_error(hpusbscsi);
		return;
        }

	FILL_BULK_URB(
		u,
		hpusbscsi->dev,
		hpusbscsi->current_data_pipe,
		hpusbscsi->srb->sense_buffer,
		SCSI_SENSE_BUFFERSIZE,
		simple_done,
		hpusbscsi
	);

	if (0 > usb_submit_urb(u)) {
		handle_usb_error(hpusbscsi);
		return;
	}
	if (hpusbscsi->state != HP_STATE_PREMATURE && hpusbscsi->state != HP_STATE_ERROR)
		hpusbscsi->state = HP_STATE_WORKING;
}

static void issue_request_sense (struct hpusbscsi *hpusbscsi)
{
	FILL_BULK_URB(
		&hpusbscsi->dataurb,
		hpusbscsi->dev,
		usb_sndbulkpipe(hpusbscsi->dev, hpusbscsi->ep_out),
		&hpusbscsi->sense_command,
		SENSE_COMMAND_SIZE,
		request_sense_callback,
		hpusbscsi
	);

	hpusbscsi->current_data_pipe = usb_rcvbulkpipe(hpusbscsi->dev, hpusbscsi->ep_in);

	if (0 > usb_submit_urb(&hpusbscsi->dataurb)) {
		handle_usb_error(hpusbscsi);
	}
}

