/*matcddrv.h------------------------------------------------------------------

	Matsushita(Panasonic) / Creative CD-ROM Driver	(matcd)
	Authored by Frank Durda IV

Copyright 1994, 1995, 2002, 2003  Frank Durda IV.  All rights reserved.
"FDIV" is a trademark of Frank Durda IV.

------------------------------------------------------------------------------

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the author nor the names of their contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

------------------------------------------------------------------------------

See matcd.c for Edit History information.
*/

/* $FreeBSD$
*/

/*----------------------------------------------------------------------------
	Matsushita CR562/CR563 Commands

	These equates are for commands accepted by the Matsushita drive.
	This is not a complete list - just the ones that this driver uses.
----------------------------------------------------------------------------*/

#define	NOP		0x05		/*No action - just return status*/
#define	DOOROPEN	0x06		/*Open tray*/
#define DOORCLOSE	0x07		/*Close tray*/
#define ABORT		0x08		/*Abort command*/
#define MODESELECT	0x09		/*Set drive parameters*/
#define LOCK		0x0c		/*Prevent/Allow medium removal*/
#define	PAUSE		0x0d		/*Pause/Resume playback*/
#define PLAYBLOCKS	0x0e		/*Play audio - block to block range*/
#define PLAYTRKS	0x0f		/*Play audio - tracks & index*/
#define	READ		0x10		/*Read data*/
#define READERROR	0x82		/*Read Error*/
#define	READID		0x83		/*Read Drive Type & Firmware Info*/
#define MODESENSE	0x84		/*Report drive settings*/
#define READSUBQ	0x87		/*Read Q channel information*/
#define	READDINFO	0x8b		/*Read TOC tracks & drive size*/
#define	READTOC		0x8c		/*Read entry from TOC*/

#define BLOCKPARAM	0x00		/*Used with MODESELECT command*/
#define SPEEDPARM	0x03		/*Adjust audio playback speed*/
#define	AUDIOPARM	0x05		/*Set/read audio levels & routing*/
#define RESUME		0x80		/*Used with PAUSE command*/

#define MAXCMDSIZ	12		/*Max command size with NULL*/

/*	Possible data transfers for MODESELECT + BLOCKPARAM */

#define MODE_DATA	0x00		/*2048, 2340*/
#define MODE_DA		0x82		/*2352*/
#define MODE_USER	0x01		/*2048, 2052, 2336, 2340, 2352*/
#define MODE_UNKNOWN	0xff		/*Uninitialized state*/
#define MODE_XA		0x81		/*2048, 2060, 2324, 2336, 2340, 2352*/
					/*The driver does not implement XA.*/
#define DEFVOL		0xff		/*Default drive volume level, 100%
					  volume.  Based on drive action.*/
#define OUTLEFT		0x01		/*Output on Left*/
#define OUTRIGHT	0x02		/*Output on Right*/


/*	 Matsushita CR562/CR563 Status bits*/

#define	MATCD_ST_DOOROPEN	0x80	/*Door is open right now*/
#define	MATCD_ST_DSKIN		0x40	/*Disc in drive*/
#define	MATCD_ST_SPIN		0x20	/*Disc is spinning*/
#define MATCD_ST_ERROR		0x10	/*Error on command*/
#define	MATCD_ST_AUDIOBSY	0x08	/*Drive is playing audio*/
#define MATCD_ST_LOCK		0x04	/*Drive is locked*/
#define MATCD_ST_X2		0x02	/*Media is at double-speed*/
#define MATCD_ST_READY		0x01	/*Drive is ready*/


/*	Error codes returned from READERROR command.
	(The hardware vendor did not provide any additional information
	 on what conditions generate these errors.)
*/

#define	NO_ERROR	0x00
#define	RECV_RETRY	0x01
#define	RECV_ECC	0x02
#define	NOT_READY	0x03
#define	TOC_ERROR	0x04
#define	UNRECV_ERROR	0x05
#define	SEEK_ERROR	0x06
#define	TRACK_ERROR	0x07
#define	RAM_ERROR	0x08
#define	DIAG_ERROR	0x09
#define	FOCUS_ERROR	0x0a
#define	CLV_ERROR	0x0b
#define	DATA_ERROR	0x0c
#define	ADDRESS_ERROR	0x0d
#define	CDB_ERROR	0x0e
#define	END_ADDRESS	0x0f
#define	MODE_ERROR	0x10
#define	MEDIA_CHANGED	0x11
#define	HARD_RESET	0x12
#define	ROM_ERROR	0x13
#define	CMD_ERROR	0x14
#define	DISC_OUT	0x15
#define	HARD_ERROR	0x16
#define	ILLEGAL_REQ	0x17


/*	Human-readable error messages - what a concept!*/

static unsigned char * matcderrors[]={"No error",	/* 00 */
			"Soft read error after retry",	/* 01 */
			"Soft read error after error-correction", /* 02 */
			"Not ready",			/* 03 */
			"Unable to read TOC",		/* 04 */
			"Hard read error of data track",/* 05 */
			"Seek did not complete",	/* 06 */
			"Tracking servo failure",	/* 07 */
			"Drive RAM failure",		/* 08 */
			"Drive self-test failed",	/* 09 */
			"Focusing servo failure",	/* 0a */
			"Spindle servo failure",	/* 0b */
			"Data path failure",		/* 0c */
			"Illegal logical block address",/* 0d */
			"Illegal field in CDB",		/* 0e */
			"End of user encountered on this track", /* 0f */
			"Illegal data mode for this track",	/* 10 */
			"Media changed",		/* 11 */
			"Power-on or drive reset occurred",	/* 12 */
			"Drive ROM failure",		/* 13 */
			"Illegal drive command received from host",/* 14 */
			"Disc removed during operation",/* 15 */
			"Drive Hardware error",		/* 16 */
			"Illegal request from host"};	/* 17 */

/*End of matcddrv.h*/

