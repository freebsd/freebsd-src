/* Driver for SCM Microsystems USB-ATAPI cable
 *
 * $Id: shuttle_usbat.c,v 1.16 2002/02/25 00:40:13 mdharm Exp $
 *
 * Current development and maintenance by:
 *   (c) 2000, 2001 Robert Baruch (autophile@starband.net)
 *
 * Developed with the assistance of:
 *   (c) 2002 Alan Stern <stern@rowland.org>
 *
 * Many originally ATAPI devices were slightly modified to meet the USB
 * market by using some kind of translation from ATAPI to USB on the host,
 * and the peripheral would translate from USB back to ATAPI.
 *
 * SCM Microsystems (www.scmmicro.com) makes a device, sold to OEM's only, 
 * which does the USB-to-ATAPI conversion.  By obtaining the data sheet on
 * their device under nondisclosure agreement, I have been able to write
 * this driver for Linux.
 *
 * The chip used in the device can also be used for EPP and ISA translation
 * as well. This driver is only guaranteed to work with the ATAPI
 * translation.
 *
 * The only peripheral that I know of (as of 27 Mar 2001) that uses this
 * device is the Hewlett-Packard 8200e/8210e/8230e CD-Writer Plus.
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

#include "transport.h"
#include "protocol.h"
#include "usb.h"
#include "debug.h"
#include "shuttle_usbat.h"

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>

extern int usb_stor_control_msg(struct us_data *us, unsigned int pipe,
	u8 request, u8 requesttype, u16 value, u16 index,
	void *data, u16 size);
extern int usb_stor_bulk_msg(struct us_data *us, void *data, int pipe,
	unsigned int len, unsigned int *act_len);

#define short_pack(LSB,MSB) ( ((u16)(LSB)) | ( ((u16)(MSB))<<8 ) )
#define LSB_of(s) ((s)&0xFF)
#define MSB_of(s) ((s)>>8)

int transferred = 0;

/*
 * Send a control message and wait for the response.
 *
 * us - the pointer to the us_data structure for the device to use
 *
 * request - the URB Setup Packet's first 6 bytes. The first byte always
 *  corresponds to the request type, and the second byte always corresponds
 *  to the request.  The other 4 bytes do not correspond to value and index,
 *  since they are used in a custom way by the SCM protocol.
 *
 * xfer_data - a buffer from which to get, or to which to store, any data
 *  that gets send or received, respectively, with the URB. Even though
 *  it looks like we allocate a buffer in this code for the data, xfer_data
 *  must contain enough allocated space.
 *
 * xfer_len - the number of bytes to send or receive with the URB.
 *
 */

static int usbat_send_control(struct us_data *us,
		int pipe,
		unsigned char request,
		unsigned char requesttype,
		unsigned short value,
		unsigned short index,
		unsigned char *xfer_data,
		unsigned int xfer_len) {

	int result;

	// Send the URB to the device and wait for a response.

	/* Why are request and request type reversed in this call? */

	result = usb_stor_control_msg(us, pipe,
			request, requesttype, value, index,
			xfer_data, xfer_len);


	// Check the return code for the command.

	if (result < 0) {
		/* if the command was aborted, indicate that */
		if (result == -ECONNRESET)
			return USB_STOR_TRANSPORT_ABORTED;

		/* a stall is a fatal condition from the device */
		if (result == -EPIPE) {
			US_DEBUGP("-- Stall on control pipe. Clearing\n");
			result = usb_stor_clear_halt(us, pipe);
			US_DEBUGP("-- usb_stor_clear_halt() returns %d\n", result);
			return USB_STOR_TRANSPORT_FAILED;
		}

		/* Uh oh... serious problem here */
		return USB_STOR_TRANSPORT_ERROR;
	}

	return USB_STOR_TRANSPORT_GOOD;
}

