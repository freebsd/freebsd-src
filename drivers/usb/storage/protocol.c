/* Driver for USB Mass Storage compliant devices
 *
 * $Id: protocol.c,v 1.13 2002/02/25 00:34:56 mdharm Exp $
 *
 * Current development and maintenance by:
 *   (c) 1999-2002 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * Developed with the assistance of:
 *   (c) 2000 David L. Brown, Jr. (usb-storage@davidb.org)
 *   (c) 2002 Alan Stern (stern@rowland.org)
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

#include "protocol.h"
#include "usb.h"
#include "debug.h"
#include "scsiglue.h"
#include "transport.h"

/***********************************************************************
 * Helper routines
 ***********************************************************************/
 
static void * find_data_location(Scsi_Cmnd *srb)
{
	if (srb->use_sg) {
		/*
		 * This piece of code only works if the first page is
		 * big enough to hold more than 3 bytes -- which is
		 * _very_ likely.
		 */
		struct scatterlist *sg;

		sg = (struct scatterlist *) srb->request_buffer;
		return (void *) sg[0].address;
	} else
		return (void *) srb->request_buffer;
}

/* Fix-up the return data from an INQUIRY command to show 
 * ANSI SCSI rev 2 so we don't confuse the SCSI layers above us
 */
void fix_inquiry_data(Scsi_Cmnd *srb)
{
	unsigned char *data_ptr;

	/* verify that it's an INQUIRY command */
	if (srb->cmnd[0] != INQUIRY)
		return;

	/* oddly short buffer -- bail out */
	if (srb->request_bufflen < 3)
		return;

	data_ptr = find_data_location(srb);

	/* if it's already 2, bail */
	if ((data_ptr[2] & 7) == 2)
		return;

	US_DEBUGP("Fixing INQUIRY data to show SCSI rev 2 - was %d\n",
		data_ptr[2] & 7);

	/* Change the SCSI revision number */
	data_ptr[2] = (data_ptr[2] & ~7) | 2;
}

/*
 * Fix-up the return data from a READ CAPACITY command. A Feiya reader
 * returns a value that is 1 too large.
 */
static void fix_read_capacity(Scsi_Cmnd *srb)
{
	unsigned char *dp;
	unsigned long capacity;

	/* verify that it's a READ CAPACITY command */
	if (srb->cmnd[0] != READ_CAPACITY)
		return;

	dp = find_data_location(srb);

	capacity = (dp[0]<<24) + (dp[1]<<16) + (dp[2]<<8) + (dp[3]);
	US_DEBUGP("US: Fixing capacity: from %ld to %ld\n",
		capacity+1, capacity);
	capacity--;
	dp[0] = (capacity >> 24);
	dp[1] = (capacity >> 16);
	dp[2] = (capacity >> 8);
	dp[3] = (capacity);
}

/***********************************************************************
 * Protocol routines
 ***********************************************************************/

void usb_stor_qic157_command(Scsi_Cmnd *srb, struct us_data *us)
{
	/* Pad the ATAPI command with zeros 
	 * NOTE: This only works because a Scsi_Cmnd struct field contains
	 * a unsigned char cmnd[12], so we know we have storage available
	 */
	for (; srb->cmd_len<12; srb->cmd_len++)
		srb->cmnd[srb->cmd_len] = 0;

	/* set command length to 12 bytes */
	srb->cmd_len = 12;

	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
	if (srb->result == GOOD << 1) {

		/* fix the INQUIRY data if necessary */
		fix_inquiry_data(srb);
	}
}

