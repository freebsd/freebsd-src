/* Driver for SanDisk SDDR-09 SmartMedia reader
 *
 *   (c) 2000, 2001 Robert Baruch (autophile@starband.net)
 *   (c) 2002 Andries Brouwer (aeb@cwi.nl)
 *
 * The SanDisk SDDR-09 SmartMedia reader uses the Shuttle EUSB-01 chip.
 * This chip is a programmable USB controller. In the SDDR-09, it has
 * been programmed to obey a certain limited set of SCSI commands.
 * This driver translates the "real" SCSI commands to the SDDR-09 SCSI
 * commands.
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
#include "sddr09.h"

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>

#define short_pack(lsb,msb) ( ((u16)(lsb)) | ( ((u16)(msb))<<8 ) )
#define LSB_of(s) ((s)&0xFF)
#define MSB_of(s) ((s)>>8)

/* #define US_DEBUGP printk */

/*
 * First some stuff that does not belong here:
 * data on SmartMedia and other cards, completely
 * unrelated to this driver.
 * Similar stuff occurs in <linux/mtd/nand_ids.h>.
 */

struct nand_flash_dev {
	int model_id;
	int chipshift;		/* 1<<cs bytes total capacity */
	char pageshift;		/* 1<<ps bytes in a page */
	char blockshift;	/* 1<<bs pages in an erase block */
	char zoneshift;		/* 1<<zs blocks in a zone */
				/* # of logical blocks is 125/128 of this */
	char pageadrlen;	/* length of an address in bytes - 1 */
};

/*
 * NAND Flash Manufacturer ID Codes
 */
#define NAND_MFR_AMD		0x01
#define NAND_MFR_TOSHIBA	0x98
#define NAND_MFR_SAMSUNG	0xec

static inline char *nand_flash_manufacturer(int manuf_id) {
	switch(manuf_id) {
	case NAND_MFR_AMD:
		return "AMD";
	case NAND_MFR_TOSHIBA:
		return "Toshiba";
	case NAND_MFR_SAMSUNG:
		return "Samsung";
	default:
		return "unknown";
	}
}

/*
 * It looks like it is unnecessary to attach manufacturer to the
 * remaining data: SSFDC prescribes manufacturer-independent id codes.
 */

static struct nand_flash_dev nand_flash_ids[] = {
	/* NAND flash - these I verified */
	{ 0x6e, 20, 8, 4, 8, 2},	/* 1 MB */
	{ 0xe8, 20, 8, 4, 8, 2},	/* 1 MB */
	{ 0xec, 20, 8, 4, 8, 2},	/* 1 MB */
	{ 0x64, 21, 8, 4, 9, 2}, 	/* 2 MB */
	{ 0xea, 21, 8, 4, 9, 2},	/* 2 MB */
	{ 0x6b, 22, 9, 4, 9, 2},	/* 4 MB */
	{ 0xe3, 22, 9, 4, 9, 2},	/* 4 MB */
	{ 0xe5, 22, 9, 4, 9, 2},	/* 4 MB */
	{ 0xe6, 23, 9, 4, 10, 2},	/* 8 MB */
	{ 0x73, 24, 9, 5, 10, 2},	/* 16 MB */
	{ 0x75, 25, 9, 5, 10, 2},	/* 32 MB */
	{ 0x76, 26, 9, 5, 10, 3},	/* 64 MB */
	{ 0x79, 27, 9, 5, 10, 3},	/* 128 MB */
	/* MASK ROM - from unknown source */
	{ 0x5d, 21, 9, 4, 8, 2},	/* 2 MB */
	{ 0xd5, 22, 9, 4, 9, 2},	/* 4 MB */
	{ 0xd6, 23, 9, 4, 10, 2},	/* 8 MB */
	{ 0,}
};

#define SIZE(a)	(sizeof(a)/sizeof((a)[0]))

static struct nand_flash_dev *
nand_find_id(unsigned char id) {
	int i;

	for (i = 0; i < SIZE(nand_flash_ids); i++)
		if (nand_flash_ids[i].model_id == id)
			return &(nand_flash_ids[i]);
	return NULL;
}

/*
 * ECC computation.
 */
static unsigned char parity[256];
static unsigned char ecc2[256];

static void nand_init_ecc(void) {
	int i, j, a;

	parity[0] = 0;
	for (i = 1; i < 256; i++)
		parity[i] = (parity[i&(i-1)] ^ 1);

	for (i = 0; i < 256; i++) {
		a = 0;
		for (j = 0; j < 8; j++) {
			if (i & (1<<j)) {
				if ((j & 1) == 0)
					a ^= 0x04;
				if ((j & 2) == 0)
					a ^= 0x10;
				if ((j & 4) == 0)
					a ^= 0x40;
			}
		}
		ecc2[i] = ~(a ^ (a<<1) ^ (parity[i] ? 0xa8 : 0));
	}
}

/* compute 3-byte ecc on 256 bytes */
static void nand_compute_ecc(unsigned char *data, unsigned char *ecc) {
	int i, j, a;
	unsigned char par, bit, bits[8];

	par = 0;
	for (j = 0; j < 8; j++)
		bits[j] = 0;

	/* collect 16 checksum bits */
	for (i = 0; i < 256; i++) {
		par ^= data[i];
		bit = parity[data[i]];
		for (j = 0; j < 8; j++)
			if ((i & (1<<j)) == 0)
				bits[j] ^= bit;
	}

	/* put 4+4+4 = 12 bits in the ecc */
	a = (bits[3] << 6) + (bits[2] << 4) + (bits[1] << 2) + bits[0];
	ecc[0] = ~(a ^ (a<<1) ^ (parity[par] ? 0xaa : 0));

	a = (bits[7] << 6) + (bits[6] << 4) + (bits[5] << 2) + bits[4];
	ecc[1] = ~(a ^ (a<<1) ^ (parity[par] ? 0xaa : 0));

	ecc[2] = ecc2[par];
}

static int nand_compare_ecc(unsigned char *data, unsigned char *ecc) {
	return (data[0] == ecc[0] && data[1] == ecc[1] && data[2] == ecc[2]);
}

static void nand_store_ecc(unsigned char *data, unsigned char *ecc) {
	memcpy(data, ecc, 3);
}

/*
 * The actual driver starts here.
 */

/*
 * On my 16MB card, control blocks have size 64 (16 real control bytes,
 * and 48 junk bytes). In reality of course the card uses 16 control bytes,
 * so the reader makes up the remaining 48. Don't know whether these numbers
 * depend on the card. For now a constant.
 */
#define CONTROL_SHIFT 6

/*
 * LBA and PBA are unsigned ints. Special values.
 */
#define UNDEF    0xffffffff
#define SPARE    0xfffffffe
#define UNUSABLE 0xfffffffd

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