static int usbat_raw_bulk(struct us_data *us, 
		int direction,
		unsigned char *data,
		unsigned short len) {

	int result;
	int act_len;
	int pipe;

	if (direction == SCSI_DATA_READ)
		pipe = usb_rcvbulkpipe(us->pusb_dev, us->ep_in);
	else
		pipe = usb_sndbulkpipe(us->pusb_dev, us->ep_out);

	result = usb_stor_bulk_msg(us, data, pipe, len, &act_len);

        /* if we stall, we need to clear it before we go on */
        if (result == -EPIPE) {
       	        US_DEBUGP("EPIPE: clearing endpoint halt for"
			" pipe 0x%x, stalled at %d bytes\n",
			pipe, act_len);
               	usb_stor_clear_halt(us, pipe);
        }

	if (result) {

                /* NAK - that means we've retried a few times already */
       	        if (result == -ETIMEDOUT) {
                        US_DEBUGP("usbat_raw_bulk():"
				" device NAKed\n");
                        return US_BULK_TRANSFER_FAILED;
                }

                /* -ECONNRESET -- we canceled this transfer */
                if (result == -ECONNRESET) {
                        US_DEBUGP("usbat_raw_bulk():"
				" transfer aborted\n");
                        return US_BULK_TRANSFER_ABORTED;
                }

		if (result == -EPIPE) {
			US_DEBUGP("usbat_raw_bulk():"
				" output pipe stalled\n");
			return US_BULK_TRANSFER_SHORT;
		}

                /* the catch-all case */
                US_DEBUGP("us_transfer_partial(): unknown error\n");
                return US_BULK_TRANSFER_FAILED;
        }

	if (act_len != len) {
		US_DEBUGP("Warning: Transferred only %d bytes\n",
			act_len);
		return US_BULK_TRANSFER_SHORT;
	}

	US_DEBUGP("Transferred %s %d of %d bytes\n", 
		direction==SCSI_DATA_READ ? "in" : "out", act_len, len);

	return US_BULK_TRANSFER_GOOD;
}

/*
 * Note: direction must be set if command_len == 0.
 */

static int usbat_bulk_transport(struct us_data *us,
			  unsigned char *command,
			  unsigned short command_len,
			  int direction,
			  unsigned char *data,
			  unsigned short len,
			  int use_sg) {

	int result = USB_STOR_TRANSPORT_GOOD;
	int transferred = 0;
	int i;
	struct scatterlist *sg;

	if (len==0)
		return USB_STOR_TRANSPORT_GOOD;

	/* transfer the data payload for the command, if there is any */

	if (command_len != 0)
		direction = (command[0]&0x80) ? SCSI_DATA_READ :
			SCSI_DATA_WRITE;

	if (!use_sg)
		result = usbat_raw_bulk(us, direction, data, len);
	else {
		sg = (struct scatterlist *)data;
		for (i=0; i<use_sg && transferred<len; i++) {
			result = usbat_raw_bulk(us, direction,
				sg[i].address, 
				len-transferred > sg[i].length ?
					sg[i].length : len-transferred);
			if (result!=US_BULK_TRANSFER_GOOD)
				break;
			transferred += sg[i].length;
		}
	}

	return result;
}

int usbat_read(struct us_data *us,
	     unsigned char access,
	     unsigned char reg, 
	     unsigned char *content) {

	int result;

	result = usbat_send_control(us,
		usb_rcvctrlpipe(us->pusb_dev,0),
		access,
		0xC0,
		(u16)reg,
		0,
		content,
		1);

	return result;
}

int usbat_write(struct us_data *us,
	     unsigned char access,
	     unsigned char reg, 
	     unsigned char content) {

	int result;

	result = usbat_send_control(us,
		usb_sndctrlpipe(us->pusb_dev,0),
		access|0x01,
		0x40,
		short_pack(reg, content),
		0,
		NULL,
		0);

	return result;
}