void usb_stor_ATAPI_command(Scsi_Cmnd *srb, struct us_data *us)
{
	int old_cmnd = 0;

	/* Fix some commands -- this is a form of mode translation
	 * ATAPI devices only accept 12 byte long commands 
	 *
	 * NOTE: This only works because a Scsi_Cmnd struct field contains
	 * a unsigned char cmnd[12], so we know we have storage available
	 */

	/* Pad the ATAPI command with zeros */
	for (; srb->cmd_len<12; srb->cmd_len++)
		srb->cmnd[srb->cmd_len] = 0;

	/* set command length to 12 bytes */
	srb->cmd_len = 12;

	/* determine the correct (or minimum) data length for these commands */
	switch (srb->cmnd[0]) {

		/* change MODE_SENSE/MODE_SELECT from 6 to 10 byte commands */
	case MODE_SENSE:
	case MODE_SELECT:
		/* save the command so we can tell what it was */
		old_cmnd = srb->cmnd[0];

		srb->cmnd[11] = 0;
		srb->cmnd[10] = 0;
		srb->cmnd[9] = 0;
		srb->cmnd[8] = srb->cmnd[4];
		srb->cmnd[7] = 0;
		srb->cmnd[6] = 0;
		srb->cmnd[5] = 0;
		srb->cmnd[4] = 0;
		srb->cmnd[3] = 0;
		srb->cmnd[2] = srb->cmnd[2];
		srb->cmnd[1] = srb->cmnd[1];
		srb->cmnd[0] = srb->cmnd[0] | 0x40;
		break;

		/* change READ_6/WRITE_6 to READ_10/WRITE_10, which 
		 * are ATAPI commands */
	case WRITE_6:
	case READ_6:
		srb->cmnd[11] = 0;
		srb->cmnd[10] = 0;
		srb->cmnd[9] = 0;
		srb->cmnd[8] = srb->cmnd[4];
		srb->cmnd[7] = 0;
		srb->cmnd[6] = 0;
		srb->cmnd[5] = srb->cmnd[3];
		srb->cmnd[4] = srb->cmnd[2];
		srb->cmnd[3] = srb->cmnd[1] & 0x1F;
		srb->cmnd[2] = 0;
		srb->cmnd[1] = srb->cmnd[1] & 0xE0;
		srb->cmnd[0] = srb->cmnd[0] | 0x20;
		break;
	} /* end switch on cmnd[0] */

	/* convert MODE_SELECT data here */
	if (old_cmnd == MODE_SELECT)
		usb_stor_scsiSense6to10(srb);

	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
	if (srb->result == GOOD << 1) {

		/* Fix the MODE_SENSE data if we translated the command */
		if (old_cmnd == MODE_SENSE)
			usb_stor_scsiSense10to6(srb);

		/* fix the INQUIRY data if necessary */
		fix_inquiry_data(srb);
	}
}


void usb_stor_ufi_command(Scsi_Cmnd *srb, struct us_data *us)
{
	int old_cmnd = 0;

	/* fix some commands -- this is a form of mode translation
	 * UFI devices only accept 12 byte long commands 
	 *
	 * NOTE: This only works because a Scsi_Cmnd struct field contains
	 * a unsigned char cmnd[12], so we know we have storage available
	 */

	/* Pad the ATAPI command with zeros */
	for (; srb->cmd_len<12; srb->cmd_len++)
		srb->cmnd[srb->cmd_len] = 0;

	/* set command length to 12 bytes (this affects the transport layer) */
	srb->cmd_len = 12;

	/* determine the correct (or minimum) data length for these commands */
	switch (srb->cmnd[0]) {

		/* for INQUIRY, UFI devices only ever return 36 bytes */
	case INQUIRY:
		srb->cmnd[4] = 36;
		break;

		/* change MODE_SENSE/MODE_SELECT from 6 to 10 byte commands */
	case MODE_SENSE:
	case MODE_SELECT:
		/* save the command so we can tell what it was */
		old_cmnd = srb->cmnd[0];

		srb->cmnd[11] = 0;
		srb->cmnd[10] = 0;
		srb->cmnd[9] = 0;

		/* if we're sending data, we send all.	If getting data, 
		 * get the minimum */
		if (srb->cmnd[0] == MODE_SELECT)
			srb->cmnd[8] = srb->cmnd[4];
		else
			srb->cmnd[8] = 8;

		srb->cmnd[7] = 0;
		srb->cmnd[6] = 0;
		srb->cmnd[5] = 0;
		srb->cmnd[4] = 0;
		srb->cmnd[3] = 0;
		srb->cmnd[2] = srb->cmnd[2];
		srb->cmnd[1] = srb->cmnd[1];
		srb->cmnd[0] = srb->cmnd[0] | 0x40;
		break;

		/* again, for MODE_SENSE_10, we get the minimum (8) */
	case MODE_SENSE_10:
		srb->cmnd[7] = 0;
		srb->cmnd[8] = 8;
		break;

		/* for REQUEST_SENSE, UFI devices only ever return 18 bytes */
	case REQUEST_SENSE:
		srb->cmnd[4] = 18;
		break;

		/* change READ_6/WRITE_6 to READ_10/WRITE_10, which 
		 * are UFI commands */
	case WRITE_6:
	case READ_6:
		srb->cmnd[11] = 0;
		srb->cmnd[10] = 0;
		srb->cmnd[9] = 0;
		srb->cmnd[8] = srb->cmnd[4];
		srb->cmnd[7] = 0;
		srb->cmnd[6] = 0;
		srb->cmnd[5] = srb->cmnd[3];
		srb->cmnd[4] = srb->cmnd[2];
		srb->cmnd[3] = srb->cmnd[1] & 0x1F;
		srb->cmnd[2] = 0;
		srb->cmnd[1] = srb->cmnd[1] & 0xE0;
		srb->cmnd[0] = srb->cmnd[0] | 0x20;
		break;
	} /* end switch on cmnd[0] */

	/* convert MODE_SELECT data here */
	if (old_cmnd == MODE_SELECT)
		usb_stor_scsiSense6to10(srb);

	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
	if (srb->result == GOOD << 1) {

		/* Fix the MODE_SENSE data if we translated the command */
		if (old_cmnd == MODE_SENSE)
			usb_stor_scsiSense10to6(srb);

		/* Fix the data for an INQUIRY, if necessary */
		fix_inquiry_data(srb);
	}
}

