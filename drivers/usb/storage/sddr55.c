/* Driver for SanDisk SDDR-55 SmartMedia reader
 *
 * $Id:$
 *
 * SDDR55 driver v0.1:
 *
 * First release
 *
 * Current development and maintenance by:
 *   (c) 2002 Simon Munton
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
#include "sddr55.h"

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>

#define short_pack(lsb,msb) ( ((u16)(lsb)) | ( ((u16)(msb))<<8 ) )
#define LSB_of(s) ((s)&0xFF)
#define MSB_of(s) ((s)>>8)
#define PAGESIZE  512

#define set_sense_info(sk, asc, ascq)	\
    do {				\
	info->sense_data[2] = sk;	\
	info->sense_data[12] = asc;	\
	info->sense_data[13] = ascq;	\
	} while (0)


struct sddr55_card_info {
	unsigned long	capacity;	/* Size of card in bytes */
	int		max_log_blks;	/* maximum number of logical blocks */
	int		pageshift;	/* log2 of pagesize */
	int		smallpageshift;	/* 1 if pagesize == 256 */
	int		blocksize;	/* Size of block in pages */
	int		blockshift;	/* log2 of blocksize */
	int		blockmask;	/* 2^blockshift - 1 */
	int		read_only;	/* non zero if card is write protected */
	int		force_read_only;	/* non zero if we find a map error*/
	int		*lba_to_pba;	/* logical to physical map */
	int		*pba_to_lba;	/* physical to logical map */
	int		fatal_error;	/* set if we detect something nasty */
	unsigned long 	last_access;	/* number of jiffies since we last talked to device */
	unsigned char   sense_data[18];
};


#define NOT_ALLOCATED		0xffffffff
#define BAD_BLOCK		0xffff
#define CIS_BLOCK		0x400
#define UNUSED_BLOCK		0x3ff



static int sddr55_raw_bulk(struct us_data *us, 
		int direction,
		unsigned char *data,
		unsigned int len) {

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
		usb_clear_halt(us->pusb_dev, pipe);
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
			return US_BULK_TRANSFER_FAILED;
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

	US_DEBUGP("Transferred %d of %d bytes\n", act_len, len);

	return US_BULK_TRANSFER_GOOD;
}

/*
 * Note: direction must be set if command_len == 0.
 */

static int sddr55_bulk_transport(struct us_data *us,
			  int direction,
			  unsigned char *data,
			  unsigned int len) {

	int result = USB_STOR_TRANSPORT_GOOD;
	struct sddr55_card_info *info = (struct sddr55_card_info *)us->extra;

	if (len==0)
		return USB_STOR_TRANSPORT_GOOD;

	info->last_access = jiffies;

#ifdef CONFIG_USB_STORAGE_DEBUG
	if (direction == SCSI_DATA_WRITE) {
		int i;
		char string[64];

		/* Debug-print the first 48 bytes of the write transfer */

		strcpy(string, "wr: ");
		for (i=0; i<len && i<48; i++) {
			sprintf(string+strlen(string), "%02X ",
			  data[i]);
			if ((i%16)==15) {
				US_DEBUGP("%s\n", string);
				strcpy(string, "wr: ");
			}
		}
		if ((i%16)!=0)
			US_DEBUGP("%s\n", string);
	}
#endif

	/* transfer the data */

	US_DEBUGP("SCM data %s transfer %d\n",
		  ( direction==SCSI_DATA_READ ? "in" : "out"),
		  len);

	result = sddr55_raw_bulk(us, direction, data, len);

#ifdef CONFIG_USB_STORAGE_DEBUG
	if (direction == SCSI_DATA_READ) {
		int i;
		char string[64];

		/* Debug-print the first 48 bytes of the read transfer */

		strcpy(string, "rd: ");
		for (i=0; i<len && i<48; i++) {
			sprintf(string+strlen(string), "%02X ",
			  data[i]);
			if ((i%16)==15) {
				US_DEBUGP("%s\n", string);
				strcpy(string, "rd: ");
			}
		}
		if ((i%16)!=0)
			US_DEBUGP("%s\n", string);
	}
#endif

	return result;
}


