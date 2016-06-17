/* Driver for Datafab USB Compact Flash reader
 *
 * $Id: datafab.c,v 1.7 2002/02/25 00:40:13 mdharm Exp $
 *
 * datafab driver v0.1:
 *
 * First release
 *
 * Current development and maintenance by:
 *   (c) 2000 Jimmie Mayfield (mayfield+datafab@sackheads.org)
 *
 *   Many thanks to Robert Baruch for the SanDisk SmartMedia reader driver
 *   which I used as a template for this driver.
 *
 *   Some bugfixes and scatter-gather code by Gregory P. Smith 
 *   (greg-usb@electricrain.com)
 *
 *   Fix for media change by Joerg Schneider (js@joergschneider.com)
 *
 * Other contributors:
 *   (c) 2002 Alan Stern <stern@rowland.org>
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

/*
 * This driver attempts to support USB CompactFlash reader/writer devices
 * based on Datafab USB-to-ATA chips.  It was specifically developed for the 
 * Datafab MDCFE-B USB CompactFlash reader but has since been found to work 
 * with a variety of Datafab-based devices from a number of manufacturers.
 * I've received a report of this driver working with a Datafab-based
 * SmartMedia device though please be aware that I'm personally unable to
 * test SmartMedia support.
 *
 * This driver supports reading and writing.  If you're truly paranoid,
 * however, you can force the driver into a write-protected state by setting
 * the WP enable bits in datafab_handle_mode_sense().  Basically this means
 * setting mode_param_header[3] = 0x80.
 */

#include "transport.h"
#include "protocol.h"
#include "usb.h"
#include "debug.h"
#include "datafab.h"

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>

extern int usb_stor_bulk_msg(struct us_data *us, void *data, int pipe,
			     unsigned int len, unsigned int *act_len);

static int datafab_determine_lun(struct us_data *us, struct datafab_info *info);


static void datafab_dump_data(unsigned char *data, int len)
{
	unsigned char buf[80];
	int sofar = 0;

	if (!data)
		return;

	memset(buf, 0, sizeof(buf));

	for (sofar = 0; sofar < len; sofar++) {
		sprintf(buf + strlen(buf), "%02x ",
			((unsigned int) data[sofar]) & 0xFF);

		if (sofar % 16 == 15) {
			US_DEBUGP("datafab:  %s\n", buf);
			memset(buf, 0, sizeof(buf));
		}
	}

	if (strlen(buf) != 0)
		US_DEBUGP("datafab:  %s\n", buf);
}


static int datafab_raw_bulk(int direction,
			    struct us_data *us,
			    unsigned char *data, 
		            unsigned int len)
{
	int result;
	int act_len;
	int pipe;

	if (direction == SCSI_DATA_READ)
		pipe = usb_rcvbulkpipe(us->pusb_dev, us->ep_in);
	else
		pipe = usb_sndbulkpipe(us->pusb_dev, us->ep_out);

	result = usb_stor_bulk_msg(us, data, pipe, len, &act_len);

	// if we stall, we need to clear it before we go on 
	if (result == -EPIPE) {
		US_DEBUGP("datafab_raw_bulk: EPIPE. clearing endpoint halt for"
			  " pipe 0x%x, stalled at %d bytes\n", pipe, act_len);
		usb_stor_clear_halt(us, pipe);
	}

	if (result) {
		// NAK - that means we've retried a few times already 
		if (result == -ETIMEDOUT) {
			US_DEBUGP("datafab_raw_bulk:  device NAKed\n");
			return US_BULK_TRANSFER_FAILED;
		}

		// -ECONNRESET -- we canceled this transfer
		if (result == -ECONNRESET) {
			US_DEBUGP("datafab_raw_bulk:  transfer aborted\n");
			return US_BULK_TRANSFER_ABORTED;
		}

		if (result == -EPIPE) {
			US_DEBUGP("datafab_raw_bulk:  output pipe stalled\n");
			return USB_STOR_TRANSPORT_FAILED;
		}

		// the catch-all case
		US_DEBUGP("datafab_raw_bulk:  unknown error\n");
		return US_BULK_TRANSFER_FAILED;
	}

	if (act_len != len) {
		US_DEBUGP("datafab_raw_bulk:  Warning. Transferred only %d bytes\n", act_len);
		return US_BULK_TRANSFER_SHORT;
	}

	US_DEBUGP("datafab_raw_bulk:  Transfered %d of %d bytes\n", act_len, len);
	return US_BULK_TRANSFER_GOOD;
}

