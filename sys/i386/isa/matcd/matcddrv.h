/*matcd.h---------------------------------------------------------------------

	Matsushita(Panasonic) / Creative CD-ROM Driver	(matcd)
	Authored by Frank Durda IV

	Copyright 1994, 1995  Frank Durda IV.  All rights reserved.
	"FDIV" is a trademark of Frank Durda IV.


	Redistribution and use in source and binary forms, with or
	without modification, are permitted provided that the following
	conditions are met:
	1.  Redistributions of source code must retain the above copyright
	    notice positioned at the very beginning of this file without
	    modification, all copyright strings, all related programming
	    codes that display the copyright strings, this list of
	    conditions and the following disclaimer.
	2.  Redistributions in binary form must contain all copyright strings
	    and related programming code that display the copyright strings.
	3.  Redistributions in binary form must reproduce the above copyright
	    notice, this list of conditions and the following disclaimer in
	    the documentation and/or other materials provided with the
	    distribution.
	4.  All advertising materials mentioning features or use of this
	    software must display the following acknowledgement:
		"The Matsushita/Panasonic CD-ROM driver  was developed
		 by Frank Durda IV for use with "FreeBSD" and similar
		 operating systems."
	    "Similar operating systems" includes mainly non-profit oriented
	    systems for research and education, including but not restricted
	    to "NetBSD", "386BSD", and "Mach" (by CMU).  The wording of the
	    acknowledgement (in electronic form or printed text) may not be
	    changed without permission from the author.
	5.  Absolutely no warranty of function, fitness or purpose is made
	    by the author Frank Durda IV.
	6.  Neither the name of the author nor the name "FreeBSD" may
	    be used to endorse or promote products derived from this software
	    without specific prior written permission.
	    (The author can be reached at   bsdmail@nemesis.lonestar.org)
	7.  The product containing this software must meet all of these
	    conditions even if it is unsupported, not a complete system
	    and/or does not contain compiled code.
	8.  These conditions will be in force for the full life of the
	    copyright.
	9.  If all the above conditions are met, modifications to other
	    parts of this file may be freely made, although any person
	    or persons making changes do not receive the right to add their
	    name or names to the copyright strings and notices in this
	    software.  Persons making changes are encouraged to insert edit
	    history in matcd.c and to put your name and details of the
	    change there.
	10. You must have prior written permission from the author to
	    deviate from these terms.

	Vendors who produce product(s) containing this code are encouraged
	(but not required) to provide copies of the finished product(s) to
	the author and to correspond with the author about development
	activity relating to this code.   Donations of development hardware
	and/or software are also welcome.  (This is one of the faster ways
	to get a driver developed for a device.)

 	THIS SOFTWARE IS PROVIDED BY THE DEVELOPER(S) ``AS IS'' AND ANY
 	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 	PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER(S) BE
 	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 	OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


-----No changes are allowed above this line------------------------------------

See matcd.c for Edit History information.


	Matsushita CR562/CR563 Commands
	This is not a complete list - just the ones this version uses
*/

#define	NOP		0x05		/*No action - just return status*/
#define	DOOROPEN	0x06		/*Open tray*/
#define DOORCLOSE	0x07		/*Close tray*/
#define ABORT		0x08		/*Abort command*/
#define MODESELECT	0x09		/*Set drive parameters*/
#define LOCK		0x0c		/*Prevent/Allow medium removal*/
#define	PAUSE		0x0d		/*Pause/Resume playback*/
#define PLAYBLOCKS	0x0e		/*Play audio - block to block*/
#define PLAYTRKS	0x0f		/*Play audio - tracks & index*/
#define	READ		0x10		/*Read data*/
#define READERROR	0x82		/*Read Error*/
#define	READID		0x83		/*Read Drive Type & Firmware Info*/
#define MODESENSE	0x84		/*<12>Report drive settings*/
#define READSUBQ	0x87		/*<14>Read Q channel information*/
#define	READDINFO	0x8b		/*<13>Read TOC tracks & drive size*/
#define	READTOC		0x8c		/*<13>Read entry from TOC*/

#define BLOCKPARAM	0x00		/*Used with MODESELECT command*/
#define SPEEDPARM	0x03		/*<12>Adjust audio playback speed*/
#define	AUDIOPARM	0x05		/*<12>Set/read audio levels & routing*/
#define RESUME		0x80		/*Used with PAUSE command*/

#define MAXCMDSIZ	12		/*Max command size with NULL*/

/*	Possible data transfers for MODESELECT + BLOCKPARAM */

#define MODE_DATA	0x00		/*2048, 2340*/
#define MODE_DA		0x82		/*2352*/
#define MODE_USER	0x01		/*2048, 2052, 2336, 2340, 2352*/
#define MODE_UNKNOWN	0xff		/*Uninitialized state*/

/*<12>The following mode is not implemented in the driver at this time*/

#define MODE_XA		0x81		/*2048, 2060, 2324, 2336, 2340, 2352*/

#define DEFVOL		0xff		/*<12>Default drive volume level, 100%
					  volume.  Based on drive action.*/
#define OUTLEFT		0x01		/*Output on Left*/
#define OUTRIGHT	0x02		/*Output on Right*/

/*	 Matsushita CR562/CR563 Status bits*/

#define	MATCD_ST_DOOROPEN	0x80	/*Door is open right now*/
#define	MATCD_ST_DSKIN		0x40	/*Disc in drive*/
#define	MATCD_ST_SPIN		0x20	/*Disc is spinning*/
#define MATCD_ST_ERROR		0x10	/*Error on command*/
#define	MATCD_ST_AUDIOBSY	0x08	/*Drive is playing audio*/
#define MATCD_ST_LOCK		0x04	/*<14>Drive is locked*/
#define MATCD_ST_X2		0x02	/*<14>Media is at double-speed*/
#define MATCD_ST_READY		0x01	/*<14>Drive is ready*/

#define	MATCDAUDIOBSY	MATCD_ST_AUDIOBSY
#define MATCDDSKCHNG	MATCD_ST_DSKCHNG
#define	MATCDDSKIN	MATCD_ST_DSKIN
#define	MATCDDOOROPEN	MATCD_ST_DOOROPEN


/*	Error codes returned from READERROR command.*/

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

/*End of matcd.h*/