/* check if card inserted, if there is, update read_only status
 * return non zero if no card
 */

static int sddr55_status(struct us_data *us)
{
	int result;
	unsigned char command[8] = {
		0, 0, 0, 0, 0, 0xb0, 0, 0x80
	};
	unsigned char status[8];
	struct sddr55_card_info *info = (struct sddr55_card_info *)us->extra;

	/* send command */
	result = sddr55_bulk_transport(us,
		SCSI_DATA_WRITE, command, 8);

	US_DEBUGP("Result for send_command in status %d\n",
		result);

	if (result != US_BULK_TRANSFER_GOOD) {
		set_sense_info (4, 0, 0);	/* hardware error */
		return result;
	}

	result = sddr55_bulk_transport(us,
		SCSI_DATA_READ, status,	4);

	/* expect to get short transfer if no card fitted */
	if (result == US_BULK_TRANSFER_SHORT) {
		/* had a short transfer, no card inserted, free map memory */
		if (info->lba_to_pba)
			kfree(info->lba_to_pba);
		if (info->pba_to_lba)
			kfree(info->pba_to_lba);
		info->lba_to_pba = NULL;
		info->pba_to_lba = NULL;

		info->fatal_error = 0;
		info->force_read_only = 0;

		set_sense_info (2, 0x3a, 0);	/* not ready, medium not present */
		return result;
	}

	if (result != US_BULK_TRANSFER_GOOD) {
		set_sense_info (4, 0, 0);	/* hardware error */
		return result;
	}
	
	/* check write protect status */
	info->read_only = (status[0] & 0x20);

	/* now read status */
	result = sddr55_bulk_transport(us,
		SCSI_DATA_READ, status,	2);

	if (result != US_BULK_TRANSFER_GOOD) {
		set_sense_info (4, 0, 0);	/* hardware error */
	}

	return result;
}


static int sddr55_read_data(struct us_data *us,
		unsigned int lba,
		unsigned int page,
		unsigned short sectors,
		unsigned char *content,
		int use_sg) {

	int result;
	unsigned char command[8] = {
		0, 0, 0, 0, 0, 0xb0, 0, 0x85
	};
	unsigned char status[8];
	struct sddr55_card_info *info = (struct sddr55_card_info *)us->extra;

	unsigned int pba;
	unsigned long address;

	unsigned short pages;
	unsigned char *buffer = NULL;
	unsigned char *ptr;
	struct scatterlist *sg = NULL;
	int i;
	int len;
	int transferred;

	// If we're using scatter-gather, we have to create a new
	// buffer to read all of the data in first, since a
	// scatter-gather buffer could in theory start in the middle
	// of a page, which would be bad. A developer who wants a
	// challenge might want to write a limited-buffer
	// version of this code.

	len = sectors * PAGESIZE;

	if (use_sg) {
		sg = (struct scatterlist *)content;
		buffer = kmalloc(len, GFP_NOIO);
		if (buffer == NULL)
			return USB_STOR_TRANSPORT_ERROR;
		ptr = buffer;
	} else
		ptr = content;

	// This could be made much more efficient by checking for
	// contiguous LBA's. Another exercise left to the student.

	while (sectors>0) {

		/* have we got to end? */
		if (lba >= info->max_log_blks)
			break;

		pba = info->lba_to_pba[lba];

		// Read as many sectors as possible in this block

		pages = info->blocksize - page;
		if (pages > (sectors << info->smallpageshift))
			pages = (sectors << info->smallpageshift);

		US_DEBUGP("Read %02X pages, from PBA %04X"
			" (LBA %04X) page %02X\n",
			pages, pba, lba, page);

		if (pba == NOT_ALLOCATED) {
			/* no pba for this lba, fill with zeroes */
			memset (ptr, 0, pages << info->pageshift);
		} else {

			address = (pba << info->blockshift) + page;

			command[1] = LSB_of(address>>16);
			command[2] = LSB_of(address>>8);
			command[3] = LSB_of(address);

			command[6] = LSB_of(pages << (1 - info->smallpageshift));

			/* send command */
			result = sddr55_bulk_transport(us,
				SCSI_DATA_WRITE, command, 8);

			US_DEBUGP("Result for send_command in read_data %d\n",
				result);

			if (result != US_BULK_TRANSFER_GOOD) {
				if (use_sg)
					kfree(buffer);
				return result;
			}

			/* read data */
			result = sddr55_bulk_transport(us,
				SCSI_DATA_READ, ptr,
				pages<<info->pageshift);

			if (result != US_BULK_TRANSFER_GOOD) {
				if (use_sg)
					kfree(buffer);
				return result;
			}

			/* now read status */
			result = sddr55_bulk_transport(us,
				SCSI_DATA_READ, status, 2);

			if (result != US_BULK_TRANSFER_GOOD) {
				if (use_sg)
					kfree(buffer);
				return result;
			}

			/* check status for error */
			if (status[0] == 0xff && status[1] == 0x4) {
				set_sense_info (3, 0x11, 0);
				if (use_sg)
					kfree(buffer);

				return USB_STOR_TRANSPORT_FAILED;
			}

		}

		page = 0;
		lba++;
		sectors -= pages >> info->smallpageshift;
		ptr += (pages << info->pageshift);
	}

	if (use_sg) {
		transferred = 0;
		for (i=0; i<use_sg && transferred<len; i++) {
			memcpy(sg[i].address, buffer+transferred,
				len-transferred > sg[i].length ?
					sg[i].length : len-transferred);
			transferred += sg[i].length;
		}
		kfree(buffer);
	}

	return USB_STOR_TRANSPORT_GOOD;
}