static int
sddr09_send_control(struct us_data *us,
		    int pipe,
		    unsigned char request,
		    unsigned char requesttype,
		    unsigned int value,
		    unsigned int index,
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
			result = usb_clear_halt(us->pusb_dev, pipe);
			US_DEBUGP("-- usb_clear_halt() returns %d\n", result);
			return USB_STOR_TRANSPORT_FAILED;
		}

		return USB_STOR_TRANSPORT_ERROR;
	}

	return USB_STOR_TRANSPORT_GOOD;
}

/* send vendor interface command (0x41) */
/* called for requests 0, 1, 8 */
static int
sddr09_send_command(struct us_data *us,
		    unsigned char request,
		    unsigned char direction,
		    unsigned char *xfer_data,
		    unsigned int xfer_len) {
	int pipe;
	unsigned char requesttype = (0x41 | direction);

	// Get the receive or send control pipe number

	if (direction == USB_DIR_IN)
		pipe = usb_rcvctrlpipe(us->pusb_dev,0);
	else
		pipe = usb_sndctrlpipe(us->pusb_dev,0);

	return sddr09_send_control(us, pipe, request, requesttype,
				   0, 0, xfer_data, xfer_len);
}

static int
sddr09_send_scsi_command(struct us_data *us,
			 unsigned char *command,
			 unsigned int command_len) {
	return sddr09_send_command(us, 0, USB_DIR_OUT, command, command_len);
}

static int
sddr09_raw_bulk(struct us_data *us, int direction,
		unsigned char *data, unsigned int len) {

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
                /* -ECONNRESET -- we canceled this transfer */
                if (result == -ECONNRESET) {
                        US_DEBUGP("usbat_raw_bulk(): transfer aborted\n");
                        return US_BULK_TRANSFER_ABORTED;
                }

                /* NAK - that means we've retried a few times already */
       	        if (result == -ETIMEDOUT)
                        US_DEBUGP("usbat_raw_bulk(): device NAKed\n");
		else if (result == -EOVERFLOW)
			US_DEBUGP("us_transfer_partial(): babble/overflow\n");
		else if (result != -EPIPE)
			US_DEBUGP("us_transfer_partial(): unknown error %d\n",
				  result);

                return US_BULK_TRANSFER_FAILED;
        }

	if (act_len != len) {
		US_DEBUGP("Warning: Transferred only %d of %d bytes\n",
			  act_len, len);
		return US_BULK_TRANSFER_SHORT;
	}

	return US_BULK_TRANSFER_GOOD;
}

static int
sddr09_bulk_transport(struct us_data *us, int direction,
		      unsigned char *data, unsigned int len,
		      int use_sg) {

	int result = USB_STOR_TRANSPORT_GOOD;
	int transferred = 0;
	int i;
	struct scatterlist *sg;
	char string[64];

#define DEBUG_PRCT 12

	if (len == 0)
		return USB_STOR_TRANSPORT_GOOD;

	if (direction == SCSI_DATA_WRITE && !use_sg) {

		/* Debug-print the first N bytes of the write transfer */

		strcpy(string, "wr: ");
		for (i=0; i<len && i<DEBUG_PRCT; i++) {
			sprintf(string+strlen(string), "%02X ",
				data[i]);
			if ((i%16) == 15) {
				US_DEBUGP("%s\n", string);
				strcpy(string, "wr: ");
			}
		}
		if ((i%16)!=0)
			US_DEBUGP("%s\n", string);
	}

	US_DEBUGP("SCM data %s transfer %d sg buffers %d\n",
		  (direction == SCSI_DATA_READ) ? "in" : "out",
		  len, use_sg);

	if (!use_sg)
		result = sddr09_raw_bulk(us, direction, data, len);
	else {
		sg = (struct scatterlist *)data;

		for (i=0; i<use_sg && transferred<len; i++) {
			unsigned char *buf;
			unsigned int length;

			buf = sg[i].address;
			length = len-transferred;
			if (length > sg[i].length)
				length = sg[i].length;

			result = sddr09_raw_bulk(us, direction, buf, length);
			if (result != US_BULK_TRANSFER_GOOD)
				break;
			transferred += sg[i].length;
		}
	}

	if (direction == SCSI_DATA_READ && !use_sg) {

		/* Debug-print the first N bytes of the read transfer */

		strcpy(string, "rd: ");
		for (i=0; i<len && i<DEBUG_PRCT; i++) {
			sprintf(string+strlen(string), "%02X ",
				data[i]);
			if ((i%16) == 15) {
				US_DEBUGP("%s\n", string);
				strcpy(string, "rd: ");
			}
		}
		if ((i%16)!=0)
			US_DEBUGP("%s\n", string);
	}

	return result;
}

#if 0
/*
 * Test Unit Ready Command: 12 bytes.
 * byte 0: opcode: 00
 */
static int
sddr09_test_unit_ready(struct us_data *us) {
	unsigned char command[6] = {
		0, 0x20, 0, 0, 0, 0
	};
	int result;

	result = sddr09_send_scsi_command(us, command, sizeof(command));

	US_DEBUGP("sddr09_test_unit_ready returns %d\n", result);

	return result;
}
#endif

/*
 * Request Sense Command: 12 bytes.
 * byte 0: opcode: 03
 * byte 4: data length
 */
static int
sddr09_request_sense(struct us_data *us, unsigned char *sensebuf, int buflen) {
	unsigned char command[12] = {
		0x03, 0x20, 0, 0, buflen, 0, 0, 0, 0, 0, 0, 0
	};
	int result;

	result = sddr09_send_scsi_command(us, command, sizeof(command));
	if (result != USB_STOR_TRANSPORT_GOOD) {
		US_DEBUGP("request sense failed\n");
		return result;
	}

	result = sddr09_raw_bulk(us, SCSI_DATA_READ, sensebuf, buflen);
	if (result != USB_STOR_TRANSPORT_GOOD)
		US_DEBUGP("request sense bulk in failed\n");
	else
		US_DEBUGP("request sense worked\n");

	return result;
}

/*
 * Read Command: 12 bytes.
 * byte 0: opcode: E8
 * byte 1: last two bits: 00: read data, 01: read blockwise control,
 *                        10: read both, 11: read pagewise control.
 *         It turns out we need values 20, 21, 22, 23 here (LUN 1).
 * bytes 2-5: address (interpretation depends on byte 1, see below)
 * bytes 10-11: count (idem)
 *
 * A page has 512 data bytes and 64 control bytes (16 control and 48 junk).
 * A read data command gets data in 512-byte pages.
 * A read control command gets control in 64-byte chunks.
 * A read both command gets data+control in 576-byte chunks.
 *
 * Blocks are groups of 32 pages, and read blockwise control jumps to the
 * next block, while read pagewise control jumps to the next page after
 * reading a group of 64 control bytes.
 * [Here 512 = 1<<pageshift, 32 = 1<<blockshift, 64 is constant?]
 *
 * (1 MB and 2 MB cards are a bit different, but I have only a 16 MB card.)
 */