int usbat_set_shuttle_features(struct us_data *us,
	     unsigned char external_trigger,
	     unsigned char epp_control, 
	     unsigned char mask_byte, 
	     unsigned char test_pattern, 
	     unsigned char subcountH, 
	     unsigned char subcountL) {

	int result;
	unsigned char command[8] = {
		0x40, 0x81, epp_control, external_trigger,
		test_pattern, mask_byte, subcountL, subcountH
	};

	result = usbat_send_control(us,
		usb_sndctrlpipe(us->pusb_dev,0),
		0x80,
		0x40,
		0,
		0,
		command,
		8);

	return result;
}

int usbat_read_block(struct us_data *us,
	     unsigned char access,
	     unsigned char reg, 
	     unsigned char *content,
	     unsigned short len,
	     int use_sg) {

	int result;
	unsigned char command[8] = {
		0xC0, access|0x02, reg, 0x00, 0x00, 0x00, 
		LSB_of(len), MSB_of(len)
	};

	result = usbat_send_control(us,
		usb_sndctrlpipe(us->pusb_dev,0),
		0x80,
		0x40,
		0,
		0,
		command,
		8);

	if (result != USB_STOR_TRANSPORT_GOOD)
		return result;

	result = usbat_bulk_transport(us,
		NULL, 0, SCSI_DATA_READ, content, len, use_sg);

	return result;
}

/*
 * Block, waiting for an ATA device to become not busy or to report
 * an error condition.
 */

int usbat_wait_not_busy(struct us_data *us, int minutes) {

	int i;
	int result;
	unsigned char status;

	/* Synchronizing cache on a CDR could take a heck of a long time,
	 * but probably not more than 10 minutes or so. On the other hand,
	 * doing a full blank on a CDRW at speed 1 will take about 75
	 * minutes!
	 */

	for (i=0; i<1200+minutes*60; i++) {

 		result = usbat_read(us, USBAT_ATA, 0x17, &status);

		if (result!=USB_STOR_TRANSPORT_GOOD)
			return result;
		if (status&0x01) { // check condition
			result = usbat_read(us, USBAT_ATA, 0x10, &status);
			return USB_STOR_TRANSPORT_FAILED;
		}
		if (status&0x20) // device fault
			return USB_STOR_TRANSPORT_FAILED;

		if ((status&0x80)==0x00) { // not busy
			US_DEBUGP("Waited not busy for %d steps\n", i);
			return USB_STOR_TRANSPORT_GOOD;
		}

		if (i<500)
			wait_ms(10); // 5 seconds
		else if (i<700)
			wait_ms(50); // 10 seconds
		else if (i<1200)
			wait_ms(100); // 50 seconds
		else
			wait_ms(1000); // X minutes
	}

	US_DEBUGP("Waited not busy for %d minutes, timing out.\n",
		minutes);
	return USB_STOR_TRANSPORT_FAILED;
}

int usbat_write_block(struct us_data *us,
	     unsigned char access,
	     unsigned char reg, 
	     unsigned char *content,
	     unsigned short len,
	     int use_sg,
	     int minutes) {

	int result;
	unsigned char command[8] = {
		0x40, access|0x03, reg, 0x00, 0x00, 0x00, 
		LSB_of(len), MSB_of(len)
	};

	result = usbat_send_control(us,
		usb_sndctrlpipe(us->pusb_dev,0),
		0x80,
		0x40,
		0,
		0,
		command,
		8);

	if (result != USB_STOR_TRANSPORT_GOOD)
		return result;

	result = usbat_bulk_transport(us,
		NULL, 0, SCSI_DATA_WRITE, content, len, use_sg);

	if (result != USB_STOR_TRANSPORT_GOOD)
		return result;

	return usbat_wait_not_busy(us, minutes);
}

