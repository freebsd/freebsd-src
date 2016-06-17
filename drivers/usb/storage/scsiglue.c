/* Driver for USB Mass Storage compliant devices
 * SCSI layer glue code
 *
 * $Id: scsiglue.c,v 1.24 2001/11/11 03:33:58 mdharm Exp $
 *
 * Current development and maintenance by:
 *   (c) 1999, 2000 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * Developed with the assistance of:
 *   (c) 2000 David L. Brown, Jr. (usb-storage@davidb.org)
 *   (c) 2000 Stephen J. Gowdy (SGowdy@lbl.gov)
 *
 * Initial work by:
 *   (c) 1999 Michael Gee (michael@linuxspecific.com)
 *
 * This driver is based on the 'USB Mass Storage Class' document. This
 * describes in detail the protocol used to communicate with such
 * devices.  Clearly, the designers had SCSI and ATAPI commands in
 * mind when they created this document.  The commands are all very
 * similar to commands in the SCSI-II and ATAPI specifications.
 *
 * It is important to note that in a number of cases this class
 * exhibits class-specific exemptions from the USB specification.
 * Notably the usage of NAK, STALL and ACK differs from the norm, in
 * that they are used to communicate wait, failed and OK on commands.
 *
 * Also, for certain devices, the interrupt endpoint is used to convey
 * status of a command.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "scsiglue.h"
#include "usb.h"
#include "debug.h"
#include "transport.h"

#include <linux/slab.h>

/*
 * kernel thread actions
 */

#define US_ACT_COMMAND		1
#define US_ACT_DEVICE_RESET	2
#define US_ACT_BUS_RESET	3
#define US_ACT_HOST_RESET	4
#define US_ACT_EXIT		5

/***********************************************************************
 * Host functions 
 ***********************************************************************/

static const char* host_info(struct Scsi_Host *host)
{
	return "SCSI emulation for USB Mass Storage devices";
}

/* detect a virtual adapter (always works) */
static int detect(struct SHT *sht)
{
	struct us_data *us;
	char local_name[32];
	/* Note: this function gets called with io_request_lock spinlock helt! */
	/* This is not nice at all, but how else are we to get the
	 * data here? */
	us = (struct us_data *)sht->proc_dir;

	/* set up the name of our subdirectory under /proc/scsi/ */
	sprintf(local_name, "usb-storage-%d", us->host_number);
	sht->proc_name = kmalloc (strlen(local_name) + 1, GFP_ATOMIC);
	if (!sht->proc_name) 
		return 0;
	strcpy(sht->proc_name, local_name);

	/* we start with no /proc directory entry */
	sht->proc_dir = NULL;

	/* register the host */
	us->host = scsi_register(sht, sizeof(us));
	if (us->host) {
		us->host->hostdata[0] = (unsigned long)us;
		us->host_no = us->host->host_no;
		return 1;
	}

	/* odd... didn't register properly.  Abort and free pointers */
	kfree(sht->proc_name);
	sht->proc_name = NULL;
	return 0;
}

/* Release all resources used by the virtual host
 *
 * NOTE: There is no contention here, because we're already deregistered
 * the driver and we're doing each virtual host in turn, not in parallel
 */
static int release(struct Scsi_Host *psh)
{
	struct us_data *us = (struct us_data *)psh->hostdata[0];

	US_DEBUGP("release() called for host %s\n", us->htmplt.name);

	/* Kill the control threads
	 *
	 * Enqueue the command, wake up the thread, and wait for 
	 * notification that it's exited.
	 */
	US_DEBUGP("-- sending US_ACT_EXIT command to thread\n");
	us->action = US_ACT_EXIT;
	
	up(&(us->sema));
	wait_for_completion(&(us->notify));

	/* remove the pointer to the data structure we were using */
	(struct us_data*)psh->hostdata[0] = NULL;

	/* we always have a successful release */
	return 0;
}

/* run command */
static int command( Scsi_Cmnd *srb )
{
	US_DEBUGP("Bad use of us_command\n");

	return DID_BAD_TARGET << 16;
}

