/* Driver for USB Mass Storage compliant devices
 *
 * $Id: transport.c,v 1.44 2002/02/25 00:43:41 mdharm Exp $
 *
 * Current development and maintenance by:
 *   (c) 1999-2002 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * Developed with the assistance of:
 *   (c) 2000 David L. Brown, Jr. (usb-storage@davidb.org)
 *   (c) 2000 Stephen J. Gowdy (SGowdy@lbl.gov)
 *   (c) 2002 Alan Stern <stern@rowland.org>
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

#include <linux/config.h>
#include "transport.h"
#include "protocol.h"
#include "usb.h"
#include "debug.h"

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>

/***********************************************************************
 * Helper routines
 ***********************************************************************/

/* Calculate the length of the data transfer (not the command) for any
 * given SCSI command
 */
unsigned int usb_stor_transfer_length(Scsi_Cmnd *srb)
{
      	int i;
	int doDefault = 0;
	unsigned int len = 0;
	unsigned int total = 0;
	struct scatterlist *sg;

	/* This table tells us:
	   X = command not supported
	   L = return length in cmnd[4] (8 bits).
	   M = return length in cmnd[8] (8 bits).
	   G = return length in cmnd[3] and cmnd[4] (16 bits)
	   H = return length in cmnd[7] and cmnd[8] (16 bits)
	   I = return length in cmnd[8] and cmnd[9] (16 bits)
	   C = return length in cmnd[2] to cmnd[5] (32 bits)
	   D = return length in cmnd[6] to cmnd[9] (32 bits)
	   B = return length in blocksize so we use buff_len
	   R = return length in cmnd[2] to cmnd[4] (24 bits)
	   S = return length in cmnd[3] to cmnd[5] (24 bits)
	   T = return length in cmnd[6] to cmnd[8] (24 bits)
	   U = return length in cmnd[7] to cmnd[9] (24 bits)
	   0-9 = fixed return length
	   V = 20 bytes
	   W = 24 bytes
	   Z = return length is mode dependant or not in command, use buff_len
	*/

	static char *lengths =

	      /* 0123456789ABCDEF   0123456789ABCDEF */

		"00XLZ6XZBXBBXXXB" "00LBBLG0R0L0GG0X"  /* 00-1F */
		"XXXXT8XXB4B0BBBB" "ZZZ0B00HCSSZTBHH"  /* 20-3F */
		"M0HHB0X000H0HH0X" "XHH0HHXX0TH0H0XX"  /* 40-5F */
		"XXXXXXXXXXXXXXXX" "XXXXXXXXXXXXXXXX"  /* 60-7F */
		"XXXXXXXXXXXXXXXX" "XXXXXXXXXXXXXXXX"  /* 80-9F */
		"X0XXX00XB0BXBXBB" "ZZZ0XUIDU000XHBX"  /* A0-BF */
		"XXXXXXXXXXXXXXXX" "XXXXXXXXXXXXXXXX"  /* C0-DF */
		"XDXXXXXXXXXXXXXX" "XXW00HXXXXXXXXXX"; /* E0-FF */

	/* Commands checked in table:

	   CHANGE_DEFINITION 40
	   COMPARE 39
	   COPY 18
	   COPY_AND_VERIFY 3a
	   ERASE 19
	   ERASE_10 2c
	   ERASE_12 ac
	   EXCHANGE_MEDIUM a6
	   FORMAT_UNIT 04
	   GET_DATA_BUFFER_STATUS 34
	   GET_MESSAGE_10 28
	   GET_MESSAGE_12 a8
	   GET_WINDOW 25   !!! Has more data than READ_CAPACITY, need to fix table
	   INITIALIZE_ELEMENT_STATUS 07 !!! REASSIGN_BLOCKS luckily uses buff_len
	   INQUIRY 12
	   LOAD_UNLOAD 1b
	   LOCATE 2b
	   LOCK_UNLOCK_CACHE 36
	   LOG_SELECT 4c
	   LOG_SENSE 4d
	   MEDIUM_SCAN 38     !!! This was M
	   MODE_SELECT6 15
	   MODE_SELECT_10 55
	   MODE_SENSE_6 1a
	   MODE_SENSE_10 5a
	   MOVE_MEDIUM a5
	   OBJECT_POSITION 31  !!! Same as SEARCH_DATA_EQUAL
	   PAUSE_RESUME 4b
	   PLAY_AUDIO_10 45
	   PLAY_AUDIO_12 a5
	   PLAY_AUDIO_MSF 47
	   PLAY_AUDIO_TRACK_INDEX 48
   	   PLAY_AUDIO_TRACK_RELATIVE_10 49
	   PLAY_AUDIO_TRACK_RELATIVE_12 a9
	   POSITION_TO_ELEMENT 2b
      	   PRE-FETCH 34
	   PREVENT_ALLOW_MEDIUM_REMOVAL 1e
	   PRINT 0a             !!! Same as WRITE_6 but is always in bytes
	   READ_6 08
	   READ_10 28
	   READ_12 a8
	   READ_BLOCK_LIMITS 05
	   READ_BUFFER 3c
	   READ_CAPACITY 25
	   READ_CDROM_CAPACITY 25
	   READ_DEFECT_DATA 37
	   READ_DEFECT_DATA_12 b7
	   READ_ELEMENT_STATUS b8 !!! Think this is in bytes
	   READ_GENERATION 29 !!! Could also be M?
	   READ_HEADER 44     !!! This was L
	   READ_LONG 3e
	   READ_POSITION 34   !!! This should be V but conflicts with PRE-FETCH
	   READ_REVERSE 0f
	   READ_SUB-CHANNEL 42 !!! Is this in bytes?
	   READ_TOC 43         !!! Is this in bytes?
	   READ_UPDATED_BLOCK 2d
	   REASSIGN_BLOCKS 07
	   RECEIVE 08        !!! Same as READ_6 probably in bytes though
	   RECEIVE_DIAGNOSTIC_RESULTS 1c
	   RECOVER_BUFFERED_DATA 14 !!! For PRINTERs this is bytes
	   RELEASE_UNIT 17
	   REQUEST_SENSE 03
	   REQUEST_VOLUME_ELEMENT_ADDRESS b5 !!! Think this is in bytes
	   RESERVE_UNIT 16
	   REWIND 01
	   REZERO_UNIT 01
	   SCAN 1b          !!! Conflicts with various commands, should be L
	   SEARCH_DATA_EQUAL 31
	   SEARCH_DATA_EQUAL_12 b1
	   SEARCH_DATA_LOW 30
	   SEARCH_DATA_LOW_12 b0
	   SEARCH_DATA_HIGH 32
	   SEARCH_DATA_HIGH_12 b2
	   SEEK_6 0b         !!! Conflicts with SLEW_AND_PRINT
	   SEEK_10 2b
	   SEND 0a           !!! Same as WRITE_6, probably in bytes though
	   SEND 2a           !!! Similar to WRITE_10 but for scanners
	   SEND_DIAGNOSTIC 1d
	   SEND_MESSAGE_6 0a   !!! Same as WRITE_6 - is in bytes
	   SEND_MESSAGE_10 2a  !!! Same as WRITE_10 - is in bytes
	   SEND_MESSAGE_12 aa  !!! Same as WRITE_12 - is in bytes
	   SEND_OPC 54
	   SEND_VOLUME_TAG b6 !!! Think this is in bytes
	   SET_LIMITS 33
	   SET_LIMITS_12 b3
	   SET_WINDOW 24
	   SLEW_AND_PRINT 0b !!! Conflicts with SEEK_6
	   SPACE 11
	   START_STOP_UNIT 1b
	   STOP_PRINT 1b
	   SYNCHRONIZE_BUFFER 10
	   SYNCHRONIZE_CACHE 35
	   TEST_UNIT_READY 00
	   UPDATE_BLOCK 3d
	   VERIFY 13
	   VERIFY 2f
	   VERIFY_12 af
	   WRITE_6 0a
	   WRITE_10 2a
	   WRITE_12 aa
	   WRITE_AND_VERIFY 2e
	   WRITE_AND_VERIFY_12 ae
	   WRITE_BUFFER 3b
	   WRITE_FILEMARKS 10
	   WRITE_LONG 3f
	   WRITE_SAME 41
	*/

	if (srb->sc_data_direction == SCSI_DATA_WRITE) {
		doDefault = 1;
	}
	else
		switch (lengths[srb->cmnd[0]]) {
			case 'L':
				len = srb->cmnd[4];
				break;

			case 'M':
				len = srb->cmnd[8];
				break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				len = lengths[srb->cmnd[0]]-'0';
				break;

			case 'G':
				len = (((unsigned int)srb->cmnd[3])<<8) |
					srb->cmnd[4];
				break;

			case 'H':
				len = (((unsigned int)srb->cmnd[7])<<8) |
					srb->cmnd[8];
				break;

			case 'I':
				len = (((unsigned int)srb->cmnd[8])<<8) |
					srb->cmnd[9];
				break;

			case 'R':
				len = (((unsigned int)srb->cmnd[2])<<16) |
					(((unsigned int)srb->cmnd[3])<<8) |
					srb->cmnd[4];
				break;

			case 'S':
				len = (((unsigned int)srb->cmnd[3])<<16) |
					(((unsigned int)srb->cmnd[4])<<8) |
					srb->cmnd[5];
				break;

			case 'T':
				len = (((unsigned int)srb->cmnd[6])<<16) |
					(((unsigned int)srb->cmnd[7])<<8) |
					srb->cmnd[8];
				break;

			case 'U':
				len = (((unsigned int)srb->cmnd[7])<<16) |
					(((unsigned int)srb->cmnd[8])<<8) |
					srb->cmnd[9];
				break;

			case 'C':
				len = (((unsigned int)srb->cmnd[2])<<24) |
					(((unsigned int)srb->cmnd[3])<<16) |
					(((unsigned int)srb->cmnd[4])<<8) |
					srb->cmnd[5];
				break;

			case 'D':
				len = (((unsigned int)srb->cmnd[6])<<24) |
					(((unsigned int)srb->cmnd[7])<<16) |
					(((unsigned int)srb->cmnd[8])<<8) |
					srb->cmnd[9];
				break;

			case 'V':
				len = 20;
				break;

			case 'W':
				len = 24;
				break;

			case 'B':
				/* Use buffer size due to different block sizes */
				doDefault = 1;
				break;

			case 'X':
				US_DEBUGP("Error: UNSUPPORTED COMMAND %02X\n",
						srb->cmnd[0]);
				doDefault = 1;
				break;

			case 'Z':
				/* Use buffer size due to mode dependence */
				doDefault = 1;
				break;

			default:
				US_DEBUGP("Error: COMMAND %02X out of range or table inconsistent (%c).\n",
						srb->cmnd[0], lengths[srb->cmnd[0]] );
				doDefault = 1;
		}
	   
	   if ( doDefault == 1 ) {
		   /* Are we going to scatter gather? */
		   if (srb->use_sg) {
			   /* Add up the sizes of all the sg segments */
			   sg = (struct scatterlist *) srb->request_buffer;
			   for (i = 0; i < srb->use_sg; i++)
				   total += sg[i].length;
			   len = total;
		   }
		   else
			   /* Just return the length of the buffer */
			   len = srb->request_bufflen;
	   }

	return len;
}