int usbat_rw_block_test(struct us_data *us,
	     unsigned char access,
	     unsigned char *registers,
	     unsigned char *data_out,
	     unsigned short num_registers,
	     unsigned char data_reg, 
	     unsigned char status_reg, 
	     unsigned char timeout, 
	     unsigned char qualifier, 
	     int direction,
	     unsigned char *content,
	     unsigned short len,
	     int use_sg,
	     int minutes) {

	int result;

	// Not really sure the 0x07, 0x17, 0xfc, 0xe7 is necessary here,
	// but that's what came out of the trace every single time.

	unsigned char command[16] = {
		0x40, access|0x07, 0x07, 0x17, 0xfc, 0xe7,
		LSB_of(num_registers*2), MSB_of(num_registers*2),
		(direction==SCSI_DATA_WRITE ? 0x40 : 0xC0), 
		access|(direction==SCSI_DATA_WRITE ? 0x05 : 0x04), 
		data_reg, status_reg,
		timeout, qualifier, LSB_of(len), MSB_of(len)
	};

	int i;
	unsigned char data[num_registers*2];
	unsigned char status;

	for (i=0; i<num_registers; i++) {
		data[i<<1] = registers[i];
		data[1+(i<<1)] = data_out[i];
	}

	for (i=0; i<20; i++) {

		/*
		 * The first time we send the full command, which consists
		 * of downloading the SCSI command followed by downloading
		 * the data via a write-and-test.  Any other time we only
		 * send the command to download the data -- the SCSI command
		 * is still 'active' in some sense in the device.
		 * 
		 * We're only going to try sending the data 10 times. After
		 * that, we just return a failure.
		 */

		result = usbat_send_control(us,
			  usb_sndctrlpipe(us->pusb_dev,0),
			0x80,
			0x40,
			0,
			0,
			(i==0 ? command : command+8),
			(i==0 ? 16 : 8));

		if (result != USB_STOR_TRANSPORT_GOOD)
			return result;

		if (i==0) {

			result = usbat_bulk_transport(us,
				NULL, 0, SCSI_DATA_WRITE, 
				data, num_registers*2, 0);

			if (result!=USB_STOR_TRANSPORT_GOOD)
				return result;

		}


		//US_DEBUGP("Transfer %s %d bytes, sg buffers %d\n",
		//	direction == SCSI_DATA_WRITE ? "out" : "in",
		//	len, use_sg);

		result = usbat_bulk_transport(us,
			NULL, 0, direction, content, len, use_sg);

		/*
		 * If we get a stall on the bulk download, we'll retry
		 * the bulk download -- but not the SCSI command because
		 * in some sense the SCSI command is still 'active' and
		 * waiting for the data. Don't ask me why this should be;
		 * I'm only following what the Windoze driver did.
		 *
		 * Note that a stall for the test-and-read/write command means
		 * that the test failed. In this case we're testing to make
		 * sure that the device is error-free
		 * (i.e. bit 0 -- CHK -- of status is 0). The most likely
		 * hypothesis is that the USBAT chip somehow knows what
		 * the device will accept, but doesn't give the device any
		 * data until all data is received. Thus, the device would
		 * still be waiting for the first byte of data if a stall
		 * occurs, even if the stall implies that some data was
		 * transferred.
		 */

		if (result == US_BULK_TRANSFER_SHORT) {

			/*
			 * If we're reading and we stalled, then clear
			 * the bulk output pipe only the first time.
			 */

			if (direction==SCSI_DATA_READ && i==0)
				usb_stor_clear_halt(us,
					usb_sndbulkpipe(us->pusb_dev,
					  us->ep_out));
			/*
			 * Read status: is the device angry, or just busy?
			 */

 			result = usbat_read(us, USBAT_ATA, 
				direction==SCSI_DATA_WRITE ? 0x17 : 0x0E, 
				&status);

			if (result!=USB_STOR_TRANSPORT_GOOD)
				return result;
			if (status&0x01) // check condition
				return USB_STOR_TRANSPORT_FAILED;
			if (status&0x20) // device fault
				return USB_STOR_TRANSPORT_FAILED;

			US_DEBUGP("Redoing %s\n",
			  direction==SCSI_DATA_WRITE ? "write" : "read");

		} else if (result != US_BULK_TRANSFER_GOOD)
			return result;
		else
			return usbat_wait_not_busy(us, minutes);

	}

	US_DEBUGP("Bummer! %s bulk data 20 times failed.\n",
		direction==SCSI_DATA_WRITE ? "Writing" : "Reading");

	return USB_STOR_TRANSPORT_FAILED;
}