static int sddr55_write_data(struct us_data *us,
		unsigned int lba,
		unsigned int page,
		unsigned short sectors,
		unsigned char *content,
		int use_sg) {

	int result;
	unsigned char command[8] = {
		0, 0, 0, 0, 0, 0xb0, 0, 0x86
	};
	unsigned char status[8];
	struct sddr55_card_info *info = (struct sddr55_card_info *)us->extra;

	unsigned int pba;
	unsigned int new_pba;
	unsigned long address;

	unsigned short pages;
	unsigned char *buffer = NULL;
	unsigned char *ptr;
	struct scatterlist *sg = NULL;
	int i;
	int len;
	int transferred;

	/* check if we are allowed to write */
	if (info->read_only || info->force_read_only) {
		set_sense_info (7, 0x27, 0);	/* read only */
		return USB_STOR_TRANSPORT_FAILED;
	}

	// If we're using scatter-gather, we have to create a new
	// buffer to write all of the data in first, since a
	// scatter-gather buffer could in theory start in the middle
	// of a page, which would be bad. A developer who wants a
	// challenge might want to write a limited-buffer
	// version of this code.

	len = sectors * PAGESIZE;

	if (use_sg) {
		sg = (struct scatterlist *)content;
		buffer = kmalloc(len, GFP_NOIO);
		if (buffer == NULL)
			return USB_STOR_TRANSPORT_ERROR;

		transferred = 0;
		for (i=0; i<use_sg && transferred<len; i++) {
			memcpy(buffer+transferred, sg[i].address,
				len-transferred > sg[i].length ?
					sg[i].length : len-transferred);
			transferred += sg[i].length;
		}

		ptr = buffer;
	} else
		ptr = content;

	while (sectors > 0) {

		/* have we got to end? */
		if (lba >= info->max_log_blks)
			break;

		pba = info->lba_to_pba[lba];

		// Write as many sectors as possible in this block

		pages = info->blocksize - page;
		if (pages > (sectors << info->smallpageshift))
			pages = (sectors << info->smallpageshift);

		US_DEBUGP("Write %02X pages, to PBA %04X"
			" (LBA %04X) page %02X\n",
			pages, pba, lba, page);
			
		command[4] = 0;

		if (pba == NOT_ALLOCATED) {
			/* no pba allocated for this lba, find a free pba to use */

			int max_pba = (info->max_log_blks / 250 ) * 256;
			int found_count = 0;
			int found_pba = -1;

			/* set pba to first block in zone lba is in */
			pba = (lba / 1000) * 1024;

			US_DEBUGP("No PBA for LBA %04X\n",lba);

			if (max_pba > 1024)
				max_pba = 1024;

			/* scan through the map lookiong for an unused block
			 * leave 16 unused blocks at start (or as many as possible)
			 * since the sddr55 seems to reuse a used block when it shouldn't
			 * if we don't leave space */
			for (i = 0; i < max_pba; i++, pba++) {
				if (info->pba_to_lba[pba] == UNUSED_BLOCK) {
					found_pba = pba;
					if (found_count++ > 16)
						break;
				}
			}

			pba = found_pba;

			if (pba == -1) {
				/* oh dear, couldn't find an unallocated block */
				US_DEBUGP("Couldn't find unallocated block\n");

				set_sense_info (3, 0x31, 0);	/* medium error */

				if (use_sg)
					kfree(buffer);

				return USB_STOR_TRANSPORT_FAILED;
			}

			US_DEBUGP("Allocating PBA %04X for LBA %04X\n", pba, lba);

			/* set writing to unallocated block flag */
			command[4] = 0x40;
		}

		address = (pba << info->blockshift) + page;

		command[1] = LSB_of(address>>16);
		command[2] = LSB_of(address>>8); 
		command[3] = LSB_of(address);

		/* set the lba into the command, modulo 1000 */
		command[0] = LSB_of(lba % 1000);
		command[6] = MSB_of(lba % 1000);

		command[4] |= LSB_of(pages >> info->smallpageshift);

		/* send command */
		result = sddr55_bulk_transport(us,
			SCSI_DATA_WRITE, command, 8);

		if (result != US_BULK_TRANSFER_GOOD) {
			US_DEBUGP("Result for send_command in write_data %d\n",
			result);

			set_sense_info (3, 0x3, 0);	/* peripheral write error */
			
			if (use_sg)
				kfree(buffer);
			return result;
		}

		/* send the data */
		result = sddr55_bulk_transport(us,
			SCSI_DATA_WRITE, ptr,
			pages<<info->pageshift);

		if (result != US_BULK_TRANSFER_GOOD) {
			US_DEBUGP("Result for send_data in write_data %d\n",
			result);

			set_sense_info (3, 0x3, 0);	/* peripheral write error */

			if (use_sg)
				kfree(buffer);
			return result;
		}

		/* now read status */
		result = sddr55_bulk_transport(us,
			SCSI_DATA_READ, status,	6);

		if (result != US_BULK_TRANSFER_GOOD) {
			US_DEBUGP("Result for get_status in write_data %d\n",
			result);

			set_sense_info (3, 0x3, 0);	/* peripheral write error */

			if (use_sg)
				kfree(buffer);
			return result;
		}

		new_pba = (status[3] + (status[4] << 8) + (status[5] << 16)) >> info->blockshift;

		/* check status for error */
		if (status[0] == 0xff && status[1] == 0x4) {
			set_sense_info (3, 0x0c, 0);
			if (use_sg)
				kfree(buffer);

			info->pba_to_lba[new_pba] = BAD_BLOCK;

			return USB_STOR_TRANSPORT_FAILED;
		}

		US_DEBUGP("Updating maps for LBA %04X: old PBA %04X, new PBA %04X\n",
			lba, pba, new_pba);

		/* update the lba<->pba maps, note new_pba might be the same as pba */
		info->lba_to_pba[lba] = new_pba;
		info->pba_to_lba[pba] = UNUSED_BLOCK;

		/* check that new_pba wasn't already being used */
		if (info->pba_to_lba[new_pba] != UNUSED_BLOCK) {
			printk(KERN_ERR "sddr55 error: new PBA %04X already in use for LBA %04X\n",
				new_pba, info->pba_to_lba[new_pba]);
			info->fatal_error = 1;
			set_sense_info (3, 0x31, 0);
			if (use_sg)
				kfree(buffer);

			return USB_STOR_TRANSPORT_FAILED;
		}

		/* update the pba<->lba maps for new_pba */
		info->pba_to_lba[new_pba] = lba % 1000;

		page = 0;
		lba++;
		sectors -= pages >> info->smallpageshift;
		ptr += (pages << info->pageshift);
	}

	if (use_sg) {
		kfree(buffer);
	}

	return USB_STOR_TRANSPORT_GOOD;
}