static inline int datafab_bulk_read(struct us_data *us,
			            unsigned char *data, 
		                    unsigned int len)
{
	if (len == 0)
		return USB_STOR_TRANSPORT_GOOD;

	US_DEBUGP("datafab_bulk_read:  len = %d\n", len);
	return datafab_raw_bulk(SCSI_DATA_READ, us, data, len);
}


static inline int datafab_bulk_write(struct us_data *us,
			             unsigned char *data, 
		                     unsigned int len)
{
	if (len == 0)
		return USB_STOR_TRANSPORT_GOOD;

	US_DEBUGP("datafab_bulk_write:  len = %d\n", len);
	return datafab_raw_bulk(SCSI_DATA_WRITE, us, data, len);
}


static int datafab_read_data(struct us_data *us,
		             struct datafab_info *info,
		             u32 sector,
		             u32 sectors, 
		             unsigned char *dest, 
		             int use_sg)
{
	unsigned char command[8] = { 0, 0, 0, 0, 0, 0xE0, 0x20, 0x01 };
	unsigned char *buffer = NULL;
	unsigned char *ptr;
	unsigned char  thistime;
	struct scatterlist *sg = NULL;
	int totallen, len, result;
	int sg_idx = 0, current_sg_offset = 0;
	int transferred, rc;

	// we're working in LBA mode.  according to the ATA spec, 
	// we can support up to 28-bit addressing.  I don't know if Datafab
	// supports beyond 24-bit addressing.  It's kind of hard to test 
	// since it requires > 8GB CF card.
	//
	if (sectors > 0x0FFFFFFF)
		return USB_STOR_TRANSPORT_ERROR;

	if (info->lun == -1) {
		rc = datafab_determine_lun(us, info);
		if (rc != USB_STOR_TRANSPORT_GOOD)
			return rc;
	}

	command[5] += (info->lun << 4);

	// If we're using scatter-gather, we have to create a new
	// buffer to read all of the data in first, since a
	// scatter-gather buffer could in theory start in the middle
	// of a page, which would be bad. A developer who wants a
	// challenge might want to write a limited-buffer
	// version of this code.

	totallen = sectors * info->ssize;

	do {
		// loop, never allocate or transfer more than 64k at once (min(128k, 255*info->ssize) is the real limit)
		len = min_t(int, totallen, 65536);

		if (use_sg) {
			sg = (struct scatterlist *) dest;
			buffer = kmalloc(len, GFP_NOIO);
			if (buffer == NULL)
				return USB_STOR_TRANSPORT_ERROR;
			ptr = buffer;
		} else {
			ptr = dest;
		}

		thistime = (len / info->ssize) & 0xff;

		command[0] = 0;
		command[1] = thistime;
		command[2] = sector & 0xFF;
		command[3] = (sector >> 8) & 0xFF;
		command[4] = (sector >> 16) & 0xFF;
	
		command[5] |= (sector >> 24) & 0x0F;

		// send the command
		US_DEBUGP("datafab_read_data:  sending following command\n");
		datafab_dump_data(command, sizeof(command));

		result = datafab_bulk_write(us, command, sizeof(command));
		if (result != USB_STOR_TRANSPORT_GOOD) {
			if (use_sg)
				kfree(buffer);
			return result;
		}

		// read the result
		result = datafab_bulk_read(us, ptr, len);
		if (result != USB_STOR_TRANSPORT_GOOD) {
			if (use_sg)
				kfree(buffer);
			return result;
		}

		US_DEBUGP("datafab_read_data results:  %d bytes\n", len);
		// datafab_dump_data(ptr, len);

		sectors -= thistime;
		sector  += thistime;

		if (use_sg) {
			transferred = 0;
			while (sg_idx < use_sg && transferred < len) {
				if (len - transferred >= sg[sg_idx].length - current_sg_offset) {
					US_DEBUGP("datafab_read_data:  adding %d bytes to %d byte sg buffer\n", sg[sg_idx].length - current_sg_offset, sg[sg_idx].length);
					memcpy(sg[sg_idx].address + current_sg_offset,
					       buffer + transferred,
					       sg[sg_idx].length - current_sg_offset);
					transferred += sg[sg_idx].length - current_sg_offset;
					current_sg_offset = 0;
					// on to the next sg buffer
					++sg_idx;
				} else {
					US_DEBUGP("datafab_read_data:  adding %d bytes to %d byte sg buffer\n", len - transferred, sg[sg_idx].length);
					memcpy(sg[sg_idx].address + current_sg_offset,
					       buffer + transferred,
					       len - transferred);
					current_sg_offset += len - transferred;
					// this sg buffer is only partially full and we're out of data to copy in
					break;
				}
			}
			kfree(buffer);
		} else {
			dest += len;
		}

		totallen -= len;
	} while (totallen > 0);

	return USB_STOR_TRANSPORT_GOOD;
}