/*
 * Write data to multiple registers at once. Not meant for large
 * transfers of data!
 */

int usbat_multiple_write(struct us_data *us, 
			unsigned char access,
			unsigned char *registers,
			unsigned char *data_out,
			unsigned short num_registers) {

	int result;
	unsigned char data[num_registers*2];
	int i;
	unsigned char command[8] = {
		0x40, access|0x07, 0x00, 0x00, 0x00, 0x00,
		LSB_of(num_registers*2), MSB_of(num_registers*2)
	};

	for (i=0; i<num_registers; i++) {
		data[i<<1] = registers[i];
		data[1+(i<<1)] = data_out[i];
	}

	result = usbat_send_control(us,
		usb_sndctrlpipe(us->pusb_dev,0),
		0x80,
		0x40,
		0,
		0,
		command,
		8);

	if (result != USB_STOR_TRANSPORT_GOOD)
		return result;

	result = usbat_bulk_transport(us,
		NULL, 0, SCSI_DATA_WRITE, data, num_registers*2, 0);

	if (result!=USB_STOR_TRANSPORT_GOOD)
		return result;

	return usbat_wait_not_busy(us, 0);
}

int usbat_read_user_io(struct us_data *us,
		unsigned char *data_flags) {

	int result;

	result = usbat_send_control(us,
		usb_rcvctrlpipe(us->pusb_dev,0),
		0x82,
		0xC0,
		0,
		0,
		data_flags,
		1);

	return result;
}

int usbat_write_user_io(struct us_data *us,
		unsigned char enable_flags,
		unsigned char data_flags) {

	int result;

	result = usbat_send_control(us,
		usb_sndctrlpipe(us->pusb_dev,0),
		0x82,
		0x40,
		short_pack(enable_flags, data_flags),
		0,
		NULL,
		0);

	return result;
}

/*
 * Squeeze a potentially huge (> 65535 byte) read10 command into
 * a little ( <= 65535 byte) ATAPI pipe
 */