void usb_stor_transparent_scsi_command(Scsi_Cmnd *srb, struct us_data *us)
{
	int old_cmnd = 0;

	/* This code supports devices which do not support {READ|WRITE}_6
	 * Apparently, neither Windows or MacOS will use these commands,
	 * so some devices do not support them
	 */
	if (us->flags & US_FL_MODE_XLATE) {
		US_DEBUGP("Invoking Mode Translation\n");
		/* save the old command for later */
		old_cmnd = srb->cmnd[0];

		switch (srb->cmnd[0]) {
		/* change READ_6/WRITE_6 to READ_10/WRITE_10 */
		case WRITE_6:
		case READ_6:
			srb->cmd_len = 12;
			srb->cmnd[11] = 0;
			srb->cmnd[10] = 0;
			srb->cmnd[9] = 0;
			srb->cmnd[8] = srb->cmnd[4];
			srb->cmnd[7] = 0;
			srb->cmnd[6] = 0;
			srb->cmnd[5] = srb->cmnd[3];
			srb->cmnd[4] = srb->cmnd[2];
			srb->cmnd[3] = srb->cmnd[1] & 0x1F;
			srb->cmnd[2] = 0;
			srb->cmnd[1] = srb->cmnd[1] & 0xE0;
			srb->cmnd[0] = srb->cmnd[0] | 0x20;
			break;

		/* convert MODE_SELECT data here */
		case MODE_SENSE:
		case MODE_SELECT:
			srb->cmd_len = 12;
			srb->cmnd[11] = 0;
			srb->cmnd[10] = 0;
			srb->cmnd[9] = 0;
			srb->cmnd[8] = srb->cmnd[4];
			srb->cmnd[7] = 0;
			srb->cmnd[6] = 0;
			srb->cmnd[5] = 0;
			srb->cmnd[4] = 0;
			srb->cmnd[3] = 0;
			srb->cmnd[2] = srb->cmnd[2];
			srb->cmnd[1] = srb->cmnd[1];
			srb->cmnd[0] = srb->cmnd[0] | 0x40;
			break;
		} /* switch (srb->cmnd[0]) */
	} /* if (us->flags & US_FL_MODE_XLATE) */

	/* convert MODE_SELECT data here */
	if ((us->flags & US_FL_MODE_XLATE) && (old_cmnd == MODE_SELECT))
		usb_stor_scsiSense6to10(srb);

	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
	if (srb->result == GOOD << 1) {

		/* Fix the MODE_SENSE data if we translated the command */
		if ((us->flags & US_FL_MODE_XLATE) && (old_cmnd == MODE_SENSE))
			usb_stor_scsiSense10to6(srb);

		/* fix the INQUIRY data if necessary */
		fix_inquiry_data(srb);

		/* Fix the READ CAPACITY result if necessary */
		if (us->flags & US_FL_FIX_CAPACITY)
			fix_read_capacity(srb);
	}
}