static int sddr55_read_deviceID(struct us_data *us,
		unsigned char *manufacturerID,
		unsigned char *deviceID) {

	int result;
	unsigned char command[8] = {
		0, 0, 0, 0, 0, 0xb0, 0, 0x84
	};
	unsigned char content[64];

	result = sddr55_bulk_transport(us, SCSI_DATA_WRITE, command, 8);

	US_DEBUGP("Result of send_control for device ID is %d\n",
		result);

	if (result != US_BULK_TRANSFER_GOOD)
		return result;

	result = sddr55_bulk_transport(us,
		SCSI_DATA_READ, content, 4);

	if (result != US_BULK_TRANSFER_GOOD)
		return result;

	*manufacturerID = content[0];
	*deviceID = content[1];

	if (content[0] != 0xff)	{
    		result = sddr55_bulk_transport(us,
			SCSI_DATA_READ, content, 2);
	}

	return result;
}


int sddr55_reset(struct us_data *us) {
	return 0;
}


static unsigned long sddr55_get_capacity(struct us_data *us) {

	unsigned char manufacturerID;
	unsigned char deviceID;
	int result;
	struct sddr55_card_info *info = (struct sddr55_card_info *)us->extra;

	US_DEBUGP("Reading capacity...\n");

	result = sddr55_read_deviceID(us,
		&manufacturerID,
		&deviceID);

	US_DEBUGP("Result of read_deviceID is %d\n",
		result);

	if (result != US_BULK_TRANSFER_GOOD)
		return 0;

	US_DEBUGP("Device ID = %02X\n", deviceID);
	US_DEBUGP("Manuf  ID = %02X\n", manufacturerID);

	info->pageshift = 9;
	info->smallpageshift = 0;
	info->blocksize = 16;
	info->blockshift = 4;
	info->blockmask = 15;

	switch (deviceID) {

	case 0x6e: // 1MB
	case 0xe8:
	case 0xec:
		info->pageshift = 8;
		info->smallpageshift = 1;
		return 0x00100000;

	case 0xea: // 2MB
	case 0x64:
		info->pageshift = 8;
		info->smallpageshift = 1;
	case 0x5d: // 5d is a ROM card with pagesize 512.
		return 0x00200000;

	case 0xe3: // 4MB
	case 0xe5:
	case 0x6b:
	case 0xd5:
		return 0x00400000;

	case 0xe6: // 8MB
	case 0xd6:
		return 0x00800000;

	case 0x73: // 16MB
		info->blocksize = 32;
		info->blockshift = 5;
		info->blockmask = 31;
		return 0x01000000;

	case 0x75: // 32MB
		info->blocksize = 32;
		info->blockshift = 5;
		info->blockmask = 31;
		return 0x02000000;

	case 0x76: // 64MB
		info->blocksize = 32;
		info->blockshift = 5;
		info->blockmask = 31;
		return 0x04000000;

	case 0x79: // 128MB
		info->blocksize = 32;
		info->blockshift = 5;
		info->blockmask = 31;
		return 0x08000000;

	default: // unknown
		return 0;

	}
}