static int datafab_write_data(struct us_data *us,
		              struct datafab_info *info,
		              u32 sector,
		              u32 sectors, 
		              unsigned char *src, 
		              int use_sg)
{
	unsigned char command[8] = { 0, 0, 0, 0, 0, 0xE0, 0x30, 0x02 };
	unsigned char reply[2] = { 0, 0 };
	unsigned char *buffer = NULL;
	unsigned char *ptr;
	unsigned char thistime;
	struct scatterlist *sg = NULL;
	int totallen, len, result;
	int sg_idx = 0, current_sg_offset = 0;
	int transferred, rc;

	// we're working in LBA mode.  according to the ATA spec, 
	// we can support up to 28-bit addressing.  I don't know if Datafab
	// supports beyond 24-bit addressing.  It's kind of hard to test 
	// since it requires > 8GB CF card.
	//
	if (sectors > 0x0FFFFFFF)
		return USB_STOR_TRANSPORT_ERROR;

	if (info->lun == -1) {
		rc = datafab_determine_lun(us, info);
		if (rc != USB_STOR_TRANSPORT_GOOD)
			return rc;
	}

	command[5] += (info->lun << 4);

	// If we're using scatter-gather, we have to create a new
	// buffer to read all of the data in first, since a
	// scatter-gather buffer could in theory start in the middle
	// of a page, which would be bad. A developer who wants a
	// challenge might want to write a limited-buffer
	// version of this code.

	totallen = sectors * info->ssize;

	do {
		// loop, never allocate or transfer more than 64k at once (min(128k, 255*info->ssize) is the real limit)
		len = min_t(int, totallen, 65536);

		if (use_sg) {
			sg = (struct scatterlist *) src;
			buffer = kmalloc(len, GFP_NOIO);
			if (buffer == NULL)
				return USB_STOR_TRANSPORT_ERROR;
			ptr = buffer;

			memset(buffer, 0, len);

			// copy the data from the sg bufs into the big contiguous buf
			//
			transferred = 0;
			while (transferred < len) {
				if (len - transferred >= sg[sg_idx].length - current_sg_offset) {
					US_DEBUGP("datafab_write_data:  getting %d bytes from %d byte sg buffer\n", sg[sg_idx].length - current_sg_offset, sg[sg_idx].length);
					memcpy(ptr + transferred,
					       sg[sg_idx].address + current_sg_offset,
					       sg[sg_idx].length - current_sg_offset);
					transferred += sg[sg_idx].length - current_sg_offset;
					current_sg_offset = 0;
					// on to the next sg buffer
					++sg_idx;
				} else {
					US_DEBUGP("datafab_write_data:  getting %d bytes from %d byte sg buffer\n", len - transferred, sg[sg_idx].length);
					memcpy(ptr + transferred,
					       sg[sg_idx].address + current_sg_offset,
					       len - transferred);
					current_sg_offset += len - transferred;
					// we only copied part of this sg buffer
					break;
				}
			}
		} else {
			ptr = src;
		}

		thistime = (len / info->ssize) & 0xff;

		command[0] = 0;
		command[1] = thistime;
		command[2] = sector & 0xFF;
		command[3] = (sector >> 8) & 0xFF;
		command[4] = (sector >> 16) & 0xFF;

		command[5] |= (sector >> 24) & 0x0F;

		// send the command
		US_DEBUGP("datafab_write_data:  sending following command\n");
		datafab_dump_data(command, sizeof(command));

		result = datafab_bulk_write(us, command, sizeof(command));
		if (result != USB_STOR_TRANSPORT_GOOD) {
			if (use_sg)
				kfree(buffer);
			return result;
		}

		// send the data
		result = datafab_bulk_write(us, ptr, len);
		if (result != USB_STOR_TRANSPORT_GOOD) {
			if (use_sg)
				kfree(buffer);
			return result;
		}

		// read the result
		result = datafab_bulk_read(us, reply, sizeof(reply));
		if (result != USB_STOR_TRANSPORT_GOOD) {
			if (use_sg)
				kfree(buffer);
			return result;
		}

		if (reply[0] != 0x50 && reply[1] != 0) {
			US_DEBUGP("datafab_write_data:  Gah! write return code: %02x %02x\n", reply[0], reply[1]);
			if (use_sg)
				kfree(buffer);
			return USB_STOR_TRANSPORT_ERROR;
		}

		sectors -= thistime;
		sector  += thistime;

		if (use_sg) {
			kfree(buffer);
		} else {
			src += len;
		}

		totallen -= len;
	} while (totallen > 0);

	return USB_STOR_TRANSPORT_GOOD;
}