/***********************************************************************
 * Data transfer routines
 ***********************************************************************/

/* This is the completion handler which will wake us up when an URB
 * completes.
 */
static void usb_stor_blocking_completion(struct urb *urb)
{
	struct completion *urb_done_ptr = (struct completion *)urb->context;

	complete(urb_done_ptr);
}

/* This is our function to emulate usb_control_msg() but give us enough
 * access to make aborts/resets work
 */
int usb_stor_control_msg(struct us_data *us, unsigned int pipe,
			 u8 request, u8 requesttype, u16 value, u16 index, 
			 void *data, u16 size)
{
	int status;
	struct usb_ctrlrequest *dr;

	/* allocate the device request structure */
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	if (!dr)
		return -ENOMEM;

	/* fill in the structure */
	dr->bRequestType = requesttype;
	dr->bRequest = request;
	dr->wValue = cpu_to_le16(value);
	dr->wIndex = cpu_to_le16(index);
	dr->wLength = cpu_to_le16(size);

	/* set up data structures for the wakeup system */
	init_completion(&us->current_done);

	/* lock the URB */
	down(&(us->current_urb_sem));

	/* fill the URB */
	FILL_CONTROL_URB(us->current_urb, us->pusb_dev, pipe, 
			 (unsigned char*) dr, data, size, 
			 usb_stor_blocking_completion, &us->current_done);
	us->current_urb->actual_length = 0;
	us->current_urb->error_count = 0;
	us->current_urb->transfer_flags = USB_ASYNC_UNLINK;
	us->current_urb->status = 0;

	/* submit the URB */
	status = usb_submit_urb(us->current_urb);
	if (status) {
		/* something went wrong */
		up(&(us->current_urb_sem));
		kfree(dr);
		return status;
	}

	/* wait for the completion of the URB */
	up(&(us->current_urb_sem));
	wait_for_completion(&us->current_done);
	down(&(us->current_urb_sem));

	/* return the actual length of the data transferred if no error*/
	status = us->current_urb->status;
	if (status == -ENOENT)
		status = -ECONNRESET;
	if (status >= 0)
		status = us->current_urb->actual_length;

	/* release the lock and return status */
	up(&(us->current_urb_sem));
	kfree(dr);
  	return status;
}