static int
sddr09_readX(struct us_data *us, int x, unsigned long fromaddress,
	     int nr_of_pages, int bulklen, unsigned char *buf,
	     int use_sg) {

	unsigned char command[12] = {
		0xe8, 0x20 | x, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	int result;

	command[2] = MSB_of(fromaddress>>16);
	command[3] = LSB_of(fromaddress>>16); 
	command[4] = MSB_of(fromaddress & 0xFFFF);
	command[5] = LSB_of(fromaddress & 0xFFFF); 

	command[10] = MSB_of(nr_of_pages);
	command[11] = LSB_of(nr_of_pages);

	result = sddr09_send_scsi_command(us, command, sizeof(command));

	if (result != USB_STOR_TRANSPORT_GOOD) {
		US_DEBUGP("Result for send_control in sddr09_read2%d %d\n",
			  x, result);
		return result;
	}

	result = sddr09_bulk_transport(us, SCSI_DATA_READ,
				       buf, bulklen, use_sg);

	if (result != USB_STOR_TRANSPORT_GOOD)
		US_DEBUGP("Result for bulk_transport in sddr09_read2%d %d\n",
			  x, result);

	return result;
}

/*
 * Read Data
 *
 * fromaddress counts data shorts:
 * increasing it by 256 shifts the bytestream by 512 bytes;
 * the last 8 bits are ignored.
 *
 * nr_of_pages counts pages of size (1 << pageshift).
 */
static int
sddr09_read20(struct us_data *us, unsigned long fromaddress,
	      int nr_of_pages, int pageshift, unsigned char *buf, int use_sg) {
	int bulklen = nr_of_pages << pageshift;

	/* The last 8 bits of fromaddress are ignored. */
	return sddr09_readX(us, 0, fromaddress, nr_of_pages, bulklen,
			    buf, use_sg);
}

/*
 * Read Blockwise Control
 *
 * fromaddress gives the starting position (as in read data;
 * the last 8 bits are ignored); increasing it by 32*256 shifts
 * the output stream by 64 bytes.
 *
 * count counts control groups of size (1 << controlshift).
 * For me, controlshift = 6. Is this constant?
 *
 * After getting one control group, jump to the next block
 * (fromaddress += 8192).
 */
static int
sddr09_read21(struct us_data *us, unsigned long fromaddress,
	      int count, int controlshift, unsigned char *buf, int use_sg) {

	int bulklen = (count << controlshift);
	return sddr09_readX(us, 1, fromaddress, count, bulklen,
			    buf, use_sg);
}

/*
 * Read both Data and Control
 *
 * fromaddress counts data shorts, ignoring control:
 * increasing it by 256 shifts the bytestream by 576 = 512+64 bytes;
 * the last 8 bits are ignored.
 *
 * nr_of_pages counts pages of size (1 << pageshift) + (1 << controlshift).
 */
static int
sddr09_read22(struct us_data *us, unsigned long fromaddress,
	      int nr_of_pages, int pageshift, unsigned char *buf, int use_sg) {

	int bulklen = (nr_of_pages << pageshift) + (nr_of_pages << CONTROL_SHIFT);
	US_DEBUGP("sddr09_read22: reading %d pages, %d bytes\n",
		  nr_of_pages, bulklen);
	return sddr09_readX(us, 2, fromaddress, nr_of_pages, bulklen,
			    buf, use_sg);
}

#if 0
/*
 * Read Pagewise Control
 *
 * fromaddress gives the starting position (as in read data;
 * the last 8 bits are ignored); increasing it by 256 shifts
 * the output stream by 64 bytes.
 *
 * count counts control groups of size (1 << controlshift).
 * For me, controlshift = 6. Is this constant?
 *
 * After getting one control group, jump to the next page
 * (fromaddress += 256).
 */
static int
sddr09_read23(struct us_data *us, unsigned long fromaddress,
	      int count, int controlshift, unsigned char *buf, int use_sg) {

	int bulklen = (count << controlshift);
	return sddr09_readX(us, 3, fromaddress, count, bulklen,
			    buf, use_sg);
}
#endif

#if 0
/*
 * Erase Command: 12 bytes.
 * byte 0: opcode: EA
 * bytes 6-9: erase address (big-endian, counting shorts, sector aligned).
 * 
 * Always precisely one block is erased; bytes 2-5 and 10-11 are ignored.
 * The byte address being erased is 2*Eaddress.
 */
static int
sddr09_erase(struct us_data *us, unsigned long Eaddress) {
	unsigned char command[12] = {
		0xea, 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	int result;

	command[6] = MSB_of(Eaddress>>16);
	command[7] = LSB_of(Eaddress>>16);
	command[8] = MSB_of(Eaddress & 0xFFFF);
	command[9] = LSB_of(Eaddress & 0xFFFF);

	result = sddr09_send_scsi_command(us, command, sizeof(command));

	if (result != USB_STOR_TRANSPORT_GOOD)
		US_DEBUGP("Result for send_control in sddr09_erase %d\n",
			  result);

	return result;
}
#endif

/*
 * Write Command: 12 bytes.
 * byte 0: opcode: E9
 * bytes 2-5: write address (big-endian, counting shorts, sector aligned).
 * bytes 6-9: erase address (big-endian, counting shorts, sector aligned).
 * bytes 10-11: sector count (big-endian, in 512-byte sectors).
 *
 * If write address equals erase address, the erase is done first,
 * otherwise the write is done first. When erase address equals zero
 * no erase is done?
 */
static int
sddr09_writeX(struct us_data *us,
	      unsigned long Waddress, unsigned long Eaddress,
	      int nr_of_pages, int bulklen, unsigned char *buf, int use_sg) {

	unsigned char command[12] = {
		0xe9, 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	int result;

	command[2] = MSB_of(Waddress>>16);
	command[3] = LSB_of(Waddress>>16);
	command[4] = MSB_of(Waddress & 0xFFFF);
	command[5] = LSB_of(Waddress & 0xFFFF);

	command[6] = MSB_of(Eaddress>>16);
	command[7] = LSB_of(Eaddress>>16);
	command[8] = MSB_of(Eaddress & 0xFFFF);
	command[9] = LSB_of(Eaddress & 0xFFFF);

	command[10] = MSB_of(nr_of_pages);
	command[11] = LSB_of(nr_of_pages);

	result = sddr09_send_scsi_command(us, command, sizeof(command));

	if (result != USB_STOR_TRANSPORT_GOOD) {
		US_DEBUGP("Result for send_control in sddr09_writeX %d\n",
			  result);
		return result;
	}

	result = sddr09_bulk_transport(us, SCSI_DATA_WRITE,
				       buf, bulklen, use_sg);

	if (result != USB_STOR_TRANSPORT_GOOD)
		US_DEBUGP("Result for bulk_transport in sddr09_writeX %d\n",
			  result);

	return result;
}

/* erase address, write same address */
static int
sddr09_write_inplace(struct us_data *us, unsigned long address,
		     int nr_of_pages, int pageshift, unsigned char *buf,
		     int use_sg) {
	int bulklen = (nr_of_pages << pageshift) + (nr_of_pages << CONTROL_SHIFT);
	return sddr09_writeX(us, address, address, nr_of_pages, bulklen,
			     buf, use_sg);
}

#if 0
/*
 * Read Scatter Gather Command: 3+4n bytes.
 * byte 0: opcode E7
 * byte 2: n
 * bytes 4i-1,4i,4i+1: page address
 * byte 4i+2: page count
 * (i=1..n)
 *
 * This reads several pages from the card to a single memory buffer.
 */
static int
sddr09_read_sg_test_only(struct us_data *us) {
	unsigned char command[15] = {
		0xe7, 0x20, 0
	};
	int result, bulklen, nsg, ct;
	unsigned char *buf;
	unsigned long address;

	nsg = bulklen = 0;

	address = 040000; ct = 1;
	nsg++;
	bulklen += (ct << 9);
	command[4*nsg+2] = ct;
	command[4*nsg+1] = ((address >> 9) & 0xFF);
	command[4*nsg+0] = ((address >> 17) & 0xFF);
	command[4*nsg-1] = ((address >> 25) & 0xFF);

	address = 0340000; ct = 1;
	nsg++;
	bulklen += (ct << 9);
	command[4*nsg+2] = ct;
	command[4*nsg+1] = ((address >> 9) & 0xFF);
	command[4*nsg+0] = ((address >> 17) & 0xFF);
	command[4*nsg-1] = ((address >> 25) & 0xFF);

	address = 01000000; ct = 2;
	nsg++;
	bulklen += (ct << 9);
	command[4*nsg+2] = ct;
	command[4*nsg+1] = ((address >> 9) & 0xFF);
	command[4*nsg+0] = ((address >> 17) & 0xFF);
	command[4*nsg-1] = ((address >> 25) & 0xFF);

	command[2] = nsg;

	result = sddr09_send_scsi_command(us, command, 4*nsg+3);

	if (result != USB_STOR_TRANSPORT_GOOD) {
		US_DEBUGP("Result for send_control in sddr09_read_sg %d\n",
			  result);
		return result;
	}

	buf = (unsigned char *) kmalloc(bulklen, GFP_NOIO);
	if (!buf)
		return USB_STOR_TRANSPORT_ERROR;

	result = sddr09_bulk_transport(us, SCSI_DATA_READ,
				       buf, bulklen, 0);
	if (result != USB_STOR_TRANSPORT_GOOD)
		US_DEBUGP("Result for bulk_transport in sddr09_read_sg %d\n",
			  result);

	kfree(buf);

	return result;
}
#endif

/*
 * Read Status Command: 12 bytes.
 * byte 0: opcode: EC
 *
 * Returns 64 bytes, all zero except for the first.
 * bit 0: 1: Error
 * bit 5: 1: Suspended
 * bit 6: 1: Ready
 * bit 7: 1: Not write-protected
 */

static int
sddr09_read_status(struct us_data *us, unsigned char *status) {

	unsigned char command[12] = {
		0xec, 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	unsigned char data[64];
	int result;

	US_DEBUGP("Reading status...\n");

	result = sddr09_send_scsi_command(us, command, sizeof(command));
	if (result != USB_STOR_TRANSPORT_GOOD)
		return result;

	result = sddr09_bulk_transport(us, SCSI_DATA_READ,
				       data, sizeof(data), 0);
	*status = data[0];
	return result;
}

static int
sddr09_read_data(struct us_data *us,
		 unsigned long address,
		 unsigned int sectors,
		 unsigned char *content,
		 int use_sg) {

	struct sddr09_card_info *info = (struct sddr09_card_info *) us->extra;
	unsigned int lba, maxlba, pba;
	unsigned int page, pages;
	unsigned char *buffer = NULL;
	unsigned char *ptr;
	struct scatterlist *sg = NULL;
	int result, i, len;

	// If we're using scatter-gather, we have to create a new
	// buffer to read all of the data in first, since a
	// scatter-gather buffer could in theory start in the middle
	// of a page, which would be bad. A developer who wants a
	// challenge might want to write a limited-buffer
	// version of this code.

	len = sectors*info->pagesize;

	if (use_sg) {
		sg = (struct scatterlist *)content;
		buffer = kmalloc(len, GFP_NOIO);
		if (buffer == NULL)
			return USB_STOR_TRANSPORT_ERROR;
		ptr = buffer;
	} else
		ptr = content;

	// Figure out the initial LBA and page
	lba = address >> info->blockshift;
	page = (address & info->blockmask);
	maxlba = info->capacity >> (info->pageshift + info->blockshift);

	// This could be made much more efficient by checking for
	// contiguous LBA's. Another exercise left to the student.

	result = USB_STOR_TRANSPORT_GOOD;

	while (sectors > 0) {

		/* Find number of pages we can read in this block */
		pages = info->blocksize - page;
		if (pages > sectors)
			pages = sectors;

		/* Not overflowing capacity? */
		if (lba >= maxlba) {
			US_DEBUGP("Error: Requested lba %u exceeds "
				  "maximum %u\n", lba, maxlba);
			result = USB_STOR_TRANSPORT_ERROR;
			break;
		}

		/* Find where this lba lives on disk */
		pba = info->lba_to_pba[lba];

		if (pba == UNDEF) {	/* this lba was never written */

			US_DEBUGP("Read %d zero pages (LBA %d) page %d\n",
				  pages, lba, page);

			/* This is not really an error. It just means
			   that the block has never been written.
			   Instead of returning USB_STOR_TRANSPORT_ERROR
			   it is better to return all zero data. */

			memset(ptr, 0, pages << info->pageshift);

		} else {
			US_DEBUGP("Read %d pages, from PBA %d"
				  " (LBA %d) page %d\n",
				  pages, pba, lba, page);

			address = ((pba << info->blockshift) + page) << 
				info->pageshift;

			result = sddr09_read20(us, address>>1,
					       pages, info->pageshift, ptr, 0);
			if (result != USB_STOR_TRANSPORT_GOOD)
				break;
		}

		page = 0;
		lba++;
		sectors -= pages;
		ptr += (pages << info->pageshift);
	}

	if (use_sg && result == USB_STOR_TRANSPORT_GOOD) {
		int transferred = 0;

		for (i=0; i<use_sg && transferred<len; i++) {
			unsigned char *buf = sg[i].address;
			unsigned int length;

			length = len-transferred;
			if (length > sg[i].length)
				length = sg[i].length;

			memcpy(buf, buffer+transferred, length);
			transferred += sg[i].length;
		}
	}

	if (use_sg)
		kfree(buffer);

	return result;
}

/* we never free blocks, so lastpba can only increase */
static unsigned int
sddr09_find_unused_pba(struct sddr09_card_info *info) {
	static unsigned int lastpba = 1;
	int numblocks = info->capacity >> (info->blockshift + info->pageshift);
	int i;

	for (i = lastpba+1; i < numblocks; i++) {
		if (info->pba_to_lba[i] == UNDEF) {
			lastpba = i;
			return i;
		}
	}
	return 0;
}

static int
sddr09_write_lba(struct us_data *us, unsigned int lba,
		 unsigned int page, unsigned int pages,
		 unsigned char *ptr) {

	struct sddr09_card_info *info = (struct sddr09_card_info *) us->extra;
	unsigned long address;
	unsigned int pba, lbap;
	unsigned int pagelen, blocklen;
	unsigned char *blockbuffer, *bptr, *cptr, *xptr;
	unsigned char ecc[3];
	int i, result;

	lbap = ((lba & 0x3ff) << 1) | 0x1000;
	if (parity[MSB_of(lbap) ^ LSB_of(lbap)])
		lbap ^= 1;
	pba = info->lba_to_pba[lba];

	if (pba == UNDEF) {
		pba = sddr09_find_unused_pba(info);
		if (!pba) {
			printk("sddr09_write_lba: Out of unused blocks\n");
			return USB_STOR_TRANSPORT_ERROR;
		}
		info->pba_to_lba[pba] = lba;
		info->lba_to_pba[lba] = pba;
	}

	if (pba == 1) {
		/* Maybe it is impossible to write to PBA 1.
		   Fake success, but don't do anything. */
		printk("sddr09: avoid writing to pba 1\n");
		return USB_STOR_TRANSPORT_GOOD;
	}

	pagelen = (1 << info->pageshift) + (1 << CONTROL_SHIFT);
	blocklen = (pagelen << info->blockshift);
	blockbuffer = kmalloc(blocklen, GFP_NOIO);
	if (!blockbuffer) {
		printk("sddr09_write_lba: Out of memory\n");
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* read old contents */
	address = (pba << (info->pageshift + info->blockshift));
	result = sddr09_read22(us, address>>1, info->blocksize,
			       info->pageshift, blockbuffer, 0);
	if (result != USB_STOR_TRANSPORT_GOOD)
		goto err;

	/* check old contents */
	for (i = 0; i < info->blockshift; i++) {
		bptr = blockbuffer + i*pagelen;
		cptr = bptr + info->pagesize;
		nand_compute_ecc(bptr, ecc);
		if (!nand_compare_ecc(cptr+13, ecc)) {
			US_DEBUGP("Warning: bad ecc in page %d- of pba %d\n",
				  i, pba);
			nand_store_ecc(cptr+13, ecc);
		}
		nand_compute_ecc(bptr+(info->pagesize / 2), ecc);
		if (!nand_compare_ecc(cptr+8, ecc)) {
			US_DEBUGP("Warning: bad ecc in page %d+ of pba %d\n",
				  i, pba);
			nand_store_ecc(cptr+8, ecc);
		}
	}

	/* copy in new stuff and compute ECC */
	xptr = ptr;
	for (i = page; i < page+pages; i++) {
		bptr = blockbuffer + i*pagelen;
		cptr = bptr + info->pagesize;
		memcpy(bptr, xptr, info->pagesize);
		xptr += info->pagesize;
		nand_compute_ecc(bptr, ecc);
		nand_store_ecc(cptr+13, ecc);
		nand_compute_ecc(bptr+(info->pagesize / 2), ecc);
		nand_store_ecc(cptr+8, ecc);
		cptr[6] = cptr[11] = MSB_of(lbap);
		cptr[7] = cptr[12] = LSB_of(lbap);
	}

	US_DEBUGP("Rewrite PBA %d (LBA %d)\n", pba, lba);

	result = sddr09_write_inplace(us, address>>1, info->blocksize,
				      info->pageshift, blockbuffer, 0);

	US_DEBUGP("sddr09_write_inplace returns %d\n", result);

#if 0
	{
	    unsigned char status = 0;
	    int result2 = sddr09_read_status(us, &status);
	    if (result2 != USB_STOR_TRANSPORT_GOOD)
		US_DEBUGP("sddr09_write_inplace: cannot read status\n");
	    else if (status != 0xc0)
		US_DEBUGP("sddr09_write_inplace: status after write: 0x%x\n",
			  status);
	}
#endif

#if 0
	{
	    int result2 = sddr09_test_unit_ready(us);
	}
#endif
 err:
	kfree(blockbuffer);

	/* TODO: instead of doing kmalloc/kfree for each block,
	   add a bufferpointer to the info structure */

	return result;
}

static int
sddr09_write_data(struct us_data *us,
		  unsigned long address,
		  unsigned int sectors,
		  unsigned char *content,
		  int use_sg) {

	struct sddr09_card_info *info = (struct sddr09_card_info *) us->extra;
	unsigned int lba, page, pages;
	unsigned char *buffer = NULL;
	unsigned char *ptr;
	struct scatterlist *sg = NULL;
	int result, i, len;

	// If we're using scatter-gather, we have to create a new
	// buffer to write all of the data in first, since a
	// scatter-gather buffer could in theory start in the middle
	// of a page, which would be bad. A developer who wants a
	// challenge might want to write a limited-buffer
	// version of this code.

	len = sectors*info->pagesize;

	if (use_sg) {
		int transferred = 0;

		sg = (struct scatterlist *)content;
		buffer = kmalloc(len, GFP_NOIO);
		if (buffer == NULL)
			return USB_STOR_TRANSPORT_ERROR;

		for (i=0; i<use_sg && transferred<len; i++) {
			memcpy(buffer+transferred,
			       sg[i].address,
			       len-transferred > sg[i].length ?
			        sg[i].length : len-transferred);
			transferred += sg[i].length;
		}
		ptr = buffer;
	} else
		ptr = content;

	// Figure out the initial LBA and page
	lba = address >> info->blockshift;
	page = (address & info->blockmask);

	// This could be made much more efficient by checking for
	// contiguous LBA's. Another exercise left to the student.

	result = USB_STOR_TRANSPORT_GOOD;

	while (sectors > 0) {

		// Write as many sectors as possible in this block

		pages = info->blocksize - page;
		if (pages > sectors)
			pages = sectors;

		result = sddr09_write_lba(us, lba, page, pages, ptr);
		if (result != USB_STOR_TRANSPORT_GOOD)
			break;

		page = 0;
		lba++;
		sectors -= pages;
		ptr += (pages << info->pageshift);
	}

	if (use_sg)
		kfree(buffer);

	return result;
}

int sddr09_read_control(struct us_data *us,
		unsigned long address,
		unsigned int blocks,
		unsigned char *content,
		int use_sg) {

	US_DEBUGP("Read control address %08lX blocks %04X\n",
		address, blocks);

	return sddr09_read21(us, address, blocks, CONTROL_SHIFT, content, use_sg);
}

static int
sddr09_read_deviceID(struct us_data *us, unsigned char *deviceID) {
/*
 * Read Device ID Command: 12 bytes.
 * byte 0: opcode: ED
 *
 * Returns 2 bytes: Manufacturer ID and Device ID.
 * On more recent cards 3 bytes: the third byte is an option code A5
 * signifying that the secret command to read an 128-bit ID is available.
 * On still more recent cards 4 bytes: the fourth byte C0 means that
 * a second read ID cmd is available.
 */

	unsigned char command[12] = {
		0xed, 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	unsigned char content[64];
	int result, i;

	result = sddr09_send_scsi_command(us, command, sizeof(command));
	if (result != USB_STOR_TRANSPORT_GOOD)
		return result;

	result = sddr09_bulk_transport(us, SCSI_DATA_READ, content, 64, 0);

	for (i = 0; i < 4; i++)
		deviceID[i] = content[i];

	return result;
}

static int
sddr09_get_wp(struct us_data *us, struct sddr09_card_info *info) {
	int result;
	unsigned char status;

	result = sddr09_read_status(us, &status);
	if (result != USB_STOR_TRANSPORT_GOOD) {
		US_DEBUGP("sddr09_get_wp: read_status fails\n");
		return result;
	}
	US_DEBUGP("sddr09_get_wp: status %02X", status);
	if ((status & 0x80) == 0) {
		info->flags |= SDDR09_WP;	/* write protected */
		US_DEBUGP(" WP");
	}
	if (status & 0x40)
		US_DEBUGP(" Ready");
	if (status & 0x20)
		US_DEBUGP(" Suspended");
	if (status & 0x1)
		US_DEBUGP(" Error");
	US_DEBUGP("\n");
	return USB_STOR_TRANSPORT_GOOD;
}

#if 0
/*
 * Reset Command: 12 bytes.
 * byte 0: opcode: EB
 */
static int
sddr09_reset(struct us_data *us) {

	unsigned char command[12] = {
		0xeb, 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	return sddr09_send_scsi_command(us, command, sizeof(command));
}
#endif

static struct nand_flash_dev *
sddr09_get_cardinfo(struct us_data *us, unsigned char flags) {
	struct nand_flash_dev *cardinfo;
	unsigned char deviceID[4];
	char blurbtxt[256];
	int result;

	US_DEBUGP("Reading capacity...\n");

	result = sddr09_read_deviceID(us, deviceID);

	if (result != USB_STOR_TRANSPORT_GOOD) {
		US_DEBUGP("Result of read_deviceID is %d\n", result);
		printk("sddr09: could not read card info\n");
		return 0;
	}

	sprintf(blurbtxt, "sddr09: Found Flash card, ID = %02X %02X %02X %02X",
		deviceID[0], deviceID[1], deviceID[2], deviceID[3]);

	/* Byte 0 is the manufacturer */
	sprintf(blurbtxt + strlen(blurbtxt),
		": Manuf. %s",
		nand_flash_manufacturer(deviceID[0]));

	/* Byte 1 is the device type */
	cardinfo = nand_find_id(deviceID[1]);
	if (cardinfo) {
		/* MB or MiB? It is neither. A 16 MB card has
		   17301504 raw bytes, of which 16384000 are
		   usable for user data. */
		sprintf(blurbtxt + strlen(blurbtxt),
			", %d MB", 1<<(cardinfo->chipshift - 20));
	} else {
		sprintf(blurbtxt + strlen(blurbtxt),
			", type unrecognized");
	}

	/* Byte 2 is code to signal availability of 128-bit ID */
	if (deviceID[2] == 0xa5) {
		sprintf(blurbtxt + strlen(blurbtxt),
			", 128-bit ID");
	}

	/* Byte 3 announces the availability of another read ID command */
	if (deviceID[3] == 0xc0) {
		sprintf(blurbtxt + strlen(blurbtxt),
			", extra cmd");
	}

	if (flags & SDDR09_WP)
		sprintf(blurbtxt + strlen(blurbtxt),
			", WP");

	printk("%s\n", blurbtxt);

	return cardinfo;
}

static int
sddr09_read_map(struct us_data *us) {

	struct scatterlist *sg;
	struct sddr09_card_info *info = (struct sddr09_card_info *) us->extra;
	int numblocks, alloc_len, alloc_blocks;
	int i, j, result;
	unsigned char *ptr;
	unsigned int lba, lbact;

	if (!info->capacity)
		return -1;

	// read 64 (1<<6) bytes for every block 
	// ( 1 << ( blockshift + pageshift ) bytes)
	//	 of capacity:
	// (1<<6)*capacity/(1<<(b+p)) =
	// ((1<<6)*capacity)>>(b+p) =
	// capacity>>(b+p-6)

	alloc_len = info->capacity >> 
		(info->blockshift + info->pageshift - CONTROL_SHIFT);

	// Allocate a number of scatterlist structures according to
	// the number of 128k blocks in the alloc_len. Adding 128k-1
	// and then dividing by 128k gives the correct number of blocks.
	// 128k = 1<<17

	alloc_blocks = (alloc_len + (1<<17) - 1) >> 17;
	sg = kmalloc(alloc_blocks*sizeof(struct scatterlist),
		     GFP_NOIO);
	if (sg == NULL)
		return 0;

	for (i=0; i<alloc_blocks; i++) {
		if (i<alloc_blocks-1) {
			sg[i].address = kmalloc( (1<<17), GFP_NOIO );
			sg[i].page = NULL;
			sg[i].length = (1<<17);
		} else {
			sg[i].address = kmalloc(alloc_len, GFP_NOIO);
			sg[i].page = NULL;
			sg[i].length = alloc_len;
		}
		alloc_len -= sg[i].length;
	}
	for (i=0; i<alloc_blocks; i++)
		if (sg[i].address == NULL) {
			for (i=0; i<alloc_blocks; i++)
				if (sg[i].address != NULL)
					kfree(sg[i].address);
			kfree(sg);
			return 0;
		}

	numblocks = info->capacity >> (info->blockshift + info->pageshift);

	result = sddr09_read_control(us, 0, numblocks,
				     (unsigned char *)sg, alloc_blocks);
	if (result != USB_STOR_TRANSPORT_GOOD) {
		for (i=0; i<alloc_blocks; i++)
			kfree(sg[i].address);
		kfree(sg);
		return -1;
	}

	kfree(info->lba_to_pba);
	kfree(info->pba_to_lba);
	info->lba_to_pba = kmalloc(numblocks*sizeof(int), GFP_NOIO);
	info->pba_to_lba = kmalloc(numblocks*sizeof(int), GFP_NOIO);

	if (info->lba_to_pba == NULL || info->pba_to_lba == NULL) {
		kfree(info->lba_to_pba);
		kfree(info->pba_to_lba);
		info->lba_to_pba = NULL;
		info->pba_to_lba = NULL;
		for (i=0; i<alloc_blocks; i++)
			kfree(sg[i].address);
		kfree(sg);
		return 0;
	}

	for (i = 0; i < numblocks; i++)
		info->lba_to_pba[i] = info->pba_to_lba[i] = UNDEF;

	ptr = sg[0].address;

	/*
	 * Define lba-pba translation table
	 */
	// Each block is 64 bytes of control data, so block i is located in
	// scatterlist block i*64/128k = i*(2^6)*(2^-17) = i*(2^-11)

#if 0
	/* No translation */
	for (i=0; i<numblocks; i++) {
		lba = i;
		info->pba_to_lba[i] = lba;
		info->lba_to_pba[lba] = i;
	}
	printk("sddr09: no translation today\n");
#else
	for (i=0; i<numblocks; i++) {
		ptr = sg[i>>11].address + ((i&0x7ff)<<6);

		if (i == 0 || i == 1) {
			info->pba_to_lba[i] = UNUSABLE;
			continue;
		}

		/* special PBAs have control field 0^16 */
		for (j = 0; j < 16; j++)
			if (ptr[j] != 0)
				goto nonz;
		info->pba_to_lba[i] = UNUSABLE;
		printk("sddr09: PBA %04X has no logical mapping\n", i);
		continue;

	nonz:
		/* unwritten PBAs have control field FF^16 */
		for (j = 0; j < 16; j++)
			if (ptr[j] != 0xff)
				goto nonff;
		continue;

	nonff:
		/* normal PBAs start with six FFs */
		if (j < 6) {
			printk("sddr09: PBA %04X has no logical mapping: "
			       "reserved area = %02X%02X%02X%02X "
			       "data status %02X block status %02X\n",
			       i, ptr[0], ptr[1], ptr[2], ptr[3],
			       ptr[4], ptr[5]);
			info->pba_to_lba[i] = UNUSABLE;
			continue;
		}

		if ((ptr[6] >> 4) != 0x01) {
			printk("sddr09: PBA %04X has invalid address field "
			       "%02X%02X/%02X%02X\n",
			       i, ptr[6], ptr[7], ptr[11], ptr[12]);
			info->pba_to_lba[i] = UNUSABLE;
			continue;
		}

		/* check even parity */
		if (parity[ptr[6] ^ ptr[7]]) {
			printk("sddr09: Bad parity in LBA for block %04X"
			       " (%02X %02X)\n", i, ptr[6], ptr[7]);
			info->pba_to_lba[i] = UNUSABLE;
			continue;
		}

		lba = short_pack(ptr[7], ptr[6]);
		lba = (lba & 0x07FF) >> 1;

		/*
		 * Every 1024 physical blocks ("zone"), the LBA numbers
		 * go back to zero, but are within a higher block of LBA's.
		 * Also, there is a maximum of 1000 LBA's per zone.
		 * In other words, in PBA 1024-2047 you will find LBA 0-999
		 * which are really LBA 1000-1999. This allows for 24 bad
		 * or special physical blocks per zone.
		 */

		if (lba >= 1000) {
			printk("sddr09: Bad LBA %04X for block %04X\n",
			       lba, i);
			info->pba_to_lba[i] = UNDEF /* UNUSABLE */;
			continue;
		}

		lba += 1000*(i/0x400);

		if (lba<0x10 || (lba >= 0x3E0 && lba < 0x3EF))
			US_DEBUGP("LBA %04X <-> PBA %04X\n", lba, i);

		info->pba_to_lba[i] = lba;
		info->lba_to_pba[lba] = i;
	}
#endif

	/*
	 * Approximate capacity. This is not entirely correct yet,
	 * since a zone with less than 1000 usable pages leads to
	 * missing LBAs. Especially if it is the last zone, some
	 * LBAs can be past capacity.
	 */
	lbact = 0;
	for (i = 0; i < numblocks; i += 1024) {
		int ct = 0;

		for (j = 0; j < 1024 && i+j < numblocks; j++) {
			if (info->pba_to_lba[i+j] != UNUSABLE) {
				if (ct >= 1000)
					info->pba_to_lba[i+j] = SPARE;
				else
					ct++;
			}
		}
		lbact += ct;
	}
	info->lbact = lbact;
	US_DEBUGP("Found %d LBA's\n", lbact);

	for (i=0; i<alloc_blocks; i++)
		kfree(sg[i].address);
	kfree(sg);
	return 0;
}

static void
sddr09_card_info_destructor(void *extra) {
	struct sddr09_card_info *info = (struct sddr09_card_info *)extra;

	if (!info)
		return;

	kfree(info->lba_to_pba);
	kfree(info->pba_to_lba);
}

static void
sddr09_init_card_info(struct us_data *us) {
	if (!us->extra) {
		us->extra = kmalloc(sizeof(struct sddr09_card_info), GFP_NOIO);
		if (us->extra) {
			memset(us->extra, 0, sizeof(struct sddr09_card_info));
			us->extra_destructor = sddr09_card_info_destructor;
		}
	}
}

/*
 * It is unclear whether this does anything.
 * However, the request sense succeeds only after a reboot,
 * not if we do this a second time.
 */
int
sddr09_init(struct us_data *us) {
#if 0
	int result;
	unsigned char data[2];

	printk("sddr09_init\n");

	nand_init_ecc();

	result = sddr09_send_command(us, 0x01, USB_DIR_IN, data, 2);
	if (result != USB_STOR_TRANSPORT_GOOD) {
		US_DEBUGP("sddr09_init: send_command fails\n");
		return result;
	}

	US_DEBUGP("SDDR09init: %02X %02X\n", data[0], data[1]);
	// get 07 02

	result = sddr09_send_command(us, 0x08, USB_DIR_IN, data, 2);
	if (result != USB_STOR_TRANSPORT_GOOD) {
		US_DEBUGP("sddr09_init: 2nd send_command fails\n");
		return result;
	}

	US_DEBUGP("SDDR09init: %02X %02X\n", data[0], data[1]);
	// get 07 00

#if 1
	result = sddr09_request_sense(us, data, sizeof(data));
	if (result == USB_STOR_TRANSPORT_GOOD && data[2] != 0) {
		int j;
		for (j=0; j<sizeof(data); j++)
			printk(" %02X", data[j]);
		printk("\n");
		// get 70 00 00 00 00 00 00 * 00 00 00 00 00 00
		// 70: current command
		// sense key 0, sense code 0, extd sense code 0
		// additional transfer length * = sizeof(data) - 7
	}
#endif
#endif
	return USB_STOR_TRANSPORT_GOOD;		/* not result */
}

/*
 * Transport for the Sandisk SDDR-09
 */
int sddr09_transport(Scsi_Cmnd *srb, struct us_data *us)
{
	static unsigned char sense = 0;
	static unsigned char havefakesense = 0;
	int result, i;
	unsigned char *ptr;
	unsigned long capacity;
	unsigned int page, pages;
	char string[64];

	struct sddr09_card_info *info;

	unsigned char inquiry_response[36] = {
		0x00, 0x80, 0x00, 0x02, 0x1F, 0x00, 0x00, 0x00
	};

	unsigned char mode_page_01[16] = {
		0x0F, 0x00, 0, 0x00,
		0x01, 0x0A,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	info = (struct sddr09_card_info *)us->extra;
	if (!info) {
		nand_init_ecc();
		sddr09_init_card_info(us);
		info = (struct sddr09_card_info *)us->extra;
		if (!info)
			return USB_STOR_TRANSPORT_ERROR;
	}

	ptr = (unsigned char *)srb->request_buffer;

	if (srb->cmnd[0] == REQUEST_SENSE && havefakesense) {
		/* for a faked command, we have to follow with a faked sense */
		memset(ptr, 0, srb->request_bufflen);
		if (srb->request_bufflen > 7) {
			ptr[0] = 0x70;
			ptr[2] = sense;
			ptr[7] = srb->request_bufflen - 7;
		}
		sense = havefakesense = 0;
		return USB_STOR_TRANSPORT_GOOD;
	}

	sense = 0;
	havefakesense = 1;

	/* Dummy up a response for INQUIRY since SDDR09 doesn't
	   respond to INQUIRY commands */

	if (srb->cmnd[0] == INQUIRY) {
		memset(inquiry_response+8, 0, 28);
		fill_inquiry_response(us, inquiry_response, 36);
		return USB_STOR_TRANSPORT_GOOD;
	}

	if (srb->cmnd[0] == READ_CAPACITY) {
		struct nand_flash_dev *cardinfo;

		sddr09_get_wp(us, info);	/* read WP bit */

		cardinfo = sddr09_get_cardinfo(us, info->flags);
		if (!cardinfo)
			return USB_STOR_TRANSPORT_FAILED;

		info->capacity = (1 << cardinfo->chipshift);
		info->pageshift = cardinfo->pageshift;
		info->pagesize = (1 << info->pageshift);
		info->blockshift = cardinfo->blockshift;
		info->blocksize = (1 << info->blockshift);
		info->blockmask = info->blocksize - 1;

		// map initialization, must follow get_cardinfo()
		sddr09_read_map(us);

		// Report capacity

		capacity = (info->lbact << info->blockshift) - 1;

		ptr[0] = MSB_of(capacity>>16);
		ptr[1] = LSB_of(capacity>>16);
		ptr[2] = MSB_of(capacity&0xFFFF);
		ptr[3] = LSB_of(capacity&0xFFFF);

		// Report page size

		ptr[4] = MSB_of(info->pagesize>>16);
		ptr[5] = LSB_of(info->pagesize>>16);
		ptr[6] = MSB_of(info->pagesize&0xFFFF);
		ptr[7] = LSB_of(info->pagesize&0xFFFF);

		return USB_STOR_TRANSPORT_GOOD;
	}

	if (srb->cmnd[0] == MODE_SENSE) {

		// Read-write error recovery page: there needs to
		// be a check for write-protect here

		if ( (srb->cmnd[2] & 0x3F) == 0x01 ) {

			US_DEBUGP(
				"SDDR09: Dummy up request for mode page 1\n");

			if (ptr == NULL || 
			    srb->request_bufflen<sizeof(mode_page_01))
				return USB_STOR_TRANSPORT_ERROR;

			mode_page_01[0] = sizeof(mode_page_01) - 1;
			mode_page_01[2] = (info->flags & SDDR09_WP) ? 0x80 : 0;
			memcpy(ptr, mode_page_01, sizeof(mode_page_01));
			return USB_STOR_TRANSPORT_GOOD;

		} else if ( (srb->cmnd[2] & 0x3F) == 0x3F ) {

			US_DEBUGP("SDDR09: Dummy up request for "
				  "all mode pages\n");

			if (ptr == NULL || 
			    srb->request_bufflen<sizeof(mode_page_01))
				return USB_STOR_TRANSPORT_ERROR;

			memcpy(ptr, mode_page_01, sizeof(mode_page_01));
			return USB_STOR_TRANSPORT_GOOD;

		}

		return USB_STOR_TRANSPORT_ERROR;
	}

	if (srb->cmnd[0] == ALLOW_MEDIUM_REMOVAL) {

		US_DEBUGP(
			"SDDR09: %s medium removal. Not that I can do"
			" anything about it...\n",
			(srb->cmnd[4]&0x03) ? "Prevent" : "Allow");

		return USB_STOR_TRANSPORT_GOOD;

	}

	havefakesense = 0;

	if (srb->cmnd[0] == READ_10) {

		page = short_pack(srb->cmnd[3], srb->cmnd[2]);
		page <<= 16;
		page |= short_pack(srb->cmnd[5], srb->cmnd[4]);
		pages = short_pack(srb->cmnd[8], srb->cmnd[7]);

		US_DEBUGP("READ_10: read page %d pagect %d\n",
			  page, pages);

		return sddr09_read_data(us, page, pages, ptr, srb->use_sg);
	}

	if (srb->cmnd[0] == WRITE_10) {

		page = short_pack(srb->cmnd[3], srb->cmnd[2]);
		page <<= 16;
		page |= short_pack(srb->cmnd[5], srb->cmnd[4]);
		pages = short_pack(srb->cmnd[8], srb->cmnd[7]);

		US_DEBUGP("WRITE_10: write page %d pagect %d\n",
			  page, pages);

		return sddr09_write_data(us, page, pages, ptr, srb->use_sg);
	}

	// Pass TEST_UNIT_READY and REQUEST_SENSE through

	if (srb->cmnd[0] != TEST_UNIT_READY &&
	    srb->cmnd[0] != REQUEST_SENSE) {
		havefakesense = 1;
		return USB_STOR_TRANSPORT_ERROR;
	}

	for (; srb->cmd_len<12; srb->cmd_len++)
		srb->cmnd[srb->cmd_len] = 0;

	srb->cmnd[1] = 0x20;

	string[0] = 0;
	for (i=0; i<12; i++)
		sprintf(string+strlen(string), "%02X ", srb->cmnd[i]);

	US_DEBUGP("SDDR09: Send control for command %s\n",
		  string);

	result = sddr09_send_scsi_command(us, srb->cmnd, 12);
	if (result != USB_STOR_TRANSPORT_GOOD) {
		US_DEBUGP("sddr09_transport: sddr09_send_scsi_command "
			  "returns %d\n", result);
		return result;
	}

	if (srb->request_bufflen == 0)
		return USB_STOR_TRANSPORT_GOOD;

	if (srb->sc_data_direction == SCSI_DATA_WRITE ||
	    srb->sc_data_direction == SCSI_DATA_READ) {

		US_DEBUGP("SDDR09: %s %d bytes\n",
			  (srb->sc_data_direction == SCSI_DATA_WRITE) ?
			  "sending" : "receiving",
			  srb->request_bufflen);

		result = sddr09_bulk_transport(us,
					       srb->sc_data_direction,
					       srb->request_buffer, 
					       srb->request_bufflen,
					       srb->use_sg);

		return result;
	} 

	return USB_STOR_TRANSPORT_GOOD;
}