static int datafab_determine_lun(struct us_data *us,
				 struct datafab_info *info)
{
	// dual-slot readers can be thought of as dual-LUN devices.  we need to
	// determine which card slot is being used.  we'll send an IDENTIFY DEVICE
	// command and see which LUN responds...
	//
	// there might be a better way of doing this?
	//
	unsigned char command[8] = { 0, 1, 0, 0, 0, 0xa0, 0xec, 1 };
	unsigned char buf[512];
	int count = 0, rc;

	if (!us || !info)
		return USB_STOR_TRANSPORT_ERROR;

	US_DEBUGP("datafab_determine_lun:  locating...\n");

	// we'll try 10 times before giving up...
	//
	while (count++ < 10) {
		command[5] = 0xa0;

		rc = datafab_bulk_write(us, command, 8);
		if (rc != USB_STOR_TRANSPORT_GOOD) 
			return rc;

		rc = datafab_bulk_read(us, buf, sizeof(buf));
		if (rc == USB_STOR_TRANSPORT_GOOD) {
			info->lun = 0;
			return USB_STOR_TRANSPORT_GOOD;
		}

		command[5] = 0xb0;

		rc = datafab_bulk_write(us, command, 8);
		if (rc != USB_STOR_TRANSPORT_GOOD) 
			return rc;

		rc = datafab_bulk_read(us, buf, sizeof(buf));
		if (rc == USB_STOR_TRANSPORT_GOOD) {
			info->lun = 1;
			return USB_STOR_TRANSPORT_GOOD;
		}

                wait_ms(20);
	}

	return USB_STOR_TRANSPORT_FAILED;
}

static int datafab_id_device(struct us_data *us,
			     struct datafab_info *info)
{
	// this is a variation of the ATA "IDENTIFY DEVICE" command...according
	// to the ATA spec, 'Sector Count' isn't used but the Windows driver
	// sets this bit so we do too...
	//
	unsigned char command[8] = { 0, 1, 0, 0, 0, 0xa0, 0xec, 1 };
	unsigned char reply[512];
	int rc;

	if (!us || !info)
		return USB_STOR_TRANSPORT_ERROR;

	if (info->lun == -1) {
		rc = datafab_determine_lun(us, info);
		if (rc != USB_STOR_TRANSPORT_GOOD)
			return rc;
	}

	command[5] += (info->lun << 4);

	rc = datafab_bulk_write(us, command, 8);
	if (rc != USB_STOR_TRANSPORT_GOOD) 
		return rc;

	// we'll go ahead and extract the media capacity while we're here...
	//
	rc = datafab_bulk_read(us, reply, sizeof(reply));
	if (rc == USB_STOR_TRANSPORT_GOOD) {
		// capacity is at word offset 57-58
		//
		info->sectors = ((u32)(reply[117]) << 24) | 
				((u32)(reply[116]) << 16) |
				((u32)(reply[115]) <<  8) | 
				((u32)(reply[114])      );
	}
		
	return rc;
}