/* This is our function to emulate usb_bulk_msg() but give us enough
 * access to make aborts/resets work
 */
int usb_stor_bulk_msg(struct us_data *us, void *data, int pipe,
		      unsigned int len, unsigned int *act_len)
{
	int status;

	/* set up data structures for the wakeup system */
	init_completion(&us->current_done);

	/* lock the URB */
	down(&(us->current_urb_sem));

	/* fill the URB */
	FILL_BULK_URB(us->current_urb, us->pusb_dev, pipe, data, len,
		      usb_stor_blocking_completion, &us->current_done);
	us->current_urb->actual_length = 0;
	us->current_urb->error_count = 0;
	us->current_urb->transfer_flags = USB_ASYNC_UNLINK;
	us->current_urb->status = 0;

	/* submit the URB */
	status = usb_submit_urb(us->current_urb);
	if (status) {
		/* something went wrong */
		up(&(us->current_urb_sem));
		return status;
	}

	/* wait for the completion of the URB */
	up(&(us->current_urb_sem));
	wait_for_completion(&us->current_done);
	down(&(us->current_urb_sem));
	if (us->current_urb->status == -ENOENT)
		us->current_urb->status = -ECONNRESET;

	/* return the actual length of the data transferred */
	*act_len = us->current_urb->actual_length;

	/* release the lock and return status */
	up(&(us->current_urb_sem));
	return us->current_urb->status;
}

/* This is a version of usb_clear_halt() that doesn't read the status from
 * the device -- this is because some devices crash their internal firmware
 * when the status is requested after a halt
 */
int usb_stor_clear_halt(struct us_data *us, int pipe)
{
	int result;
	int endp = usb_pipeendpoint(pipe) | (usb_pipein(pipe) << 7);

	result = usb_stor_control_msg(us,
		usb_sndctrlpipe(us->pusb_dev, 0),
		USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT, 0,
		endp, NULL, 0);		/* note: no 3*HZ timeout */
	US_DEBUGP("usb_stor_clear_halt: result=%d\n", result);

	/* this is a failure case */
	if (result < 0)
		return result;

	/* reset the toggles and endpoint flags */
	usb_endpoint_running(us->pusb_dev, usb_pipeendpoint(pipe),
		usb_pipeout(pipe));
	usb_settoggle(us->pusb_dev, usb_pipeendpoint(pipe),
		usb_pipeout(pipe), 0);

	return 0;
}

/*
 * Transfer one SCSI scatter-gather buffer via bulk transfer
 *
 * Note that this function is necessary because we want the ability to
 * use scatter-gather memory.  Good performance is achieved by a combination
 * of scatter-gather and clustering (which makes each chunk bigger).
 *
 * Note that the lower layer will always retry when a NAK occurs, up to the
 * timeout limit.  Thus we don't have to worry about it for individual
 * packets.
 */
int usb_stor_transfer_partial(struct us_data *us, char *buf, int length)
{
	int result;
	int partial;
	int pipe;

	/* calculate the appropriate pipe information */
	if (us->srb->sc_data_direction == SCSI_DATA_READ)
		pipe = usb_rcvbulkpipe(us->pusb_dev, us->ep_in);
	else
		pipe = usb_sndbulkpipe(us->pusb_dev, us->ep_out);

	/* transfer the data */
	US_DEBUGP("usb_stor_transfer_partial(): xfer %d bytes\n", length);
	result = usb_stor_bulk_msg(us, buf, pipe, length, &partial);
	US_DEBUGP("usb_stor_bulk_msg() returned %d xferred %d/%d\n",
		  result, partial, length);

	/* if we stall, we need to clear it before we go on */
	if (result == -EPIPE) {
		US_DEBUGP("clearing endpoint halt for pipe 0x%x\n", pipe);
		usb_stor_clear_halt(us, pipe);
	}

	/* did we abort this command? */
	if (result == -ECONNRESET) {
		US_DEBUGP("usb_stor_transfer_partial(): transfer aborted\n");
		return US_BULK_TRANSFER_ABORTED;
	}

	/* did we send all the data? */
	if (partial == length) {
		US_DEBUGP("usb_stor_transfer_partial(): transfer complete\n");
		return US_BULK_TRANSFER_GOOD;
	}

	/* NAK - that means we've retried a few times already */
	if (result == -ETIMEDOUT) {
		US_DEBUGP("usb_stor_transfer_partial(): device NAKed\n");
		return US_BULK_TRANSFER_FAILED;
	}

	/* the catch-all error case */
	if (result) {
		US_DEBUGP("usb_stor_transfer_partial(): unknown error\n");
		return US_BULK_TRANSFER_FAILED;
	}

	/* no error code, so we must have transferred some data, 
	 * just not all of it */
	return US_BULK_TRANSFER_SHORT;
}