static int sddr55_read_map(struct us_data *us) {

	struct sddr55_card_info *info = (struct sddr55_card_info *)(us->extra);
	int numblocks;
	unsigned char *buffer;
	unsigned char command[8] = { 0, 0, 0, 0, 0, 0xb0, 0, 0x8a};	
	int i;
	unsigned short lba;
	unsigned short max_lba;
	int result;

	if (!info->capacity)
		return -1;

	numblocks = info->capacity >> (info->blockshift + info->pageshift);
	
	buffer = kmalloc( numblocks * 2, GFP_NOIO );
	
	if (!buffer)
		return -1;

	command[6] = numblocks * 2 / 256;

	result = sddr55_bulk_transport(us, SCSI_DATA_WRITE, command, 8);

	if ( result != US_BULK_TRANSFER_GOOD) {
		kfree (buffer);
		return -1;
	}

	result = sddr55_bulk_transport(us, SCSI_DATA_READ, buffer, numblocks * 2);

	if ( result != US_BULK_TRANSFER_GOOD) {
		kfree (buffer);
		return -1;
	}

	result = sddr55_bulk_transport(us, SCSI_DATA_READ, command, 2);

	if ( result != US_BULK_TRANSFER_GOOD) {
		kfree (buffer);
		return -1;
	}

	if (info->lba_to_pba)
		kfree(info->lba_to_pba);
	if (info->pba_to_lba)
		kfree(info->pba_to_lba);
	info->lba_to_pba = kmalloc(numblocks*sizeof(int), GFP_NOIO);
	info->pba_to_lba = kmalloc(numblocks*sizeof(int), GFP_NOIO);

	if (info->lba_to_pba == NULL || info->pba_to_lba == NULL) {
		if (info->lba_to_pba != NULL)
			kfree(info->lba_to_pba);
		if (info->pba_to_lba != NULL)
			kfree(info->pba_to_lba);
		info->lba_to_pba = NULL;
		info->pba_to_lba = NULL;
		kfree(buffer);
		return -1;
	}

	memset(info->lba_to_pba, 0xff, numblocks*sizeof(int));
	memset(info->pba_to_lba, 0xff, numblocks*sizeof(int));

	/* set maximum lba */
	max_lba = info->max_log_blks;
	if (max_lba > 1000)
		max_lba = 1000;

	// Each block is 64 bytes of control data, so block i is located in
	// scatterlist block i*64/128k = i*(2^6)*(2^-17) = i*(2^-11)

	for (i=0; i<numblocks; i++) {
		int zone = i / 1024;

		lba = short_pack(buffer[i * 2], buffer[i * 2 + 1]);

			/* Every 1024 physical blocks ("zone"), the LBA numbers
			 * go back to zero, but are within a higher
			 * block of LBA's. Also, there is a maximum of
			 * 1000 LBA's per zone. In other words, in PBA
			 * 1024-2047 you will find LBA 0-999 which are
			 * really LBA 1000-1999. Yes, this wastes 24
			 * physical blocks per zone. Go figure. 
			 * These devices can have blocks go bad, so there
			 * are 24 spare blocks to use when blocks do go bad.
			 */

			/* SDDR55 returns 0xffff for a bad block, and 0x400 for the 
			 * CIS block. (Is this true for cards 8MB or less??)
			 * Record these in the physical to logical map
			 */ 

		info->pba_to_lba[i] = lba;

		if (lba >= max_lba) {
			continue;
		}
		
		if (info->lba_to_pba[lba + zone * 1000] != NOT_ALLOCATED &&
		    !info->force_read_only) {
			printk("sddr55: map inconsistency at LBA %04X\n", lba + zone * 1000);
			info->force_read_only = 1;
		}

		if (lba<0x10 || (lba>=0x3E0 && lba<0x3EF))
			US_DEBUGP("LBA %04X <-> PBA %04X\n", lba, i);

		info->lba_to_pba[lba + zone * 1000] = i;
	}

	kfree(buffer);
	return 0;
}