static int datafab_handle_mode_sense(struct us_data *us,
				     Scsi_Cmnd * srb, 
		                     unsigned char *ptr,
				     int sense_6)
{
	unsigned char mode_param_header[8] = {
		0, 0, 0, 0, 0, 0, 0, 0
	};
	unsigned char rw_err_page[12] = {
		0x1, 0xA, 0x21, 1, 0, 0, 0, 0, 1, 0, 0, 0
	};
	unsigned char cache_page[12] = {
		0x8, 0xA, 0x1, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	unsigned char rbac_page[12] = {
		0x1B, 0xA, 0, 0x81, 0, 0, 0, 0, 0, 0, 0, 0
	};
	unsigned char timer_page[8] = {
		0x1C, 0x6, 0, 0, 0, 0
	};
	unsigned char pc, page_code;
	unsigned short total_len = 0;
	unsigned short param_len, i = 0;

	// most of this stuff is just a hack to get things working.  the
	// datafab reader doesn't present a SCSI interface so we
	// fudge the SCSI commands...
	//
        
	if (sense_6)
		param_len = srb->cmnd[4];
	else
		param_len = ((u16) (srb->cmnd[7]) >> 8) | ((u16) (srb->cmnd[8]));

	pc = srb->cmnd[2] >> 6;
	page_code = srb->cmnd[2] & 0x3F;

	switch (pc) {
	   case 0x0:
		US_DEBUGP("datafab_handle_mode_sense:  Current values\n");
		break;
	   case 0x1:
		US_DEBUGP("datafab_handle_mode_sense:  Changeable values\n");
		break;
	   case 0x2:
		US_DEBUGP("datafab_handle_mode_sense:  Default values\n");
		break;
	   case 0x3:
		US_DEBUGP("datafab_handle_mode_sense:  Saves values\n");
		break;
	}

	mode_param_header[3] = 0x80;	// write enable

	switch (page_code) {
	   case 0x0:
		// vendor-specific mode
		return USB_STOR_TRANSPORT_ERROR;

	   case 0x1:
		total_len = sizeof(rw_err_page);
		mode_param_header[0] = total_len >> 8;
		mode_param_header[1] = total_len & 0xFF;
		mode_param_header[3] = 0x00;	// WP enable: 0x80

		memcpy(ptr, mode_param_header, sizeof(mode_param_header));
		i += sizeof(mode_param_header);
		memcpy(ptr + i, rw_err_page, sizeof(rw_err_page));
		break;

	   case 0x8:
		total_len = sizeof(cache_page);
		mode_param_header[0] = total_len >> 8;
		mode_param_header[1] = total_len & 0xFF;
		mode_param_header[3] = 0x00;	// WP enable: 0x80

		memcpy(ptr, mode_param_header, sizeof(mode_param_header));
		i += sizeof(mode_param_header);
		memcpy(ptr + i, cache_page, sizeof(cache_page));
		break;

	   case 0x1B:
		total_len = sizeof(rbac_page);
		mode_param_header[0] = total_len >> 8;
		mode_param_header[1] = total_len & 0xFF;
		mode_param_header[3] = 0x00;	// WP enable: 0x80

		memcpy(ptr, mode_param_header, sizeof(mode_param_header));
		i += sizeof(mode_param_header);
		memcpy(ptr + i, rbac_page, sizeof(rbac_page));
		break;

	   case 0x1C:
		total_len = sizeof(timer_page);
		mode_param_header[0] = total_len >> 8;
		mode_param_header[1] = total_len & 0xFF;
		mode_param_header[3] = 0x00;	// WP enable: 0x80

		memcpy(ptr, mode_param_header, sizeof(mode_param_header));
		i += sizeof(mode_param_header);
		memcpy(ptr + i, timer_page, sizeof(timer_page));
		break;

	   case 0x3F:		// retrieve all pages
		total_len = sizeof(timer_page) + sizeof(rbac_page) +
		    sizeof(cache_page) + sizeof(rw_err_page);
		mode_param_header[0] = total_len >> 8;
		mode_param_header[1] = total_len & 0xFF;
		mode_param_header[3] = 0x00;	// WP enable

		memcpy(ptr, mode_param_header, sizeof(mode_param_header));
		i += sizeof(mode_param_header);
		memcpy(ptr + i, timer_page, sizeof(timer_page));
		i += sizeof(timer_page);
		memcpy(ptr + i, rbac_page, sizeof(rbac_page));
		i += sizeof(rbac_page);
		memcpy(ptr + i, cache_page, sizeof(cache_page));
		i += sizeof(cache_page);
		memcpy(ptr + i, rw_err_page, sizeof(rw_err_page));
		break;
	}

	return USB_STOR_TRANSPORT_GOOD;
}

void datafab_info_destructor(void *extra)
{
	// this routine is a placeholder...
	// currently, we don't allocate any extra memory so we're okay
}


// Transport for the Datafab MDCFE-B
//
int datafab_transport(Scsi_Cmnd * srb, struct us_data *us)
{
	struct datafab_info *info;
	int rc;
	unsigned long block, blocks;
	unsigned char *ptr = NULL;
	unsigned char inquiry_reply[36] = {
		0x00, 0x80, 0x00, 0x01, 0x1F, 0x00, 0x00, 0x00
	};

	if (!us->extra) {
		us->extra = kmalloc(sizeof(struct datafab_info), GFP_NOIO);
		if (!us->extra) {
			US_DEBUGP("datafab_transport:  Gah! Can't allocate storage for Datafab info struct!\n");
			return USB_STOR_TRANSPORT_ERROR;
		}
		memset(us->extra, 0, sizeof(struct datafab_info));
		us->extra_destructor = datafab_info_destructor;
  		((struct datafab_info *)us->extra)->lun = -1;
	}

	info = (struct datafab_info *) (us->extra);
	ptr = (unsigned char *) srb->request_buffer;

	if (srb->cmnd[0] == INQUIRY) {
		US_DEBUGP("datafab_transport:  INQUIRY.  Returning bogus response");
		memset( inquiry_reply + 8, 0, 28 );
		fill_inquiry_response(us, inquiry_reply, 36);
		return USB_STOR_TRANSPORT_GOOD;
	}

	if (srb->cmnd[0] == READ_CAPACITY) {
		unsigned int max_sector;

		info->ssize = 0x200;  // hard coded 512 byte sectors as per ATA spec
		rc = datafab_id_device(us, info);
		if (rc != USB_STOR_TRANSPORT_GOOD)
			return rc;

		US_DEBUGP("datafab_transport:  READ_CAPACITY:  "
			  "%ld sectors, %ld bytes per sector\n",
			  info->sectors, info->ssize);

		// build the reply
		//
		max_sector = info->sectors - 1;
		ptr[0] = (max_sector >> 24) & 0xFF;
		ptr[1] = (max_sector >> 16) & 0xFF;
		ptr[2] = (max_sector >> 8) & 0xFF;
		ptr[3] = (max_sector) & 0xFF;

		ptr[4] = (info->ssize >> 24) & 0xFF;
		ptr[5] = (info->ssize >> 16) & 0xFF;
		ptr[6] = (info->ssize >> 8) & 0xFF;
		ptr[7] = (info->ssize) & 0xFF;

		return USB_STOR_TRANSPORT_GOOD;
	}

	if (srb->cmnd[0] == MODE_SELECT_10) {
		US_DEBUGP("datafab_transport:  Gah! MODE_SELECT_10.\n");
		return USB_STOR_TRANSPORT_ERROR;
	}

	// don't bother implementing READ_6 or WRITE_6.  Just set MODE_XLATE and
	// let the usb storage code convert to READ_10/WRITE_10
	//
	if (srb->cmnd[0] == READ_10) {
		block = ((u32)(srb->cmnd[2]) << 24) | ((u32)(srb->cmnd[3]) << 16) |
		        ((u32)(srb->cmnd[4]) <<  8) | ((u32)(srb->cmnd[5]));

		blocks = ((u32)(srb->cmnd[7]) << 8) | ((u32)(srb->cmnd[8]));

		US_DEBUGP("datafab_transport:  READ_10: read block 0x%04lx  count %ld\n", block, blocks);
		return datafab_read_data(us, info, block, blocks, ptr, srb->use_sg);
	}

	if (srb->cmnd[0] == READ_12) {
		// we'll probably never see a READ_12 but we'll do it anyway...
		//
		block = ((u32)(srb->cmnd[2]) << 24) | ((u32)(srb->cmnd[3]) << 16) |
		        ((u32)(srb->cmnd[4]) <<  8) | ((u32)(srb->cmnd[5]));

		blocks = ((u32)(srb->cmnd[6]) << 24) | ((u32)(srb->cmnd[7]) << 16) |
		         ((u32)(srb->cmnd[8]) <<  8) | ((u32)(srb->cmnd[9]));

		US_DEBUGP("datafab_transport:  READ_12: read block 0x%04lx  count %ld\n", block, blocks);
		return datafab_read_data(us, info, block, blocks, ptr, srb->use_sg);
	}

	if (srb->cmnd[0] == WRITE_10) {
		block = ((u32)(srb->cmnd[2]) << 24) | ((u32)(srb->cmnd[3]) << 16) |
		        ((u32)(srb->cmnd[4]) <<  8) | ((u32)(srb->cmnd[5]));

		blocks = ((u32)(srb->cmnd[7]) << 8) | ((u32)(srb->cmnd[8]));

		US_DEBUGP("datafab_transport:  WRITE_10: write block 0x%04lx  count %ld\n", block, blocks);
		return datafab_write_data(us, info, block, blocks, ptr, srb->use_sg);
	}

	if (srb->cmnd[0] == WRITE_12) {
		// we'll probably never see a WRITE_12 but we'll do it anyway...
		//
		block = ((u32)(srb->cmnd[2]) << 24) | ((u32)(srb->cmnd[3]) << 16) |
		        ((u32)(srb->cmnd[4]) <<  8) | ((u32)(srb->cmnd[5]));

		blocks = ((u32)(srb->cmnd[6]) << 24) | ((u32)(srb->cmnd[7]) << 16) |
		         ((u32)(srb->cmnd[8]) <<  8) | ((u32)(srb->cmnd[9]));

		US_DEBUGP("datafab_transport:  WRITE_12: write block 0x%04lx  count %ld\n", block, blocks);
		return datafab_write_data(us, info, block, blocks, ptr, srb->use_sg);
	}

	if (srb->cmnd[0] == TEST_UNIT_READY) {
		US_DEBUGP("datafab_transport:  TEST_UNIT_READY.\n");
		return datafab_id_device(us, info);
	}

	if (srb->cmnd[0] == REQUEST_SENSE) {
		US_DEBUGP("datafab_transport:  REQUEST_SENSE.  Returning faked response\n");

		// this response is pretty bogus right now.  eventually if necessary
		// we can set the correct sense data.  so far though it hasn't been
		// necessary
		//
		ptr[0] = 0xF0;
		ptr[2] = info->sense_key;
		ptr[7] = 11;
		ptr[12] = info->sense_asc;
		ptr[13] = info->sense_ascq;

		return USB_STOR_TRANSPORT_GOOD;
	}

	if (srb->cmnd[0] == MODE_SENSE) {
		US_DEBUGP("datafab_transport:  MODE_SENSE_6 detected\n");
		return datafab_handle_mode_sense(us, srb, ptr, TRUE);
	}

	if (srb->cmnd[0] == MODE_SENSE_10) {
		US_DEBUGP("datafab_transport:  MODE_SENSE_10 detected\n");
		return datafab_handle_mode_sense(us, srb, ptr, FALSE);
	}

	if (srb->cmnd[0] == ALLOW_MEDIUM_REMOVAL) {
		// sure.  whatever.  not like we can stop the user from
		// popping the media out of the device (no locking doors, etc)
		//
		return USB_STOR_TRANSPORT_GOOD;
	}

	if (srb->cmnd[0] == START_STOP) {
		/* this is used by sd.c'check_scsidisk_media_change to detect
		   media change */
		US_DEBUGP("datafab_transport:  START_STOP.\n");
		/* the first datafab_id_device after a media change returns
		   an error (determined experimentally) */
		rc = datafab_id_device(us, info);
		if (rc == USB_STOR_TRANSPORT_GOOD) {
			info->sense_key = NO_SENSE;
			srb->result = SUCCESS;
		} else {
			info->sense_key = UNIT_ATTENTION;
			srb->result = CHECK_CONDITION;
		}
		return rc;
        }

	US_DEBUGP("datafab_transport:  Gah! Unknown command: %d (0x%x)\n", srb->cmnd[0], srb->cmnd[0]);
	return USB_STOR_TRANSPORT_ERROR;
}