int usbat_handle_read10(struct us_data *us,
		unsigned char *registers,
		unsigned char *data,
		Scsi_Cmnd *srb) {

	int result = USB_STOR_TRANSPORT_GOOD;
	unsigned char *buffer;
	unsigned int len;
	unsigned int sector;
	unsigned int amount;
	struct scatterlist *sg = NULL;
	int sg_segment = 0;
	int sg_offset = 0;

	US_DEBUGP("handle_read10: transfersize %d\n",
		srb->transfersize);

	if (srb->request_bufflen < 0x10000) {

		result = usbat_rw_block_test(us, USBAT_ATA, 
			registers, data, 19,
			0x10, 0x17, 0xFD, 0x30,
			SCSI_DATA_READ,
			srb->request_buffer, 
			srb->request_bufflen, srb->use_sg, 1);

		return result;
	}

	/*
	 * Since we're requesting more data than we can handle in
	 * a single read command (max is 64k-1), we will perform
	 * multiple reads, but each read must be in multiples of
	 * a sector.  Luckily the sector size is in srb->transfersize
	 * (see linux/drivers/scsi/sr.c).
	 */

	if (data[7+0] == GPCMD_READ_CD) {
		len = short_pack(data[7+9], data[7+8]);
		len <<= 16;
		len |= data[7+7];
		US_DEBUGP("handle_read10: GPCMD_READ_CD: len %d\n", len);
		srb->transfersize = srb->request_bufflen/len;
	}

	if (!srb->transfersize)  {
		srb->transfersize = 2048; /* A guess */
		US_DEBUGP("handle_read10: transfersize 0, forcing %d\n",
			srb->transfersize);
	}

	len = (65535/srb->transfersize) * srb->transfersize;
	US_DEBUGP("Max read is %d bytes\n", len);
	buffer = kmalloc(len, GFP_NOIO);
	if (buffer == NULL) // bloody hell!
		return USB_STOR_TRANSPORT_FAILED;
	sector = short_pack(data[7+3], data[7+2]);
	sector <<= 16;
	sector |= short_pack(data[7+5], data[7+4]);
	transferred = 0;

	if (srb->use_sg) {
		sg = (struct scatterlist *)srb->request_buffer;
		sg_segment = 0; // for keeping track of where we are in
		sg_offset = 0;  // the scatter/gather list
	}

	while (transferred != srb->request_bufflen) {

		if (len > srb->request_bufflen - transferred)
			len = srb->request_bufflen - transferred;

		data[3] = len&0xFF; 	  // (cylL) = expected length (L)
		data[4] = (len>>8)&0xFF;  // (cylH) = expected length (H)

		// Fix up the SCSI command sector and num sectors

		data[7+2] = MSB_of(sector>>16); // SCSI command sector
		data[7+3] = LSB_of(sector>>16);
		data[7+4] = MSB_of(sector&0xFFFF);
		data[7+5] = LSB_of(sector&0xFFFF);
		if (data[7+0] == GPCMD_READ_CD)
			data[7+6] = 0;
		data[7+7] = MSB_of(len / srb->transfersize); // SCSI command
		data[7+8] = LSB_of(len / srb->transfersize); // num sectors

		result = usbat_rw_block_test(us, USBAT_ATA, 
			registers, data, 19,
			0x10, 0x17, 0xFD, 0x30,
			SCSI_DATA_READ,
			buffer,
			len, 0, 1);

		if (result != USB_STOR_TRANSPORT_GOOD)
			break;

		// Transfer the received data into the srb buffer

		if (!srb->use_sg) {
			memcpy(srb->request_buffer+transferred, buffer, len);
		} else {
			amount = 0;
			while (amount<len) {
				if (len - amount >= 
					  sg[sg_segment].length-sg_offset) {
				  memcpy(sg[sg_segment].address + sg_offset,
					buffer + amount,
					sg[sg_segment].length - sg_offset);
				  amount += 
					  sg[sg_segment].length-sg_offset;
				  sg_segment++;
				  sg_offset=0;
				} else {
				  memcpy(sg[sg_segment].address + sg_offset,
					buffer + amount,
					len - amount);
				  sg_offset += (len - amount);
				  amount = len;
				}
			}
		}

		// Update the amount transferred and the sector number

		transferred += len;
		sector += len / srb->transfersize;

	} // while transferred != srb->request_bufflen

	kfree(buffer);
	return result;
}

static int hp_8200e_select_and_test_registers(struct us_data *us) {

	int result;
	int selector;
	unsigned char status;

	// try device = master, then device = slave.

	for (selector = 0xA0; selector <= 0xB0; selector += 0x10) {

		if ( (result = usbat_write(us, USBAT_ATA, 0x16, selector)) != 
				USB_STOR_TRANSPORT_GOOD)
			return result;

		if ( (result = usbat_read(us, USBAT_ATA, 0x17, &status)) != 
				USB_STOR_TRANSPORT_GOOD)
			return result;

		if ( (result = usbat_read(us, USBAT_ATA, 0x16, &status)) != 
				USB_STOR_TRANSPORT_GOOD)
			return result;

		if ( (result = usbat_read(us, USBAT_ATA, 0x14, &status)) != 
				USB_STOR_TRANSPORT_GOOD)
			return result;

		if ( (result = usbat_read(us, USBAT_ATA, 0x15, &status)) != 
				USB_STOR_TRANSPORT_GOOD)
			return result;

		if ( (result = usbat_write(us, USBAT_ATA, 0x14, 0x55)) != 
				USB_STOR_TRANSPORT_GOOD)
			return result;

		if ( (result = usbat_write(us, USBAT_ATA, 0x15, 0xAA)) != 
				USB_STOR_TRANSPORT_GOOD)
			return result;

		if ( (result = usbat_read(us, USBAT_ATA, 0x14, &status)) != 
				USB_STOR_TRANSPORT_GOOD)
			return result;

		if ( (result = usbat_read(us, USBAT_ATA, 0x15, &status)) != 
				USB_STOR_TRANSPORT_GOOD)
			return result;
	}

	return result;
}

