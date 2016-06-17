/* Driver for USB Mass Storage compliant devices
 * Debugging Functions Source Code File
 *
 * $Id: debug.c,v 1.8 2002/02/25 00:40:13 mdharm Exp $
 *
 * Current development and maintenance by:
 *   (c) 1999-2002 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * Developed with the assistance of:
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

#include "debug.h"

void usb_stor_show_command(Scsi_Cmnd *srb)
{
	char *what = NULL;

	switch (srb->cmnd[0]) {
	case TEST_UNIT_READY: what = "TEST_UNIT_READY"; break;
	case REZERO_UNIT: what = "REZERO_UNIT"; break;
	case REQUEST_SENSE: what = "REQUEST_SENSE"; break;
	case FORMAT_UNIT: what = "FORMAT_UNIT"; break;
	case READ_BLOCK_LIMITS: what = "READ_BLOCK_LIMITS"; break;
	case REASSIGN_BLOCKS: what = "REASSIGN_BLOCKS"; break;
	case READ_6: what = "READ_6"; break;
	case WRITE_6: what = "WRITE_6"; break;
	case SEEK_6: what = "SEEK_6"; break;
	case READ_REVERSE: what = "READ_REVERSE"; break;
	case WRITE_FILEMARKS: what = "WRITE_FILEMARKS"; break;
	case SPACE: what = "SPACE"; break;
	case INQUIRY: what = "INQUIRY"; break;
	case RECOVER_BUFFERED_DATA: what = "RECOVER_BUFFERED_DATA"; break;
	case MODE_SELECT: what = "MODE_SELECT"; break;
	case RESERVE: what = "RESERVE"; break;
	case RELEASE: what = "RELEASE"; break;
	case COPY: what = "COPY"; break;
	case ERASE: what = "ERASE"; break;
	case MODE_SENSE: what = "MODE_SENSE"; break;
	case START_STOP: what = "START_STOP"; break;
	case RECEIVE_DIAGNOSTIC: what = "RECEIVE_DIAGNOSTIC"; break;
	case SEND_DIAGNOSTIC: what = "SEND_DIAGNOSTIC"; break;
	case ALLOW_MEDIUM_REMOVAL: what = "ALLOW_MEDIUM_REMOVAL"; break;
	case SET_WINDOW: what = "SET_WINDOW"; break;
	case READ_CAPACITY: what = "READ_CAPACITY"; break;
	case READ_10: what = "READ_10"; break;
	case WRITE_10: what = "WRITE_10"; break;
	case SEEK_10: what = "SEEK_10"; break;
	case WRITE_VERIFY: what = "WRITE_VERIFY"; break;
	case VERIFY: what = "VERIFY"; break;
	case SEARCH_HIGH: what = "SEARCH_HIGH"; break;
	case SEARCH_EQUAL: what = "SEARCH_EQUAL"; break;
	case SEARCH_LOW: what = "SEARCH_LOW"; break;
	case SET_LIMITS: what = "SET_LIMITS"; break;
	case READ_POSITION: what = "READ_POSITION"; break;
	case SYNCHRONIZE_CACHE: what = "SYNCHRONIZE_CACHE"; break;
	case LOCK_UNLOCK_CACHE: what = "LOCK_UNLOCK_CACHE"; break;
	case READ_DEFECT_DATA: what = "READ_DEFECT_DATA"; break;
	case MEDIUM_SCAN: what = "MEDIUM_SCAN"; break;
	case COMPARE: what = "COMPARE"; break;
	case COPY_VERIFY: what = "COPY_VERIFY"; break;
	case WRITE_BUFFER: what = "WRITE_BUFFER"; break;
	case READ_BUFFER: what = "READ_BUFFER"; break;
	case UPDATE_BLOCK: what = "UPDATE_BLOCK"; break;
	case READ_LONG: what = "READ_LONG"; break;
	case WRITE_LONG: what = "WRITE_LONG"; break;
	case CHANGE_DEFINITION: what = "CHANGE_DEFINITION"; break;
	case WRITE_SAME: what = "WRITE_SAME"; break;
	case GPCMD_READ_SUBCHANNEL: what = "READ SUBCHANNEL"; break;
	case READ_TOC: what = "READ_TOC"; break;
	case GPCMD_READ_HEADER: what = "READ HEADER"; break;
	case GPCMD_PLAY_AUDIO_10: what = "PLAY AUDIO (10)"; break;
	case GPCMD_PLAY_AUDIO_MSF: what = "PLAY AUDIO MSF"; break;
	case GPCMD_GET_EVENT_STATUS_NOTIFICATION:
		what = "GET EVENT/STATUS NOTIFICATION"; break;
	case GPCMD_PAUSE_RESUME: what = "PAUSE/RESUME"; break;
	case LOG_SELECT: what = "LOG_SELECT"; break;
	case LOG_SENSE: what = "LOG_SENSE"; break;
	case GPCMD_STOP_PLAY_SCAN: what = "STOP PLAY/SCAN"; break;
	case GPCMD_READ_DISC_INFO: what = "READ DISC INFORMATION"; break;
	case GPCMD_READ_TRACK_RZONE_INFO:
		what = "READ TRACK INFORMATION"; break;
	case GPCMD_RESERVE_RZONE_TRACK: what = "RESERVE TRACK"; break;
	case GPCMD_SEND_OPC: what = "SEND OPC"; break;
	case MODE_SELECT_10: what = "MODE_SELECT_10"; break;
	case GPCMD_REPAIR_RZONE_TRACK: what = "REPAIR TRACK"; break;
	case 0x59: what = "READ MASTER CUE"; break;
	case MODE_SENSE_10: what = "MODE_SENSE_10"; break;
	case GPCMD_CLOSE_TRACK: what = "CLOSE TRACK/SESSION"; break;
	case 0x5C: what = "READ BUFFER CAPACITY"; break;
	case 0x5D: what = "SEND CUE SHEET"; break;
	case GPCMD_BLANK: what = "BLANK"; break;
	case MOVE_MEDIUM: what = "MOVE_MEDIUM or PLAY AUDIO (12)"; break;
	case READ_12: what = "READ_12"; break;
	case WRITE_12: what = "WRITE_12"; break;
	case WRITE_VERIFY_12: what = "WRITE_VERIFY_12"; break;
	case SEARCH_HIGH_12: what = "SEARCH_HIGH_12"; break;
	case SEARCH_EQUAL_12: what = "SEARCH_EQUAL_12"; break;
	case SEARCH_LOW_12: what = "SEARCH_LOW_12"; break;
	case SEND_VOLUME_TAG: what = "SEND_VOLUME_TAG"; break;
	case READ_ELEMENT_STATUS: what = "READ_ELEMENT_STATUS"; break;
	case GPCMD_READ_CD_MSF: what = "READ CD MSF"; break;
	case GPCMD_SCAN: what = "SCAN"; break;
	case GPCMD_SET_SPEED: what = "SET CD SPEED"; break;
	case GPCMD_MECHANISM_STATUS: what = "MECHANISM STATUS"; break;
	case GPCMD_READ_CD: what = "READ CD"; break;
	case 0xE1: what = "WRITE CONTINUE"; break;
	case WRITE_LONG_2: what = "WRITE_LONG_2"; break;
	default: what = "(unknown command)"; break;
	}
	US_DEBUGP("Command %s (%d bytes)\n", what, srb->cmd_len);
	US_DEBUGP("%02x %02x %02x %02x "
		  "%02x %02x %02x %02x "
		  "%02x %02x %02x %02x\n",
		  srb->cmnd[0], srb->cmnd[1], srb->cmnd[2], srb->cmnd[3],
		  srb->cmnd[4], srb->cmnd[5], srb->cmnd[6], srb->cmnd[7],
		  srb->cmnd[8], srb->cmnd[9], srb->cmnd[10],
		  srb->cmnd[11]);
}

void usb_stor_print_Scsi_Cmnd( Scsi_Cmnd* cmd )
{
	int i=0, bufferSize = cmd->request_bufflen;
	u8* buffer = cmd->request_buffer;
	struct scatterlist* sg = (struct scatterlist*)cmd->request_buffer;

	US_DEBUGP("Dumping information about %p.\n", cmd );
	US_DEBUGP("cmd->cmnd[0] value is %d.\n", cmd->cmnd[0] );
	US_DEBUGP("(MODE_SENSE is %d and MODE_SENSE_10 is %d)\n",
		  MODE_SENSE, MODE_SENSE_10 );

	US_DEBUGP("buffer is %p with length %d.\n", buffer, bufferSize );
	for ( i=0; i<bufferSize; i+=16 )
	{
		US_DEBUGP("%02x %02x %02x %02x %02x %02x %02x %02x\n"
			  "%02x %02x %02x %02x %02x %02x %02x %02x\n",
			  buffer[i],
			  buffer[i+1],
			  buffer[i+2],
			  buffer[i+3],
			  buffer[i+4],
			  buffer[i+5],
			  buffer[i+6],
			  buffer[i+7],
			  buffer[i+8],
			  buffer[i+9],
			  buffer[i+10],
			  buffer[i+11],
			  buffer[i+12],
			  buffer[i+13],
			  buffer[i+14],
			  buffer[i+15] );
	}

	US_DEBUGP("Buffer has %d scatterlists.\n", cmd->use_sg );
	for ( i=0; i<cmd->use_sg; i++ )
	{
		US_DEBUGP("Length of scatterlist %d is %d.\n",i,sg[i].length);
		US_DEBUGP("%02x %02x %02x %02x %02x %02x %02x %02x\n"
			  "%02x %02x %02x %02x %02x %02x %02x %02x\n",
			  sg[i].address[0],
			  sg[i].address[1],
			  sg[i].address[2],
			  sg[i].address[3],
			  sg[i].address[4],
			  sg[i].address[5],
			  sg[i].address[6],
			  sg[i].address[7],
			  sg[i].address[8],
			  sg[i].address[9],
			  sg[i].address[10],
			  sg[i].address[11],
			  sg[i].address[12],
			  sg[i].address[13],
			  sg[i].address[14],
			  sg[i].address[15]);
	}
}

void usb_stor_show_sense(
		unsigned char key,
		unsigned char asc,
		unsigned char ascq) {

	char *keys[] = {
		"No Sense",
		"Recovered Error",
		"Not Ready",
		"Medium Error",
		"Hardware Error",
		"Illegal Request",
		"Unit Attention",
		"Data Protect",
		"Blank Check",
		"Vendor Specific",
		"Copy Aborted",
		"Aborted Command",
		"(Obsolete)",
		"Volume Overflow",
		"Miscompare"
	};

	unsigned short qual = asc;

	char *what = 0;
	char *keystr = 0;

	qual <<= 8;
	qual |= ascq;

	if (key>0x0E)
		keystr = "(Unknown Key)";
	else
		keystr = keys[key];

	switch (qual) {

	case 0x0000: what="no additional sense information"; break;
	case 0x0001: what="filemark detected"; break;
	case 0x0002: what="end of partition/medium detected"; break;
	case 0x0003: what="setmark detected"; break;
	case 0x0004: what="beginning of partition/medium detected"; break;
	case 0x0005: what="end of data detected"; break;
	case 0x0006: what="I/O process terminated"; break;
	case 0x0011: what="audio play operation in progress"; break;
	case 0x0012: what="audio play operation paused"; break;
	case 0x0013: what="audio play operation stopped due to error"; break;
	case 0x0014: what="audio play operation successfully completed"; break;
	case 0x0015: what="no current audio status to return"; break;
	case 0x0016: what="operation in progress"; break;
	case 0x0017: what="cleaning requested"; break;
	case 0x0100: what="no index/sector signal"; break;
	case 0x0200: what="no seek complete"; break;
	case 0x0300: what="peripheral device write fault"; break;
	case 0x0301: what="no write current"; break;
	case 0x0302: what="excessive write errors"; break;
	case 0x0400: what="LUN not ready, cause not reportable"; break;
	case 0x0401: what="LUN in process of becoming ready"; break;
	case 0x0402: what="LUN not ready, initializing cmd. required"; break;
	case 0x0403: what="LUN not ready, manual intervention required"; break;
	case 0x0404: what="LUN not ready, format in progress"; break;
	case 0x0405: what="LUN not ready, rebuild in progress"; break;
	case 0x0406: what="LUN not ready, recalculation in progress"; break;
	case 0x0407: what="LUN not ready, operation in progress"; break;
	case 0x0408: what="LUN not ready, long write in progress"; break;
	case 0x0500: what="LUN doesn't respond to selection"; break;
	case 0x0A00: what="error log overflow"; break;
	case 0x0C04: what="compression check miscompare error"; break;
	case 0x0C05: what="data expansion occurred during compression"; break;
	case 0x0C06: what="block not compressible"; break;
	case 0x1102: what="error too long to correct"; break;
	case 0x1106: what="CIRC unrecovered error"; break;
	case 0x1107: what="data resynchronization error"; break;
	case 0x110D: what="decompression CRC error"; break;
	case 0x110E: what="can't decompress using declared algorithm"; break;
	case 0x110F: what="error reading UPC/EAN number"; break;
	case 0x1110: what="error reading ISRC number"; break;
	case 0x1200: what="address mark not found for ID field"; break;
	case 0x1300: what="address mark not found for data field"; break;
	case 0x1403: what="end of data not found"; break;
	case 0x1404: what="block sequence error"; break;
	case 0x1600: what="data sync mark error"; break;
	case 0x1601: what="data sync error: data rewritten"; break;
	case 0x1602: what="data sync error: recommend rewrite"; break;
	case 0x1603: what="data sync error: data auto-reallocated"; break;
	case 0x1604: what="data sync error: recommend reassignment"; break;
	case 0x1900: what="defect list error"; break;
	case 0x1901: what="defect list not available"; break;
	case 0x1902: what="defect list error in primary list"; break;
	case 0x1903: what="defect list error in grown list"; break;
	case 0x1C00: what="defect list not found"; break;
	case 0x2000: what="invalid command operation code"; break;
	case 0x2400: what="invalid field in CDB"; break;
	case 0x2703: what="associated write protect"; break;
	case 0x2800: what="not ready to ready transition"; break;
	case 0x2900: what="device reset occurred"; break;
	case 0x2903: what="bus device reset function occurred"; break;
	case 0x2904: what="device internal reset"; break;
	case 0x2B00: what="copy can't execute / host can't disconnect"; break;
	case 0x2C00: what="command sequence error"; break;
	case 0x2C03: what="current program area is not empty"; break;
	case 0x2C04: what="current program area is empty"; break;
	case 0x2F00: what="commands cleared by another initiator"; break;
	case 0x3001: what="can't read medium: unknown format"; break;
	case 0x3002: what="can't read medium: incompatible format"; break;
	case 0x3003: what="cleaning cartridge installed"; break;
	case 0x3004: what="can't write medium: unknown format"; break;
	case 0x3005: what="can't write medium: incompatible format"; break;
	case 0x3006: what="can't format medium: incompatible medium"; break;
	case 0x3007: what="cleaning failure"; break;
	case 0x3008: what="can't write: application code mismatch"; break;
	case 0x3009: what="current session not fixated for append"; break;
	case 0x3201: what="defect list update failure"; break;
	case 0x3400: what="enclosure failure"; break;
	case 0x3500: what="enclosure services failure"; break;
	case 0x3502: what="enclosure services unavailable"; break;
	case 0x3503: what="enclosure services transfer failure"; break;
	case 0x3504: what="enclosure services transfer refused"; break;
	case 0x3A00: what="media not present"; break;
	case 0x3B0F: what="end of medium reached"; break;
	case 0x3F02: what="changed operating definition"; break;
	case 0x4100: what="data path failure (should use 40 NN)"; break;
	case 0x4A00: what="command phase error"; break;
	case 0x4B00: what="data phase error"; break;
	case 0x5100: what="erase failure"; break;
	case 0x5200: what="cartridge fault"; break;
	case 0x6300: what="end of user area encountered on this track"; break;
	case 0x6600: what="automatic document feeder cover up"; break;
	case 0x6601: what="automatic document feeder lift up"; break;
	case 0x6602: what="document jam in auto doc feeder"; break;
	case 0x6603: what="document miss feed auto in doc feeder"; break;
	case 0x6700: what="configuration failure"; break;
	case 0x6701: what="configuration of incapable LUN's failed"; break;
	case 0x6702: what="add logical unit failed"; break;
	case 0x6706: what="attachment of logical unit failed"; break;
	case 0x6707: what="creation of logical unit failed"; break;
	case 0x6900: what="data loss on logical unit"; break;
	case 0x6E00: what="command to logical unit failed"; break;
	case 0x7100: what="decompression exception long algorithm ID"; break;
	case 0x7204: what="empty or partially written reserved track"; break;
	case 0x7300: what="CD control error"; break;

	default:
		if (asc==0x40) {
			US_DEBUGP("%s: diagnostic failure on component"
			  " %02X\n", keystr, ascq);
			return;
		}
		if (asc==0x70) {
			US_DEBUGP("%s: decompression exception short"
			  " algorithm ID of %02X\n", keystr, ascq);
			return;
		}
		what = "(unknown ASC/ASCQ)";
	}

	US_DEBUGP("%s: %s\n", keystr, what);
}