static void sddr55_card_info_destructor(void *extra) {
	struct sddr55_card_info *info = (struct sddr55_card_info *)extra;

	if (!extra)
		return;

	if (info->lba_to_pba)
		kfree(info->lba_to_pba);
	if (info->pba_to_lba)
		kfree(info->pba_to_lba);
}


/*
 * Transport for the Sandisk SDDR-55
 */
int sddr55_transport(Scsi_Cmnd *srb, struct us_data *us)
{
	int result;
	int i;
	unsigned char inquiry_response[36] = {
		0x00, 0x80, 0x00, 0x02, 0x1F, 0x00, 0x00, 0x00
	};
	unsigned char mode_page_01[16] = { // write-protected for now
		0x03, 0x00, 0x80, 0x00,
		0x01, 0x0A,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	unsigned char *ptr;
	unsigned long capacity;
	unsigned int lba;
	unsigned int pba;
	unsigned int page;
	unsigned short pages;
	struct sddr55_card_info *info;

	if (!us->extra) {
		us->extra = kmalloc(
			sizeof(struct sddr55_card_info), GFP_NOIO);
		if (!us->extra)
			return USB_STOR_TRANSPORT_ERROR;
		memset(us->extra, 0, sizeof(struct sddr55_card_info));
		us->extra_destructor = sddr55_card_info_destructor;
	}

	info = (struct sddr55_card_info *)(us->extra);

	ptr = (unsigned char *)srb->request_buffer;

	if (srb->cmnd[0] == REQUEST_SENSE) {
		i = srb->cmnd[4];

		if (i > sizeof info->sense_data)
			i = sizeof info->sense_data;


		US_DEBUGP("SDDR55: request sense %02x/%02x/%02x\n", info->sense_data[2], info->sense_data[12], info->sense_data[13]);

		info->sense_data[0] = 0x70;
		info->sense_data[7] = 10;

		memcpy (ptr, info->sense_data, i);
		memset (info->sense_data, 0, sizeof info->sense_data);

		return USB_STOR_TRANSPORT_GOOD;
	}

	memset (info->sense_data, 0, sizeof info->sense_data);

	/* Dummy up a response for INQUIRY since SDDR55 doesn't
	   respond to INQUIRY commands */

	if (srb->cmnd[0] == INQUIRY) {
		memset(inquiry_response+8, 0, 28);
		fill_inquiry_response(us, inquiry_response, 36);
		return USB_STOR_TRANSPORT_GOOD;
	}

	/* only check card status if the map isn't allocated, ie no card seen yet
	 * or if it's been over half a second since we last accessed it
	 */
	if (info->lba_to_pba == NULL || time_after(jiffies, info->last_access + HZ/2)) {

		/* check to see if a card is fitted */
		result = sddr55_status (us);
		if (result) {
			result = sddr55_status (us);
			if (!result) {
			set_sense_info (6, 0x28, 0);	/* new media, set unit attention, not ready to ready */
			}
			return USB_STOR_TRANSPORT_FAILED;
		}
	}

	/* if we detected a problem with the map when writing, don't allow any more access */
	if (info->fatal_error) {

		set_sense_info (3, 0x31, 0);
		return USB_STOR_TRANSPORT_FAILED;
	}

	if (srb->cmnd[0] == READ_CAPACITY) {

		capacity = sddr55_get_capacity(us);

		if (!capacity) {
			set_sense_info (3, 0x30, 0);	/* incompatible medium */
			return USB_STOR_TRANSPORT_FAILED;
		}

		info->capacity = capacity;

                /* figure out the maximum logical block number, allowing for the fact
                 * that only 250 out of every 256 are used */
		info->max_log_blks = ((info->capacity >> (info->pageshift + info->blockshift)) / 256) * 250;

		/* Last page in the card, adjust as we only use 250 out of every 256 pages */
		capacity = (capacity / 256) * 250;

		capacity /= PAGESIZE;
		capacity--;

		ptr[0] = MSB_of(capacity>>16);
		ptr[1] = LSB_of(capacity>>16);
		ptr[2] = MSB_of(capacity&0xFFFF);
		ptr[3] = LSB_of(capacity&0xFFFF);

		// The page size

		ptr[4] = MSB_of(PAGESIZE>>16);
		ptr[5] = LSB_of(PAGESIZE>>16);
		ptr[6] = MSB_of(PAGESIZE&0xFFFF);
		ptr[7] = LSB_of(PAGESIZE&0xFFFF);

		sddr55_read_map(us);

		return USB_STOR_TRANSPORT_GOOD;
	}

	if (srb->cmnd[0] == MODE_SENSE) {

		mode_page_01[2] = (info->read_only || info->force_read_only) ? 0x80 : 0;

		if ( (srb->cmnd[2] & 0x3F) == 0x01 ) {

			US_DEBUGP(
			  "SDDR55: Dummy up request for mode page 1\n");

			if (ptr==NULL || 
			  srb->request_bufflen<sizeof(mode_page_01)) {
				set_sense_info (5, 0x24, 0);	/* invalid field in command */
				return USB_STOR_TRANSPORT_FAILED;
			}

			memcpy(ptr, mode_page_01, sizeof(mode_page_01));
			return USB_STOR_TRANSPORT_GOOD;

		} else if ( (srb->cmnd[2] & 0x3F) == 0x3F ) {

			US_DEBUGP(
			  "SDDR55: Dummy up request for all mode pages\n");

			if (ptr==NULL || 
			  srb->request_bufflen<sizeof(mode_page_01)) {
				set_sense_info (5, 0x24, 0);	/* invalid field in command */
				return USB_STOR_TRANSPORT_FAILED;
			}

			memcpy(ptr, mode_page_01, sizeof(mode_page_01));
			return USB_STOR_TRANSPORT_GOOD;
		}

		set_sense_info (5, 0x24, 0);	/* invalid field in command */

		return USB_STOR_TRANSPORT_FAILED;
	}

	if (srb->cmnd[0] == ALLOW_MEDIUM_REMOVAL) {

		US_DEBUGP(
		  "SDDR55: %s medium removal. Not that I can do"
		  " anything about it...\n",
		  (srb->cmnd[4]&0x03) ? "Prevent" : "Allow");

		return USB_STOR_TRANSPORT_GOOD;

	}

	if (srb->cmnd[0] == READ_10 || srb->cmnd[0] == WRITE_10) {

		page = short_pack(srb->cmnd[3], srb->cmnd[2]);
		page <<= 16;
		page |= short_pack(srb->cmnd[5], srb->cmnd[4]);
		pages = short_pack(srb->cmnd[8], srb->cmnd[7]);

		page <<= info->smallpageshift;

		// convert page to block and page-within-block

		lba = page >> info->blockshift;
		page = page & info->blockmask;

		// locate physical block corresponding to logical block

		if (lba >= info->max_log_blks) {

			US_DEBUGP("Error: Requested LBA %04X exceeds maximum "
			  "block %04X\n", lba, info->max_log_blks-1);

			set_sense_info (5, 0x24, 0);	/* invalid field in command */

			return USB_STOR_TRANSPORT_FAILED;
		}

		pba = info->lba_to_pba[lba];

		if (srb->cmnd[0] == WRITE_10) {
			US_DEBUGP("WRITE_10: write block %04X (LBA %04X) page %01X"
			        " pages %d\n",
			        pba, lba, page, pages);

			return sddr55_write_data(us, lba, page, pages, ptr, srb->use_sg);
		} else {
			US_DEBUGP("READ_10: read block %04X (LBA %04X) page %01X"
			        " pages %d\n",
			        pba, lba, page, pages);

			return sddr55_read_data(us, lba, page, pages, ptr, srb->use_sg);
		}
	}


	if (srb->cmnd[0] == TEST_UNIT_READY) {
		return USB_STOR_TRANSPORT_GOOD;
	}

	if (srb->cmnd[0] == START_STOP) {
		return USB_STOR_TRANSPORT_GOOD;
	}

	set_sense_info (5, 0x20, 0);	/* illegal command */

	return USB_STOR_TRANSPORT_FAILED; // FIXME: sense buffer?
}