/*
 * Transfer an entire SCSI command's worth of data payload over the bulk
 * pipe.
 *
 * Note that this uses usb_stor_transfer_partial to achieve its goals -- this
 * function simply determines if we're going to use scatter-gather or not,
 * and acts appropriately.  For now, it also re-interprets the error codes.
 */
void usb_stor_transfer(Scsi_Cmnd *srb, struct us_data* us)
{
	int i;
	int result = -1;
	struct scatterlist *sg;
	unsigned int total_transferred = 0;
	unsigned int transfer_amount;

	/* calculate how much we want to transfer */
	transfer_amount = usb_stor_transfer_length(srb);

	/* was someone foolish enough to request more data than available
	 * buffer space? */
	if (transfer_amount > srb->request_bufflen)
		transfer_amount = srb->request_bufflen;

	/* are we scatter-gathering? */
	if (srb->use_sg) {

		/* loop over all the scatter gather structures and 
		 * make the appropriate requests for each, until done
		 */
		sg = (struct scatterlist *) srb->request_buffer;
		for (i = 0; i < srb->use_sg; i++) {

			/* transfer the lesser of the next buffer or the
			 * remaining data */
			if (transfer_amount - total_transferred >= 
					sg[i].length) {
				result = usb_stor_transfer_partial(us,
						sg[i].address, sg[i].length);
				total_transferred += sg[i].length;
			} else
				result = usb_stor_transfer_partial(us,
						sg[i].address,
						transfer_amount - total_transferred);

			/* if we get an error, end the loop here */
			if (result)
				break;
		}
	}
	else
		/* no scatter-gather, just make the request */
		result = usb_stor_transfer_partial(us, srb->request_buffer, 
					     transfer_amount);

	/* return the result in the data structure itself */
	srb->result = result;
}

/***********************************************************************
 * Transport routines
 ***********************************************************************/

/* Invoke the transport and basic error-handling/recovery methods
 *
 * This is used by the protocol layers to actually send the message to
 * the device and receive the response.
 */