/* run command */
static int queuecommand( Scsi_Cmnd *srb , void (*done)(Scsi_Cmnd *))
{
	struct us_data *us = (struct us_data *)srb->host->hostdata[0];
	unsigned long flags;

	US_DEBUGP("queuecommand() called\n");
	srb->host_scribble = (unsigned char *)us;

	/* get exclusive access to the structures we want */
	spin_lock_irqsave(&(us->queue_exclusion), flags);

	/* enqueue the command */
	us->queue_srb = srb;
	srb->scsi_done = done;
	us->action = US_ACT_COMMAND;

	/* release the lock on the structure */
	spin_unlock_irqrestore(&(us->queue_exclusion), flags);

	/* wake up the process task */
	up(&(us->sema));

	return 0;
}

/***********************************************************************
 * Error handling functions
 ***********************************************************************/

/* Command abort */
static int command_abort( Scsi_Cmnd *srb )
{
	struct us_data *us = (struct us_data *)srb->host->hostdata[0];

	US_DEBUGP("command_abort() called\n");

	/* if we're stuck waiting for an IRQ, simulate it */
	if (atomic_read(us->ip_wanted)) {
		US_DEBUGP("-- simulating missing IRQ\n");
		up(&(us->ip_waitq));
	}

	/* if the device has been removed, this worked */
	if (!us->pusb_dev) {
		US_DEBUGP("-- device removed already\n");
		return SUCCESS;
	}

	/* if we have an urb pending, let's wake the control thread up */
	if (!us->current_done.done) {
		atomic_inc(&us->abortcnt);
		spin_unlock_irq(&io_request_lock);
		/* cancel the URB -- this will automatically wake the thread */
		usb_unlink_urb(us->current_urb);

		/* wait for us to be done */
		wait_for_completion(&(us->notify));
		spin_lock_irq(&io_request_lock);
		atomic_dec(&us->abortcnt);
		return SUCCESS;
	}

	US_DEBUGP ("-- nothing to abort\n");
	return FAILED;
}

/* This invokes the transport reset mechanism to reset the state of the
 * device */
static int device_reset( Scsi_Cmnd *srb )
{
	struct us_data *us = (struct us_data *)srb->host->hostdata[0];
	int rc;

	US_DEBUGP("device_reset() called\n" );

	spin_unlock_irq(&io_request_lock);
	rc = us->transport_reset(us);
	spin_lock_irq(&io_request_lock);
	return rc;
}

/* This resets the device port, and simulates the device
 * disconnect/reconnect for all drivers which have claimed other
 * interfaces. */
static int bus_reset( Scsi_Cmnd *srb )
{
	struct us_data *us = (struct us_data *)srb->host->hostdata[0];
	int i;
	int result;

	/* we use the usb_reset_device() function to handle this for us */
	US_DEBUGP("bus_reset() called\n");

	/* if the device has been removed, this worked */
	if (!us->pusb_dev) {
		US_DEBUGP("-- device removed already\n");
		return SUCCESS;
	}

	spin_unlock_irq(&io_request_lock);

	/* release the IRQ, if we have one */
	down(&(us->irq_urb_sem));
	if (us->irq_urb) {
		US_DEBUGP("-- releasing irq URB\n");
		result = usb_unlink_urb(us->irq_urb);
		US_DEBUGP("-- usb_unlink_urb() returned %d\n", result);
	}
	up(&(us->irq_urb_sem));

	/* attempt to reset the port */
	if (usb_reset_device(us->pusb_dev) < 0) {
		spin_lock_irq(&io_request_lock);
		return FAILED;
	}

	/* FIXME: This needs to lock out driver probing while it's working
	 * or we can have race conditions */
        for (i = 0; i < us->pusb_dev->actconfig->bNumInterfaces; i++) {
 		struct usb_interface *intf =
			&us->pusb_dev->actconfig->interface[i];
		const struct usb_device_id *id;

		/* if this is an unclaimed interface, skip it */
		if (!intf->driver) {
			continue;
		}

		US_DEBUGP("Examinging driver %s...", intf->driver->name);
		/* skip interfaces which we've claimed */
		if (intf->driver == &usb_storage_driver) {
			US_DEBUGPX("skipping ourselves.\n");
			continue;
		}

		/* simulate a disconnect and reconnect for all interfaces */
		US_DEBUGPX("simulating disconnect/reconnect.\n");
		down(&intf->driver->serialize);
		intf->driver->disconnect(us->pusb_dev, intf->private_data);
		id = usb_match_id(us->pusb_dev, intf, intf->driver->id_table);
		intf->driver->probe(us->pusb_dev, i, id);
		up(&intf->driver->serialize);
	}

	/* re-allocate the IRQ URB and submit it to restore connectivity
	 * for CBI devices
	 */
	if (us->protocol == US_PR_CBI) {
		down(&(us->irq_urb_sem));
		us->irq_urb->dev = us->pusb_dev;
		result = usb_submit_urb(us->irq_urb);
		US_DEBUGP("usb_submit_urb() returns %d\n", result);
		up(&(us->irq_urb_sem));
	}
	
	spin_lock_irq(&io_request_lock);

	US_DEBUGP("bus_reset() complete\n");
	return SUCCESS;
}