int init_8200e(struct us_data *us) {

	int result;
	unsigned char status;

	// Enable peripheral control signals

	if ( (result = usbat_write_user_io(us,
	  USBAT_UIO_OE1 | USBAT_UIO_OE0,
	  USBAT_UIO_EPAD | USBAT_UIO_1)) != USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 1\n");

	wait_ms(2000);

	if ( (result = usbat_read_user_io(us, &status)) !=
			USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 2\n");

	if ( (result = usbat_read_user_io(us, &status)) !=
			USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 3\n");

	// Reset peripheral, enable periph control signals
	// (bring reset signal up)

	if ( (result = usbat_write_user_io(us,
	  USBAT_UIO_DRVRST | USBAT_UIO_OE1 | USBAT_UIO_OE0,
	  USBAT_UIO_EPAD | USBAT_UIO_1)) != USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 4\n");

	// Enable periph control signals
	// (bring reset signal down)

	if ( (result = usbat_write_user_io(us,
	  USBAT_UIO_OE1 | USBAT_UIO_OE0,
	  USBAT_UIO_EPAD | USBAT_UIO_1)) != USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 5\n");

	wait_ms(250);

	// Write 0x80 to ISA port 0x3F

	if ( (result = usbat_write(us, USBAT_ISA, 0x3F, 0x80)) !=
			USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 6\n");

	// Read ISA port 0x27

	if ( (result = usbat_read(us, USBAT_ISA, 0x27, &status)) !=
			USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 7\n");

	if ( (result = usbat_read_user_io(us, &status)) !=
			USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 8\n");

	if ( (result = hp_8200e_select_and_test_registers(us)) !=
			 USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 9\n");

	if ( (result = usbat_read_user_io(us, &status)) !=
			USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 10\n");

	// Enable periph control signals and card detect

	if ( (result = usbat_write_user_io(us,
	  USBAT_UIO_ACKD |USBAT_UIO_OE1 | USBAT_UIO_OE0,
	  USBAT_UIO_EPAD | USBAT_UIO_1)) != USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 11\n");

	if ( (result = usbat_read_user_io(us, &status)) !=
			USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 12\n");

	wait_ms(1400);

	if ( (result = usbat_read_user_io(us, &status)) !=
			USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 13\n");

	if ( (result = hp_8200e_select_and_test_registers(us)) !=
			 USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 14\n");

	if ( (result = usbat_set_shuttle_features(us, 
			0x83, 0x00, 0x88, 0x08, 0x15, 0x14)) !=
			 USB_STOR_TRANSPORT_GOOD)
		return result;

	US_DEBUGP("INIT 15\n");

	return result;
}

/*
 * Transport for the HP 8200e
 */
int hp8200e_transport(Scsi_Cmnd *srb, struct us_data *us)
{
	int result;
	unsigned char status;
	unsigned char registers[32];
	unsigned char data[32];
	unsigned int len;
	int i;
	char string[64];

	len = srb->request_bufflen;

	/* Send A0 (ATA PACKET COMMAND).
	   Note: I guess we're never going to get any of the ATA
	   commands... just ATA Packet Commands.
 	 */

	registers[0] = 0x11;
	registers[1] = 0x12;
	registers[2] = 0x13;
	registers[3] = 0x14;
	registers[4] = 0x15;
	registers[5] = 0x16;
	registers[6] = 0x17;
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = len&0xFF; 		// (cylL) = expected length (L)
	data[4] = (len>>8)&0xFF; 	// (cylH) = expected length (H)
	data[5] = 0xB0; 		// (device sel) = slave
	data[6] = 0xA0; 		// (command) = ATA PACKET COMMAND

	for (i=7; i<19; i++) {
		registers[i] = 0x10;
		data[i] = (i-7 >= srb->cmd_len) ? 0 : srb->cmnd[i-7];
	}

	result = usbat_read(us, USBAT_ATA, 0x17, &status);
	US_DEBUGP("Status = %02X\n", status);

	if (srb->cmnd[0] == TEST_UNIT_READY)
		transferred = 0;

	if (srb->sc_data_direction == SCSI_DATA_WRITE) {

		result = usbat_rw_block_test(us, USBAT_ATA, 
			registers, data, 19,
			0x10, 0x17, 0xFD, 0x30,
			SCSI_DATA_WRITE,
			srb->request_buffer, 
			len, srb->use_sg, 10);

		if (result == USB_STOR_TRANSPORT_GOOD) {
			transferred += len;
			US_DEBUGP("Wrote %08X bytes\n", transferred);
		}

		return result;

	} else if (srb->cmnd[0] == READ_10 ||
		   srb->cmnd[0] == GPCMD_READ_CD) {

		return usbat_handle_read10(us, registers, data, srb);

	}

	if (len > 0xFFFF) {
		US_DEBUGP("Error: len = %08X... what do I do now?\n",
			len);
		return USB_STOR_TRANSPORT_ERROR;
	}

	if ( (result = usbat_multiple_write(us, 
			USBAT_ATA,
			registers, data, 7)) != USB_STOR_TRANSPORT_GOOD) {
		return result;
	}

	// Write the 12-byte command header.

	// If the command is BLANK then set the timer for 75 minutes.
	// Otherwise set it for 10 minutes.

	// NOTE: THE 8200 DOCUMENTATION STATES THAT BLANKING A CDRW
	// AT SPEED 4 IS UNRELIABLE!!!

	if ( (result = usbat_write_block(us, 
			USBAT_ATA, 0x10, srb->cmnd, 12, 0,
			srb->cmnd[0]==GPCMD_BLANK ? 75 : 10)) !=
				USB_STOR_TRANSPORT_GOOD) {
		return result;
	}

	// If there is response data to be read in 
	// then do it here.

	if (len != 0 && (srb->sc_data_direction == SCSI_DATA_READ)) {

		// How many bytes to read in? Check cylL register

		if ( (result = usbat_read(us, USBAT_ATA, 0x14, &status)) != 
		    USB_STOR_TRANSPORT_GOOD) {
			return result;
		}

		if (len>0xFF) { // need to read cylH also
			len = status;
			if ( (result = usbat_read(us, USBAT_ATA, 0x15,
				&status)) !=
				    USB_STOR_TRANSPORT_GOOD) {
				return result;
			}
			len += ((unsigned int)status)<<8;
		}
		else
			len = status;


		result = usbat_read_block(us, USBAT_ATA, 0x10, 
			srb->request_buffer, len, srb->use_sg);

		/* Debug-print the first 32 bytes of the transfer */

		if (!srb->use_sg) {
			string[0] = 0;
			for (i=0; i<len && i<32; i++) {
				sprintf(string+strlen(string), "%02X ",
				  ((unsigned char *)srb->request_buffer)[i]);
				if ((i%16)==15) {
					US_DEBUGP("%s\n", string);
					string[0] = 0;
				}
			}
			if (string[0]!=0)
				US_DEBUGP("%s\n", string);
		}
	}

	return result;
}