void usb_stor_invoke_transport(Scsi_Cmnd *srb, struct us_data *us)
{
	int need_auto_sense;
	int result;

	/* send the command to the transport layer */
	result = us->transport(srb, us);

	/* if the command gets aborted by the higher layers, we need to
	 * short-circuit all other processing
	 */
	if (result == USB_STOR_TRANSPORT_ABORTED) {
		US_DEBUGP("-- transport indicates command was aborted\n");
		srb->result = DID_ABORT << 16;

		/* Bulk-only aborts require a device reset */
		if (us->protocol == US_PR_BULK)
			us->transport_reset(us);
		return;
	}

	/* if there is a transport error, reset and don't auto-sense */
	if (result == USB_STOR_TRANSPORT_ERROR) {
		US_DEBUGP("-- transport indicates error, resetting\n");
		us->transport_reset(us);
		srb->result = DID_ERROR << 16;
		return;
	}

	/* Determine if we need to auto-sense
	 *
	 * I normally don't use a flag like this, but it's almost impossible
	 * to understand what's going on here if I don't.
	 */
	need_auto_sense = 0;

	/*
	 * If we're running the CB transport, which is incapable
	 * of determining status on it's own, we need to auto-sense almost
	 * every time.
	 */
	if (us->protocol == US_PR_CB || us->protocol == US_PR_DPCM_USB) {
		US_DEBUGP("-- CB transport device requiring auto-sense\n");
		need_auto_sense = 1;

		/* There are some exceptions to this.  Notably, if this is
		 * a UFI device and the command is REQUEST_SENSE or INQUIRY,
		 * then it is impossible to truly determine status.
		 */
		if (us->subclass == US_SC_UFI &&
		    ((srb->cmnd[0] == REQUEST_SENSE) ||
		     (srb->cmnd[0] == INQUIRY))) {
			US_DEBUGP("** no auto-sense for a special command\n");
			need_auto_sense = 0;
		}
	}

	/*
	 * If we have a failure, we're going to do a REQUEST_SENSE 
	 * automatically.  Note that we differentiate between a command
	 * "failure" and an "error" in the transport mechanism.
	 */
	if (result == USB_STOR_TRANSPORT_FAILED) {
		US_DEBUGP("-- transport indicates command failure\n");
		need_auto_sense = 1;
	}

	/*
	 * Also, if we have a short transfer on a command that can't have
	 * a short transfer, we're going to do this.
	 */
	if ((srb->result == US_BULK_TRANSFER_SHORT) &&
	    !((srb->cmnd[0] == REQUEST_SENSE) ||
	      (srb->cmnd[0] == INQUIRY) ||
	      (srb->cmnd[0] == MODE_SENSE) ||
	      (srb->cmnd[0] == LOG_SENSE) ||
	      (srb->cmnd[0] == MODE_SENSE_10))) {
		US_DEBUGP("-- unexpectedly short transfer\n");
		need_auto_sense = 1;
	}

	/* Now, if we need to do the auto-sense, let's do it */
	if (need_auto_sense) {
		int temp_result;
		void* old_request_buffer;
		unsigned short old_sg;
		unsigned old_request_bufflen;
		unsigned char old_sc_data_direction;
		unsigned char old_cmd_len;
		unsigned char old_cmnd[MAX_COMMAND_SIZE];

		US_DEBUGP("Issuing auto-REQUEST_SENSE\n");

		/* save the old command */
		memcpy(old_cmnd, srb->cmnd, MAX_COMMAND_SIZE);
		old_cmd_len = srb->cmd_len;

		/* set the command and the LUN */
		memset(srb->cmnd, 0, MAX_COMMAND_SIZE);
		srb->cmnd[0] = REQUEST_SENSE;
		srb->cmnd[1] = old_cmnd[1] & 0xE0;
		srb->cmnd[4] = 18;

		/* FIXME: we must do the protocol translation here */
		if (us->subclass == US_SC_RBC || us->subclass == US_SC_SCSI)
			srb->cmd_len = 6;
		else
			srb->cmd_len = 12;

		/* set the transfer direction */
		old_sc_data_direction = srb->sc_data_direction;
		srb->sc_data_direction = SCSI_DATA_READ;

		/* use the new buffer we have */
		old_request_buffer = srb->request_buffer;
		srb->request_buffer = srb->sense_buffer;

		/* set the buffer length for transfer */
		old_request_bufflen = srb->request_bufflen;
		srb->request_bufflen = 18;

		/* set up for no scatter-gather use */
		old_sg = srb->use_sg;
		srb->use_sg = 0;

		/* issue the auto-sense command */
		temp_result = us->transport(us->srb, us);

		/* let's clean up right away */
		srb->request_buffer = old_request_buffer;
		srb->request_bufflen = old_request_bufflen;
		srb->use_sg = old_sg;
		srb->sc_data_direction = old_sc_data_direction;
		srb->cmd_len = old_cmd_len;
		memcpy(srb->cmnd, old_cmnd, MAX_COMMAND_SIZE);

		if (temp_result == USB_STOR_TRANSPORT_ABORTED) {
			US_DEBUGP("-- auto-sense aborted\n");
			srb->result = DID_ABORT << 16;
			return;
		}
		if (temp_result != USB_STOR_TRANSPORT_GOOD) {
			US_DEBUGP("-- auto-sense failure\n");

			/* we skip the reset if this happens to be a
			 * multi-target device, since failure of an
			 * auto-sense is perfectly valid
			 */
			if (!(us->flags & US_FL_SCM_MULT_TARG)) {
				us->transport_reset(us);
			}
			srb->result = DID_ERROR << 16;
			return;
		}

		US_DEBUGP("-- Result from auto-sense is %d\n", temp_result);
		US_DEBUGP("-- code: 0x%x, key: 0x%x, ASC: 0x%x, ASCQ: 0x%x\n",
			  srb->sense_buffer[0],
			  srb->sense_buffer[2] & 0xf,
			  srb->sense_buffer[12], 
			  srb->sense_buffer[13]);
#ifdef CONFIG_USB_STORAGE_DEBUG
		usb_stor_show_sense(
			  srb->sense_buffer[2] & 0xf,
			  srb->sense_buffer[12], 
			  srb->sense_buffer[13]);
#endif

		/* set the result so the higher layers expect this data */
		srb->result = CHECK_CONDITION << 1;

		/* If things are really okay, then let's show that */
		if ((srb->sense_buffer[2] & 0xf) == 0x0)
			srb->result = GOOD << 1;
	} else /* if (need_auto_sense) */
		srb->result = GOOD << 1;

	/* Regardless of auto-sense, if we _know_ we have an error
	 * condition, show that in the result code
	 */
	if (result == USB_STOR_TRANSPORT_FAILED)
		srb->result = CHECK_CONDITION << 1;

	/* If we think we're good, then make sure the sense data shows it.
	 * This is necessary because the auto-sense for some devices always
	 * sets byte 0 == 0x70, even if there is no error
	 */
	if ((us->protocol == US_PR_CB || us->protocol == US_PR_DPCM_USB) && 
	    (result == USB_STOR_TRANSPORT_GOOD) &&
	    ((srb->sense_buffer[2] & 0xf) == 0x0))
		srb->sense_buffer[0] = 0x0;
}

/*
 * Control/Bulk/Interrupt transport
 */

/* The interrupt handler for CBI devices */
void usb_stor_CBI_irq(struct urb *urb)
{
	struct us_data *us = (struct us_data *)urb->context;

	US_DEBUGP("USB IRQ received for device on host %d\n", us->host_no);
	US_DEBUGP("-- IRQ data length is %d\n", urb->actual_length);
	US_DEBUGP("-- IRQ state is %d\n", urb->status);
	US_DEBUGP("-- Interrupt Status (0x%x, 0x%x)\n",
			us->irqbuf[0], us->irqbuf[1]);

	/* reject improper IRQs */
	if (urb->actual_length != 2) {
		US_DEBUGP("-- IRQ too short\n");
		return;
	}

	/* is the device removed? */
	if (urb->status == -ENODEV) {
		US_DEBUGP("-- device has been removed\n");
		return;
	}

	/* was this a command-completion interrupt? */
	if (us->irqbuf[0] && (us->subclass != US_SC_UFI)) {
		US_DEBUGP("-- not a command-completion IRQ\n");
		return;
	}

	/* was this a wanted interrupt? */
	if (!atomic_read(us->ip_wanted)) {
		US_DEBUGP("ERROR: Unwanted interrupt received!\n");
		return;
	}

	/* adjust the flag */
	atomic_set(us->ip_wanted, 0);
		
	/* copy the valid data */
	us->irqdata[0] = us->irqbuf[0];
	us->irqdata[1] = us->irqbuf[1];

	/* wake up the command thread */
	US_DEBUGP("-- Current value of ip_waitq is: %d\n",
			atomic_read(&us->ip_waitq.count));
	up(&(us->ip_waitq));
}