/* FIXME: This doesn't do anything right now */
static int host_reset( Scsi_Cmnd *srb )
{
	printk(KERN_CRIT "usb-storage: host_reset() requested but not implemented\n" );
	return FAILED;
}

/***********************************************************************
 * /proc/scsi/ functions
 ***********************************************************************/

/* we use this macro to help us write into the buffer */
#undef SPRINTF
#define SPRINTF(args...) \
	do { if (pos < buffer+length) pos += sprintf(pos, ## args); } while (0)

static int proc_info (char *buffer, char **start, off_t offset, int length,
		int hostno, int inout)
{
	struct us_data *us;
	char *pos = buffer;

	/* if someone is sending us data, just throw it away */
	if (inout)
		return length;

	/* lock the data structures */
	down(&us_list_semaphore);

	/* find our data from hostno */
	us = us_list;
	while (us) {
		if (us->host_no == hostno)
			break;
		us = us->next;
	}

	/* release our lock on the data structures */
	up(&us_list_semaphore);

	/* if we couldn't find it, we return an error */
	if (!us) {
		return -ESRCH;
	}

	/* print the controller name */
	SPRINTF("   Host scsi%d: usb-storage\n", hostno);

	/* print product, vendor, and serial number strings */
	SPRINTF("       Vendor: %s\n", us->vendor);
	SPRINTF("      Product: %s\n", us->product);
	SPRINTF("Serial Number: %s\n", us->serial);

	/* show the protocol and transport */
	SPRINTF("     Protocol: %s\n", us->protocol_name);
	SPRINTF("    Transport: %s\n", us->transport_name);

	/* show the GUID of the device */
	SPRINTF("         GUID: " GUID_FORMAT "\n", GUID_ARGS(us->guid));
	SPRINTF("     Attached: %s\n", us->pusb_dev ? "Yes" : "No");

	/*
	 * Calculate start of next buffer, and return value.
	 */
	*start = buffer + offset;

	if ((pos - buffer) < offset)
		return (0);
	else if ((pos - buffer - offset) < length)
		return (pos - buffer - offset);
	else
		return (length);
}

/*
 * this defines our 'host'
 */

Scsi_Host_Template usb_stor_host_template = {
	name:			"usb-storage",
	proc_info:		proc_info,
	info:			host_info,

	detect:			detect,
	release:		release,
	command:		command,
	queuecommand:		queuecommand,

	eh_abort_handler:	command_abort,
	eh_device_reset_handler:device_reset,
	eh_bus_reset_handler:	bus_reset,
	eh_host_reset_handler:	host_reset,

	can_queue:		1,
	this_id:		-1,

	sg_tablesize:		SG_ALL,
	cmd_per_lun:		1,
	present:		0,
	unchecked_isa_dma:	FALSE,
	use_clustering:		TRUE,
	use_new_eh_code:	TRUE,
	emulated:		TRUE
};

unsigned char usb_stor_sense_notready[18] = {
	[0]	= 0x70,			    /* current error */
	[2]	= 0x02,			    /* not ready */
	[5]	= 0x0a,			    /* additional length */
	[10]	= 0x04,			    /* not ready */
	[11]	= 0x03			    /* manual intervention */
};

#define USB_STOR_SCSI_SENSE_HDRSZ 4
#define USB_STOR_SCSI_SENSE_10_HDRSZ 8

struct usb_stor_scsi_sense_hdr
{
  __u8* dataLength;
  __u8* mediumType;
  __u8* devSpecParms;
  __u8* blkDescLength;
};

typedef struct usb_stor_scsi_sense_hdr Usb_Stor_Scsi_Sense_Hdr;

union usb_stor_scsi_sense_hdr_u
{
  Usb_Stor_Scsi_Sense_Hdr hdr;
  __u8* array[USB_STOR_SCSI_SENSE_HDRSZ];
};

typedef union usb_stor_scsi_sense_hdr_u Usb_Stor_Scsi_Sense_Hdr_u;

struct usb_stor_scsi_sense_hdr_10
{
  __u8* dataLengthMSB;
  __u8* dataLengthLSB;
  __u8* mediumType;
  __u8* devSpecParms;
  __u8* reserved1;
  __u8* reserved2;
  __u8* blkDescLengthMSB;
  __u8* blkDescLengthLSB;
};

typedef struct usb_stor_scsi_sense_hdr_10 Usb_Stor_Scsi_Sense_Hdr_10;

union usb_stor_scsi_sense_hdr_10_u
{
  Usb_Stor_Scsi_Sense_Hdr_10 hdr;
  __u8* array[USB_STOR_SCSI_SENSE_10_HDRSZ];
};

typedef union usb_stor_scsi_sense_hdr_10_u Usb_Stor_Scsi_Sense_Hdr_10_u;

void usb_stor_scsiSenseParseBuffer( Scsi_Cmnd* , Usb_Stor_Scsi_Sense_Hdr_u*,
				    Usb_Stor_Scsi_Sense_Hdr_10_u*, int* );

int usb_stor_scsiSense10to6( Scsi_Cmnd* the10 )
{
  __u8 *buffer=0;
  int outputBufferSize = 0;
  int length=0;
  struct scatterlist *sg = 0;
  int i=0, j=0, element=0;
  Usb_Stor_Scsi_Sense_Hdr_u the6Locations;
  Usb_Stor_Scsi_Sense_Hdr_10_u the10Locations;
  int sb=0,si=0,db=0,di=0;
  int sgLength=0;

  US_DEBUGP("-- converting 10 byte sense data to 6 byte\n");
  the10->cmnd[0] = the10->cmnd[0] & 0xBF;

  /* Determine buffer locations */
  usb_stor_scsiSenseParseBuffer( the10, &the6Locations, &the10Locations,
				 &length );

  /* Work out minimum buffer to output */
  outputBufferSize = *the10Locations.hdr.dataLengthLSB;
  outputBufferSize += USB_STOR_SCSI_SENSE_HDRSZ;

  /* Check to see if we need to trucate the output */
  if ( outputBufferSize > length )
    {
      printk( KERN_WARNING USB_STORAGE 
	      "Had to truncate MODE_SENSE_10 buffer into MODE_SENSE.\n" );
      printk( KERN_WARNING USB_STORAGE
	      "outputBufferSize is %d and length is %d.\n",
	      outputBufferSize, length );
    }
  outputBufferSize = length;

  /* Data length */
  if ( *the10Locations.hdr.dataLengthMSB != 0 ) /* MSB must be zero */
    {
      printk( KERN_WARNING USB_STORAGE 
	      "Command will be truncated to fit in SENSE6 buffer.\n" );
      *the6Locations.hdr.dataLength = 0xff;
    }
  else
    {
      *the6Locations.hdr.dataLength = *the10Locations.hdr.dataLengthLSB;
    }

  /* Medium type and DevSpecific parms */
  *the6Locations.hdr.mediumType = *the10Locations.hdr.mediumType;
  *the6Locations.hdr.devSpecParms = *the10Locations.hdr.devSpecParms;

  /* Block descriptor length */
  if ( *the10Locations.hdr.blkDescLengthMSB != 0 ) /* MSB must be zero */
    {
      printk( KERN_WARNING USB_STORAGE 
	      "Command will be truncated to fit in SENSE6 buffer.\n" );
      *the6Locations.hdr.blkDescLength = 0xff;
    }
  else
    {
      *the6Locations.hdr.blkDescLength = *the10Locations.hdr.blkDescLengthLSB;
    }

  if ( the10->use_sg == 0 )
    {
      buffer = the10->request_buffer;
      /* Copy the rest of the data */
      memmove( &(buffer[USB_STOR_SCSI_SENSE_HDRSZ]),
	       &(buffer[USB_STOR_SCSI_SENSE_10_HDRSZ]),
	       outputBufferSize - USB_STOR_SCSI_SENSE_HDRSZ );
      /* initialise last bytes left in buffer due to smaller header */
      memset( &(buffer[outputBufferSize
	    -(USB_STOR_SCSI_SENSE_10_HDRSZ-USB_STOR_SCSI_SENSE_HDRSZ)]),
	      0,
	      USB_STOR_SCSI_SENSE_10_HDRSZ-USB_STOR_SCSI_SENSE_HDRSZ );
    }
  else
    {
      sg = (struct scatterlist *) the10->request_buffer;
      /* scan through this scatterlist and figure out starting positions */
      for ( i=0; i < the10->use_sg; i++)
	{
	  sgLength = sg[i].length;
	  for ( j=0; j<sgLength; j++ )
	    {
	      /* get to end of header */
	      if ( element == USB_STOR_SCSI_SENSE_HDRSZ )
		{
		  db=i;
		  di=j;
		}
	      if ( element == USB_STOR_SCSI_SENSE_10_HDRSZ )
		{
		  sb=i;
		  si=j;
		  /* we've found both sets now, exit loops */
		  j=sgLength;
		  i=the10->use_sg;
		}
	      element++;
	    }
	}

      /* Now we know where to start the copy from */
      element = USB_STOR_SCSI_SENSE_HDRSZ;
      while ( element < outputBufferSize
	      -(USB_STOR_SCSI_SENSE_10_HDRSZ-USB_STOR_SCSI_SENSE_HDRSZ) )
	{
	  /* check limits */
	  if ( sb >= the10->use_sg ||
	       si >= sg[sb].length ||
	       db >= the10->use_sg ||
	       di >= sg[db].length )
	    {
	      printk( KERN_ERR USB_STORAGE
		      "Buffer overrun averted, this shouldn't happen!\n" );
	      break;
	    }

	  /* copy one byte */
	  sg[db].address[di] = sg[sb].address[si];

	  /* get next destination */
	  if ( sg[db].length-1 == di )
	    {
	      db++;
	      di=0;
	    }
	  else
	    {
	      di++;
	    }

	  /* get next source */
	  if ( sg[sb].length-1 == si )
	    {
	      sb++;
	      si=0;
	    }
	  else
	    {
	      si++;
	    }

	  element++;
	}
      /* zero the remaining bytes */
      while ( element < outputBufferSize )
	{
	  /* check limits */
	  if ( db >= the10->use_sg ||
	       di >= sg[db].length )
	    {
	      printk( KERN_ERR USB_STORAGE
		      "Buffer overrun averted, this shouldn't happen!\n" );
	      break;
	    }

	  sg[db].address[di] = 0;

	  /* get next destination */
	  if ( sg[db].length-1 == di )
	    {
	      db++;
	      di=0;
	    }
	  else
	    {
	      di++;
	    }
	  element++;
	}
    }

  /* All done any everything was fine */
  return 0;
}

int usb_stor_scsiSense6to10( Scsi_Cmnd* the6 )
{
  /* will be used to store part of buffer */  
  __u8 tempBuffer[USB_STOR_SCSI_SENSE_10_HDRSZ-USB_STOR_SCSI_SENSE_HDRSZ],
    *buffer=0;
  int outputBufferSize = 0;
  int length=0;
  struct scatterlist *sg = 0;
  int i=0, j=0, element=0;
  Usb_Stor_Scsi_Sense_Hdr_u the6Locations;
  Usb_Stor_Scsi_Sense_Hdr_10_u the10Locations;
  int sb=0,si=0,db=0,di=0;
  int lsb=0,lsi=0,ldb=0,ldi=0;

  US_DEBUGP("-- converting 6 byte sense data to 10 byte\n");
  the6->cmnd[0] = the6->cmnd[0] | 0x40;

  /* Determine buffer locations */
  usb_stor_scsiSenseParseBuffer( the6, &the6Locations, &the10Locations,
				 &length );

  /* Work out minimum buffer to output */
  outputBufferSize = *the6Locations.hdr.dataLength;
  outputBufferSize += USB_STOR_SCSI_SENSE_10_HDRSZ;

  /* Check to see if we need to trucate the output */
  if ( outputBufferSize > length )
    {
      printk( KERN_WARNING USB_STORAGE 
	      "Had to truncate MODE_SENSE into MODE_SENSE_10 buffer.\n" );
      printk( KERN_WARNING USB_STORAGE
	      "outputBufferSize is %d and length is %d.\n",
	      outputBufferSize, length );
    }
  outputBufferSize = length;

  /* Block descriptor length - save these before overwriting */
  tempBuffer[2] = *the10Locations.hdr.blkDescLengthMSB;
  tempBuffer[3] = *the10Locations.hdr.blkDescLengthLSB;
  *the10Locations.hdr.blkDescLengthLSB = *the6Locations.hdr.blkDescLength;
  *the10Locations.hdr.blkDescLengthMSB = 0;

  /* reserved - save these before overwriting */
  tempBuffer[0] = *the10Locations.hdr.reserved1;
  tempBuffer[1] = *the10Locations.hdr.reserved2;
  *the10Locations.hdr.reserved1 = *the10Locations.hdr.reserved2 = 0;

  /* Medium type and DevSpecific parms */
  *the10Locations.hdr.devSpecParms = *the6Locations.hdr.devSpecParms;
  *the10Locations.hdr.mediumType = *the6Locations.hdr.mediumType;

  /* Data length */
  *the10Locations.hdr.dataLengthLSB = *the6Locations.hdr.dataLength;
  *the10Locations.hdr.dataLengthMSB = 0;

  if ( !the6->use_sg )
    {
      buffer = the6->request_buffer;
      /* Copy the rest of the data */
      memmove( &(buffer[USB_STOR_SCSI_SENSE_10_HDRSZ]),
	      &(buffer[USB_STOR_SCSI_SENSE_HDRSZ]),
	      outputBufferSize-USB_STOR_SCSI_SENSE_10_HDRSZ );
      /* Put the first four bytes (after header) in place */
      memcpy( &(buffer[USB_STOR_SCSI_SENSE_10_HDRSZ]),
	      tempBuffer,
	      USB_STOR_SCSI_SENSE_10_HDRSZ-USB_STOR_SCSI_SENSE_HDRSZ );
    }
  else
    {
      sg = (struct scatterlist *) the6->request_buffer;
      /* scan through this scatterlist and figure out ending positions */
      for ( i=0; i < the6->use_sg; i++)
	{
	  for ( j=0; j<sg[i].length; j++ )
	    {
	      /* get to end of header */
	      if ( element == USB_STOR_SCSI_SENSE_HDRSZ )
		{
		  ldb=i;
		  ldi=j;
		}
	      if ( element == USB_STOR_SCSI_SENSE_10_HDRSZ )
		{
		  lsb=i;
		  lsi=j;
		  /* we've found both sets now, exit loops */
		  j=sg[i].length;
		  i=the6->use_sg;
		  break;
		}
	      element++;
	    }
	}
      /* scan through this scatterlist and figure out starting positions */
      element = length-1;
      /* destination is the last element */
      db=the6->use_sg-1;
      di=sg[db].length-1;
      for ( i=the6->use_sg-1; i >= 0; i--)
	{
	  for ( j=sg[i].length-1; j>=0; j-- )
	    {
	      /* get to end of header and find source for copy */
	      if ( element == length - 1
		   - (USB_STOR_SCSI_SENSE_10_HDRSZ-USB_STOR_SCSI_SENSE_HDRSZ) )
		{
		  sb=i;
		  si=j;
		  /* we've found both sets now, exit loops */
		  j=-1;
		  i=-1;
		}
	      element--;
	    }
	}
      /* Now we know where to start the copy from */
      element = length-1
	- (USB_STOR_SCSI_SENSE_10_HDRSZ-USB_STOR_SCSI_SENSE_HDRSZ);
      while ( element >= USB_STOR_SCSI_SENSE_10_HDRSZ )
	{
	  /* check limits */
	  if ( ( sb <= lsb && si < lsi ) ||
	       ( db <= ldb && di < ldi ) )
	    {
	      printk( KERN_ERR USB_STORAGE
		      "Buffer overrun averted, this shouldn't happen!\n" );
	      break;
	    }

	  /* copy one byte */
	  sg[db].address[di] = sg[sb].address[si];

	  /* get next destination */
	  if ( di == 0 )
	    {
	      db--;
	      di=sg[db].length-1;
	    }
	  else
	    {
	      di--;
	    }

	  /* get next source */
	  if ( si == 0 )
	    {
	      sb--;
	      si=sg[sb].length-1;
	    }
	  else
	    {
	      si--;
	    }

	  element--;
	}
      /* copy the remaining four bytes */
      while ( element >= USB_STOR_SCSI_SENSE_HDRSZ )
	{
	  /* check limits */
	  if ( db <= ldb && di < ldi )
	    {
	      printk( KERN_ERR USB_STORAGE
		      "Buffer overrun averted, this shouldn't happen!\n" );
	      break;
	    }

	  sg[db].address[di] = tempBuffer[element-USB_STOR_SCSI_SENSE_HDRSZ];

	  /* get next destination */
	  if ( di == 0 )
	    {
	      db--;
	      di=sg[db].length-1;
	    }
	  else
	    {
	      di--;
	    }
	  element--;
	}
    }

  /* All done and everything was fine */
  return 0;
}

void usb_stor_scsiSenseParseBuffer( Scsi_Cmnd* srb, Usb_Stor_Scsi_Sense_Hdr_u* the6,
			       Usb_Stor_Scsi_Sense_Hdr_10_u* the10,
			       int* length_p )

{
  int i = 0, j=0, element=0;
  struct scatterlist *sg = 0;
  int length = 0;
  __u8* buffer=0;

  /* are we scatter-gathering? */
  if ( srb->use_sg != 0 )
    {
      /* loop over all the scatter gather structures and 
       * get pointer to the data members in the headers
       * (also work out the length while we're here)
       */
      sg = (struct scatterlist *) srb->request_buffer;
      for (i = 0; i < srb->use_sg; i++)
	{
	  length += sg[i].length;
	  /* We only do the inner loop for the headers */
	  if ( element < USB_STOR_SCSI_SENSE_10_HDRSZ )
	    {
	      /* scan through this scatterlist */
	      for ( j=0; j<sg[i].length; j++ )
		{
		  if ( element < USB_STOR_SCSI_SENSE_HDRSZ )
		    {
		      /* fill in the pointers for both header types */
		      the6->array[element] = &(sg[i].address[j]);
		      the10->array[element] = &(sg[i].address[j]);
		    }
		  else if ( element < USB_STOR_SCSI_SENSE_10_HDRSZ )
		    {
		      /* only the longer headers still cares now */
		      the10->array[element] = &(sg[i].address[j]);
		    }
		  /* increase element counter */
		  element++;
		}
	    }
	}
    }
  else
    {
      length = srb->request_bufflen;
      buffer = srb->request_buffer;
      if ( length < USB_STOR_SCSI_SENSE_10_HDRSZ )
	printk( KERN_ERR USB_STORAGE
		"Buffer length smaller than header!!" );
      for( i=0; i<USB_STOR_SCSI_SENSE_10_HDRSZ; i++ )
	{
	  if ( i < USB_STOR_SCSI_SENSE_HDRSZ )
	    {
	      the6->array[i] = &(buffer[i]);
	      the10->array[i] = &(buffer[i]);
	    }
	  else
	    {
	      the10->array[i] = &(buffer[i]);
	    }
	}
    }

  /* Set value of length passed in */
  *length_p = length;
}