int usb_stor_CBI_transport(Scsi_Cmnd *srb, struct us_data *us)
{
	int result;

	/* Set up for status notification */
	atomic_set(us->ip_wanted, 1);

	/* re-initialize the mutex so that we avoid any races with
	 * early/late IRQs from previous commands */
	init_MUTEX_LOCKED(&(us->ip_waitq));

	/* COMMAND STAGE */
	/* let's send the command via the control pipe */
	result = usb_stor_control_msg(us, usb_sndctrlpipe(us->pusb_dev,0),
				      US_CBI_ADSC, 
				      USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 
				      us->ifnum, srb->cmnd, srb->cmd_len);

	/* check the return code for the command */
	US_DEBUGP("Call to usb_stor_control_msg() returned %d\n", result);
	if (result < 0) {
		/* Reset flag for status notification */
		atomic_set(us->ip_wanted, 0);
	}

	/* if the command was aborted, indicate that */
	if (result == -ECONNRESET)
		return USB_STOR_TRANSPORT_ABORTED;

	/* STALL must be cleared when it is detected */
	if (result == -EPIPE) {
		US_DEBUGP("-- Stall on control pipe. Clearing\n");
		result = usb_stor_clear_halt(us,	
			usb_sndctrlpipe(us->pusb_dev, 0));

		/* if the command was aborted, indicate that */
		if (result == -ECONNRESET)
			return USB_STOR_TRANSPORT_ABORTED;
		return USB_STOR_TRANSPORT_FAILED;
	}

	if (result < 0) {
		/* Uh oh... serious problem here */
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* DATA STAGE */
	/* transfer the data payload for this command, if one exists*/
	if (usb_stor_transfer_length(srb)) {
		usb_stor_transfer(srb, us);
		result = srb->result;
		US_DEBUGP("CBI data stage result is 0x%x\n", result);

		/* report any errors */
		if (result == US_BULK_TRANSFER_ABORTED) {
			atomic_set(us->ip_wanted, 0);
			return USB_STOR_TRANSPORT_ABORTED;
		}
		if (result == US_BULK_TRANSFER_FAILED) {
			atomic_set(us->ip_wanted, 0);
			return USB_STOR_TRANSPORT_FAILED;
		}
	}

	/* STATUS STAGE */

	/* go to sleep until we get this interrupt */
	US_DEBUGP("Current value of ip_waitq is: %d\n", atomic_read(&us->ip_waitq.count));
	down(&(us->ip_waitq));

	/* if we were woken up by an abort instead of the actual interrupt */
	if (atomic_read(us->ip_wanted)) {
		US_DEBUGP("Did not get interrupt on CBI\n");
		atomic_set(us->ip_wanted, 0);
		return USB_STOR_TRANSPORT_ABORTED;
	}

	US_DEBUGP("Got interrupt data (0x%x, 0x%x)\n", 
			us->irqdata[0], us->irqdata[1]);

	/* UFI gives us ASC and ASCQ, like a request sense
	 *
	 * REQUEST_SENSE and INQUIRY don't affect the sense data on UFI
	 * devices, so we ignore the information for those commands.  Note
	 * that this means we could be ignoring a real error on these
	 * commands, but that can't be helped.
	 */
	if (us->subclass == US_SC_UFI) {
		if (srb->cmnd[0] == REQUEST_SENSE ||
		    srb->cmnd[0] == INQUIRY)
			return USB_STOR_TRANSPORT_GOOD;
		else
			if (((unsigned char*)us->irq_urb->transfer_buffer)[0])
				return USB_STOR_TRANSPORT_FAILED;
			else
				return USB_STOR_TRANSPORT_GOOD;
	}

	/* If not UFI, we interpret the data as a result code 
	 * The first byte should always be a 0x0
	 * The second byte & 0x0F should be 0x0 for good, otherwise error 
	 */
	if (us->irqdata[0]) {
		US_DEBUGP("CBI IRQ data showed reserved bType %d\n",
				us->irqdata[0]);
		return USB_STOR_TRANSPORT_ERROR;
	}

	switch (us->irqdata[1] & 0x0F) {
		case 0x00: 
			return USB_STOR_TRANSPORT_GOOD;
		case 0x01: 
			return USB_STOR_TRANSPORT_FAILED;
		default: 
			return USB_STOR_TRANSPORT_ERROR;
	}

	/* we should never get here, but if we do, we're in trouble */
	return USB_STOR_TRANSPORT_ERROR;
}

/*
 * Control/Bulk transport
 */
int usb_stor_CB_transport(Scsi_Cmnd *srb, struct us_data *us)
{
	int result;

	/* COMMAND STAGE */
	/* let's send the command via the control pipe */
	result = usb_stor_control_msg(us, usb_sndctrlpipe(us->pusb_dev,0),
				      US_CBI_ADSC, 
				      USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 
				      us->ifnum, srb->cmnd, srb->cmd_len);

	/* check the return code for the command */
	US_DEBUGP("Call to usb_stor_control_msg() returned %d\n", result);
	if (result < 0) {
		/* if the command was aborted, indicate that */
		if (result == -ECONNRESET)
			return USB_STOR_TRANSPORT_ABORTED;

		/* a stall is a fatal condition from the device */
		if (result == -EPIPE) {
			US_DEBUGP("-- Stall on control pipe. Clearing\n");
			result = usb_stor_clear_halt(us,
				usb_sndctrlpipe(us->pusb_dev, 0));

			/* if the command was aborted, indicate that */
			if (result == -ECONNRESET)
				return USB_STOR_TRANSPORT_ABORTED;
			return USB_STOR_TRANSPORT_FAILED;
		}

		/* Uh oh... serious problem here */
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* DATA STAGE */
	/* transfer the data payload for this command, if one exists*/
	if (usb_stor_transfer_length(srb)) {
		usb_stor_transfer(srb, us);
		result = srb->result;
		US_DEBUGP("CB data stage result is 0x%x\n", result);

		/* report any errors */
		if (result == US_BULK_TRANSFER_ABORTED) {
			return USB_STOR_TRANSPORT_ABORTED;
		}
		if (result == US_BULK_TRANSFER_FAILED) {
			return USB_STOR_TRANSPORT_FAILED;
		}
	}

	/* STATUS STAGE */
	/* NOTE: CB does not have a status stage.  Silly, I know.  So
	 * we have to catch this at a higher level.
	 */
	return USB_STOR_TRANSPORT_GOOD;
}

/*
 * Bulk only transport
 */

/* Determine what the maximum LUN supported is */
int usb_stor_Bulk_max_lun(struct us_data *us)
{
	unsigned char *data;
	int result;
	int pipe;

	data = kmalloc(sizeof *data, GFP_KERNEL);
	if (!data) {
		return 0;
	}

	/* issue the command -- use usb_control_msg() because
	 *  the state machine is not yet alive */
	pipe = usb_rcvctrlpipe(us->pusb_dev, 0);
	result = usb_control_msg(us->pusb_dev, pipe,
				 US_BULK_GET_MAX_LUN, 
				 USB_DIR_IN | USB_TYPE_CLASS | 
				 USB_RECIP_INTERFACE,
				 0, us->ifnum, data, sizeof(*data), HZ);

	US_DEBUGP("GetMaxLUN command result is %d, data is %d\n", 
		  result, *data);

	/* if we have a successful request, return the result */
	if (result == 1) {
		result = *data;
		kfree(data);
		return result;
	} else {
		kfree(data);
	}

	/* if we get a STALL, clear the stall */
	if (result == -EPIPE) {
		US_DEBUGP("clearing endpoint halt for pipe 0x%x\n", pipe);

		/* Use usb_clear_halt() because the state machine
		 *  is not yet alive */
		usb_clear_halt(us->pusb_dev, pipe);
	}

	/* return the default -- no LUNs */
	return 0;
}

int usb_stor_Bulk_reset(struct us_data *us);

int usb_stor_Bulk_transport(Scsi_Cmnd *srb, struct us_data *us)
{
	struct bulk_cb_wrap *bcb;
	struct bulk_cs_wrap *bcs;
	int result;
	int pipe;
	int partial;
	int ret = USB_STOR_TRANSPORT_ERROR;

	bcb = kmalloc(sizeof *bcb, in_interrupt() ? GFP_ATOMIC : GFP_NOIO);
	if (!bcb) {
		return USB_STOR_TRANSPORT_ERROR;
	}
	bcs = kmalloc(sizeof *bcs, in_interrupt() ? GFP_ATOMIC : GFP_NOIO);
	if (!bcs) {
		kfree(bcb);
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* set up the command wrapper */
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength = cpu_to_le32(usb_stor_transfer_length(srb));
	bcb->Flags = srb->sc_data_direction == SCSI_DATA_READ ? 1 << 7 : 0;
	bcb->Tag = ++(us->tag);
	bcb->Lun = srb->cmnd[1] >> 5;
	if (us->flags & US_FL_SCM_MULT_TARG)
		bcb->Lun |= srb->target << 4;
	bcb->Length = srb->cmd_len;

	/* construct the pipe handle */
	pipe = usb_sndbulkpipe(us->pusb_dev, us->ep_out);

	/* copy the command payload */
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, srb->cmnd, bcb->Length);

	/* send it to out endpoint */
	US_DEBUGP("Bulk command S 0x%x T 0x%x Trg %d LUN %d L %d F %d CL %d\n",
		  le32_to_cpu(bcb->Signature), bcb->Tag,
		  (bcb->Lun >> 4), (bcb->Lun & 0x0F), 
		  le32_to_cpu(bcb->DataTransferLength), bcb->Flags, bcb->Length);
	result = usb_stor_bulk_msg(us, bcb, pipe, US_BULK_CB_WRAP_LEN, 
				   &partial);
	US_DEBUGP("Bulk command transfer result=%d\n", result);

	/* if the command was aborted, indicate that */
	if (result == -ECONNRESET) {
		ret = USB_STOR_TRANSPORT_ABORTED;
		goto out;
	}

	/* if we stall, we need to clear it before we go on */
	if (result == -EPIPE) {
		US_DEBUGP("clearing endpoint halt for pipe 0x%x\n", pipe);
		result = usb_stor_clear_halt(us, pipe);

		/* if the command was aborted, indicate that */
		if (result == -ECONNRESET) {
			ret = USB_STOR_TRANSPORT_ABORTED;
			goto out;
		}
		result = -EPIPE;
	} else if (result) {
		/* unknown error -- we've got a problem */
		ret = USB_STOR_TRANSPORT_ERROR;
		goto out;
	}

	/* if the command transfered well, then we go to the data stage */
	if (result == 0) {
		/* send/receive data payload, if there is any */
		if (bcb->DataTransferLength) {
			usb_stor_transfer(srb, us);
			result = srb->result;
			US_DEBUGP("Bulk data transfer result 0x%x\n", result);

			/* if it was aborted, we need to indicate that */
			if (result == US_BULK_TRANSFER_ABORTED) {
				ret = USB_STOR_TRANSPORT_ABORTED;
				goto out;
			}
		}
	}

	/* See flow chart on pg 15 of the Bulk Only Transport spec for
	 * an explanation of how this code works.
	 */

	/* construct the pipe handle */
	pipe = usb_rcvbulkpipe(us->pusb_dev, us->ep_in);

	/* get CSW for device status */
	US_DEBUGP("Attempting to get CSW...\n");
	result = usb_stor_bulk_msg(us, bcs, pipe, US_BULK_CS_WRAP_LEN, 
				   &partial);

	/* if the command was aborted, indicate that */
	if (result == -ECONNRESET) {
		ret = USB_STOR_TRANSPORT_ABORTED;
		goto out;
	}

	/* did the attempt to read the CSW fail? */
	if (result == -EPIPE) {
		US_DEBUGP("clearing endpoint halt for pipe 0x%x\n", pipe);
		result = usb_stor_clear_halt(us, pipe);

		/* if the command was aborted, indicate that */
		if (result == -ECONNRESET) {
			ret = USB_STOR_TRANSPORT_ABORTED;
			goto out;
		}

		/* get the status again */
		US_DEBUGP("Attempting to get CSW (2nd try)...\n");
		result = usb_stor_bulk_msg(us, bcs, pipe,
					   US_BULK_CS_WRAP_LEN, &partial);

		/* if the command was aborted, indicate that */
		if (result == -ECONNRESET) {
			ret = USB_STOR_TRANSPORT_ABORTED;
			goto out;
		}

		/* if it fails again, we need a reset and return an error*/
		if (result == -EPIPE) {
			US_DEBUGP("clearing halt for pipe 0x%x\n", pipe);
			result = usb_stor_clear_halt(us, pipe);

			/* if the command was aborted, indicate that */
			if (result == -ECONNRESET) {
				ret = USB_STOR_TRANSPORT_ABORTED;
			} else {
				ret = USB_STOR_TRANSPORT_ERROR;
			}
			goto out;
		}
	}

	/* if we still have a failure at this point, we're in trouble */
	US_DEBUGP("Bulk status result = %d\n", result);
	if (result) {
		ret = USB_STOR_TRANSPORT_ERROR;
		goto out;
	}

	/* check bulk status */
	US_DEBUGP("Bulk status Sig 0x%x T 0x%x R %d Stat 0x%x\n",
		  le32_to_cpu(bcs->Signature), bcs->Tag, 
		  bcs->Residue, bcs->Status);
	if ((bcs->Signature != cpu_to_le32(US_BULK_CS_SIGN) && bcs->Signature != cpu_to_le32(US_BULK_CS_OLYMPUS_SIGN)) ||
	    bcs->Tag != bcb->Tag || 
	    bcs->Status > US_BULK_STAT_PHASE || partial != 13) {
		US_DEBUGP("Bulk logical error\n");
		ret = USB_STOR_TRANSPORT_ERROR;
		goto out;
	}

	/* based on the status code, we report good or bad */
	switch (bcs->Status) {
		case US_BULK_STAT_OK:
			/* command good -- note that data could be short */
			ret = USB_STOR_TRANSPORT_GOOD;
			goto out;

		case US_BULK_STAT_FAIL:
			/* command failed */
			ret = USB_STOR_TRANSPORT_FAILED;
			goto out;

		case US_BULK_STAT_PHASE:
			/* phase error -- note that a transport reset will be
			 * invoked by the invoke_transport() function
			 */
			ret = USB_STOR_TRANSPORT_ERROR;
			goto out;
	}

	/* we should never get here, but if we do, we're in trouble */

 out:
	kfree(bcb);
	kfree(bcs);
	return ret;
}

/***********************************************************************
 * Reset routines
 ***********************************************************************/

/* This issues a CB[I] Reset to the device in question
 */
int usb_stor_CB_reset(struct us_data *us)
{
	unsigned char cmd[12];
	int result;

	US_DEBUGP("CB_reset() called\n");

	/* if the device was removed, then we're already reset */
	if (!us->pusb_dev)
		return SUCCESS;

	memset(cmd, 0xFF, sizeof(cmd));
	cmd[0] = SEND_DIAGNOSTIC;
	cmd[1] = 4;
	result = usb_control_msg(us->pusb_dev, usb_sndctrlpipe(us->pusb_dev,0),
				 US_CBI_ADSC, 
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, cmd, sizeof(cmd), HZ*5);

	if (result < 0) {
		US_DEBUGP("CB[I] soft reset failed %d\n", result);
		return FAILED;
	}

	/* long wait for reset */
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(HZ*6);
	set_current_state(TASK_RUNNING);

	US_DEBUGP("CB_reset: clearing endpoint halt\n");
	usb_stor_clear_halt(us,
			usb_rcvbulkpipe(us->pusb_dev, us->ep_in));
	usb_stor_clear_halt(us,
			usb_sndbulkpipe(us->pusb_dev, us->ep_out));

	US_DEBUGP("CB_reset done\n");
	/* return a result code based on the result of the control message */
	return SUCCESS;
}

/* This issues a Bulk-only Reset to the device in question, including
 * clearing the subsequent endpoint halts that may occur.
 */
int usb_stor_Bulk_reset(struct us_data *us)
{
	int result;

	US_DEBUGP("Bulk reset requested\n");

	/* if the device was removed, then we're already reset */
	if (!us->pusb_dev)
		return SUCCESS;

	result = usb_control_msg(us->pusb_dev, 
				 usb_sndctrlpipe(us->pusb_dev,0), 
				 US_BULK_RESET_REQUEST, 
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, NULL, 0, HZ*5);

	if (result < 0) {
		US_DEBUGP("Bulk soft reset failed %d\n", result);
		return FAILED;
	}

	/* long wait for reset */
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(HZ*6);
	set_current_state(TASK_RUNNING);

	usb_stor_clear_halt(us,
			usb_rcvbulkpipe(us->pusb_dev, us->ep_in));
	usb_stor_clear_halt(us,
			usb_sndbulkpipe(us->pusb_dev, us->ep_out));
	US_DEBUGP("Bulk soft reset completed\n");
	return SUCCESS;
}
