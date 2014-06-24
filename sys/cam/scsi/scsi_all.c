/*-
 * Implementation of Utility functions for all SCSI device types.
 *
 * Copyright (c) 1997, 1998, 1999 Justin T. Gibbs.
 * Copyright (c) 1997, 1998, 2003 Kenneth D. Merry.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stdint.h>

#ifdef _KERNEL
#include <opt_scsi.h>

#include <sys/systm.h>
#include <sys/libkern.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#else
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_queue.h>
#include <cam/cam_xpt.h>
#include <cam/scsi/scsi_all.h>
#include <sys/ata.h>
#include <sys/sbuf.h>

#ifdef _KERNEL
#include <cam/cam_periph.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_internal.h>
#else
#include <camlib.h>
#include <stddef.h>

#ifndef FALSE
#define FALSE   0
#endif /* FALSE */
#ifndef TRUE
#define TRUE    1
#endif /* TRUE */
#define ERESTART        -1              /* restart syscall */
#define EJUSTRETURN     -2              /* don't modify regs, just return */
#endif /* !_KERNEL */

/*
 * This is the default number of milliseconds we wait for devices to settle
 * after a SCSI bus reset.
 */
#ifndef SCSI_DELAY
#define SCSI_DELAY 2000
#endif
/*
 * All devices need _some_ sort of bus settle delay, so we'll set it to
 * a minimum value of 100ms. Note that this is pertinent only for SPI-
 * not transport like Fibre Channel or iSCSI where 'delay' is completely
 * meaningless.
 */
#ifndef SCSI_MIN_DELAY
#define SCSI_MIN_DELAY 100
#endif
/*
 * Make sure the user isn't using seconds instead of milliseconds.
 */
#if (SCSI_DELAY < SCSI_MIN_DELAY && SCSI_DELAY != 0)
#error "SCSI_DELAY is in milliseconds, not seconds!  Please use a larger value"
#endif

int scsi_delay;

static int	ascentrycomp(const void *key, const void *member);
static int	senseentrycomp(const void *key, const void *member);
static void	fetchtableentries(int sense_key, int asc, int ascq,
				  struct scsi_inquiry_data *,
				  const struct sense_key_table_entry **,
				  const struct asc_table_entry **);
#ifdef _KERNEL
static void	init_scsi_delay(void);
static int	sysctl_scsi_delay(SYSCTL_HANDLER_ARGS);
static int	set_scsi_delay(int delay);
#endif

#if !defined(SCSI_NO_OP_STRINGS)

#define	D	(1 << T_DIRECT)
#define	T	(1 << T_SEQUENTIAL)
#define	L	(1 << T_PRINTER)
#define	P	(1 << T_PROCESSOR)
#define	W	(1 << T_WORM)
#define	R	(1 << T_CDROM)
#define	O	(1 << T_OPTICAL)
#define	M	(1 << T_CHANGER)
#define	A	(1 << T_STORARRAY)
#define	E	(1 << T_ENCLOSURE)
#define	B	(1 << T_RBC)
#define	K	(1 << T_OCRW)
#define	V	(1 << T_ADC)
#define	F	(1 << T_OSD)
#define	S	(1 << T_SCANNER)
#define	C	(1 << T_COMM)

#define ALL	(D | T | L | P | W | R | O | M | A | E | B | K | V | F | S | C)

static struct op_table_entry plextor_cd_ops[] = {
	{ 0xD8, R, "CD-DA READ" }
};

static struct scsi_op_quirk_entry scsi_op_quirk_table[] = {
	{
		/*
		 * I believe that 0xD8 is the Plextor proprietary command
		 * to read CD-DA data.  I'm not sure which Plextor CDROM
		 * models support the command, though.  I know for sure
		 * that the 4X, 8X, and 12X models do, and presumably the
		 * 12-20X does.  I don't know about any earlier models,
		 * though.  If anyone has any more complete information,
		 * feel free to change this quirk entry.
		 */
		{T_CDROM, SIP_MEDIA_REMOVABLE, "PLEXTOR", "CD-ROM PX*", "*"},
		sizeof(plextor_cd_ops)/sizeof(struct op_table_entry),
		plextor_cd_ops
	}
};

static struct op_table_entry scsi_op_codes[] = {
	/*
	 * From: http://www.t10.org/lists/op-num.txt
	 * Modifications by Kenneth Merry (ken@FreeBSD.ORG)
	 *              and Jung-uk Kim (jkim@FreeBSD.org)
	 *
	 * Note:  order is important in this table, scsi_op_desc() currently
	 * depends on the opcodes in the table being in order to save
	 * search time.
	 * Note:  scanner and comm. devices are carried over from the previous
	 * version because they were removed in the latest spec.
	 */
	/* File: OP-NUM.TXT
	 *
	 * SCSI Operation Codes
	 * Numeric Sorted Listing
	 * as of  3/11/08
	 *
	 *     D - DIRECT ACCESS DEVICE (SBC-2)                device column key
	 *     .T - SEQUENTIAL ACCESS DEVICE (SSC-2)           -----------------
	 *     . L - PRINTER DEVICE (SSC)                      M = Mandatory
	 *     .  P - PROCESSOR DEVICE (SPC)                   O = Optional
	 *     .  .W - WRITE ONCE READ MULTIPLE DEVICE (SBC-2) V = Vendor spec.
	 *     .  . R - CD/DVE DEVICE (MMC-3)                  Z = Obsolete
	 *     .  .  O - OPTICAL MEMORY DEVICE (SBC-2)
	 *     .  .  .M - MEDIA CHANGER DEVICE (SMC-2)
	 *     .  .  . A - STORAGE ARRAY DEVICE (SCC-2)
	 *     .  .  . .E - ENCLOSURE SERVICES DEVICE (SES)
	 *     .  .  .  .B - SIMPLIFIED DIRECT-ACCESS DEVICE (RBC)
	 *     .  .  .  . K - OPTICAL CARD READER/WRITER DEVICE (OCRW)
	 *     .  .  .  .  V - AUTOMATION/DRIVE INTERFACE (ADC)
	 *     .  .  .  .  .F - OBJECT-BASED STORAGE (OSD)
	 * OP  DTLPWROMAEBKVF  Description
	 * --  --------------  ---------------------------------------------- */
	/* 00  MMMMMMMMMMMMMM  TEST UNIT READY */
	{ 0x00,	ALL, "TEST UNIT READY" },
	/* 01   M              REWIND */
	{ 0x01,	T, "REWIND" },
	/* 01  Z V ZZZZ        REZERO UNIT */
	{ 0x01,	D | W | R | O | M, "REZERO UNIT" },
	/* 02  VVVVVV V */
	/* 03  MMMMMMMMMMOMMM  REQUEST SENSE */
	{ 0x03,	ALL, "REQUEST SENSE" },
	/* 04  M    OO         FORMAT UNIT */
	{ 0x04,	D | R | O, "FORMAT UNIT" },
	/* 04   O              FORMAT MEDIUM */
	{ 0x04,	T, "FORMAT MEDIUM" },
	/* 04    O             FORMAT */
	{ 0x04,	L, "FORMAT" },
	/* 05  VMVVVV V        READ BLOCK LIMITS */
	{ 0x05,	T, "READ BLOCK LIMITS" },
	/* 06  VVVVVV V */
	/* 07  OVV O OV        REASSIGN BLOCKS */
	{ 0x07,	D | W | O, "REASSIGN BLOCKS" },
	/* 07         O        INITIALIZE ELEMENT STATUS */
	{ 0x07,	M, "INITIALIZE ELEMENT STATUS" },
	/* 08  MOV O OV        READ(6) */
	{ 0x08,	D | T | W | O, "READ(6)" },
	/* 08     O            RECEIVE */
	{ 0x08,	P, "RECEIVE" },
	/* 08                  GET MESSAGE(6) */
	{ 0x08, C, "GET MESSAGE(6)" },
	/* 09  VVVVVV V */
	/* 0A  OO  O OV        WRITE(6) */
	{ 0x0A,	D | T | W | O, "WRITE(6)" },
	/* 0A     M            SEND(6) */
	{ 0x0A,	P, "SEND(6)" },
	/* 0A                  SEND MESSAGE(6) */
	{ 0x0A, C, "SEND MESSAGE(6)" },
	/* 0A    M             PRINT */
	{ 0x0A,	L, "PRINT" },
	/* 0B  Z   ZOZV        SEEK(6) */
	{ 0x0B,	D | W | R | O, "SEEK(6)" },
	/* 0B   O              SET CAPACITY */
	{ 0x0B,	T, "SET CAPACITY" },
	/* 0B    O             SLEW AND PRINT */
	{ 0x0B,	L, "SLEW AND PRINT" },
	/* 0C  VVVVVV V */
	/* 0D  VVVVVV V */
	/* 0E  VVVVVV V */
	/* 0F  VOVVVV V        READ REVERSE(6) */
	{ 0x0F,	T, "READ REVERSE(6)" },
	/* 10  VM VVV          WRITE FILEMARKS(6) */
	{ 0x10,	T, "WRITE FILEMARKS(6)" },
	/* 10    O             SYNCHRONIZE BUFFER */
	{ 0x10,	L, "SYNCHRONIZE BUFFER" },
	/* 11  VMVVVV          SPACE(6) */
	{ 0x11,	T, "SPACE(6)" },
	/* 12  MMMMMMMMMMMMMM  INQUIRY */
	{ 0x12,	ALL, "INQUIRY" },
	/* 13  V VVVV */
	/* 13   O              VERIFY(6) */
	{ 0x13,	T, "VERIFY(6)" },
	/* 14  VOOVVV          RECOVER BUFFERED DATA */
	{ 0x14,	T | L, "RECOVER BUFFERED DATA" },
	/* 15  OMO O OOOO OO   MODE SELECT(6) */
	{ 0x15,	ALL & ~(P | R | B | F), "MODE SELECT(6)" },
	/* 16  ZZMZO OOOZ O    RESERVE(6) */
	{ 0x16,	ALL & ~(R | B | V | F | C), "RESERVE(6)" },
	/* 16         Z        RESERVE ELEMENT(6) */
	{ 0x16,	M, "RESERVE ELEMENT(6)" },
	/* 17  ZZMZO OOOZ O    RELEASE(6) */
	{ 0x17,	ALL & ~(R | B | V | F | C), "RELEASE(6)" },
	/* 17         Z        RELEASE ELEMENT(6) */
	{ 0x17,	M, "RELEASE ELEMENT(6)" },
	/* 18  ZZZZOZO    Z    COPY */
	{ 0x18,	D | T | L | P | W | R | O | K | S, "COPY" },
	/* 19  VMVVVV          ERASE(6) */
	{ 0x19,	T, "ERASE(6)" },
	/* 1A  OMO O OOOO OO   MODE SENSE(6) */
	{ 0x1A,	ALL & ~(P | R | B | F), "MODE SENSE(6)" },
	/* 1B  O   OOO O MO O  START STOP UNIT */
	{ 0x1B,	D | W | R | O | A | B | K | F, "START STOP UNIT" },
	/* 1B   O          M   LOAD UNLOAD */
	{ 0x1B,	T | V, "LOAD UNLOAD" },
	/* 1B                  SCAN */
	{ 0x1B, S, "SCAN" },
	/* 1B    O             STOP PRINT */
	{ 0x1B,	L, "STOP PRINT" },
	/* 1B         O        OPEN/CLOSE IMPORT/EXPORT ELEMENT */
	{ 0x1B,	M, "OPEN/CLOSE IMPORT/EXPORT ELEMENT" },
	/* 1C  OOOOO OOOM OOO  RECEIVE DIAGNOSTIC RESULTS */
	{ 0x1C,	ALL & ~(R | B), "RECEIVE DIAGNOSTIC RESULTS" },
	/* 1D  MMMMM MMOM MMM  SEND DIAGNOSTIC */
	{ 0x1D,	ALL & ~(R | B), "SEND DIAGNOSTIC" },
	/* 1E  OO  OOOO   O O  PREVENT ALLOW MEDIUM REMOVAL */
	{ 0x1E,	D | T | W | R | O | M | K | F, "PREVENT ALLOW MEDIUM REMOVAL" },
	/* 1F */
	/* 20  V   VVV    V */
	/* 21  V   VVV    V */
	/* 22  V   VVV    V */
	/* 23  V   V V    V */
	/* 23       O          READ FORMAT CAPACITIES */
	{ 0x23,	R, "READ FORMAT CAPACITIES" },
	/* 24  V   VV          SET WINDOW */
	{ 0x24, S, "SET WINDOW" },
	/* 25  M   M M   M     READ CAPACITY(10) */
	{ 0x25,	D | W | O | B, "READ CAPACITY(10)" },
	/* 25       O          READ CAPACITY */
	{ 0x25,	R, "READ CAPACITY" },
	/* 25             M    READ CARD CAPACITY */
	{ 0x25,	K, "READ CARD CAPACITY" },
	/* 25                  GET WINDOW */
	{ 0x25, S, "GET WINDOW" },
	/* 26  V   VV */
	/* 27  V   VV */
	/* 28  M   MOM   MM    READ(10) */
	{ 0x28,	D | W | R | O | B | K | S, "READ(10)" },
	/* 28                  GET MESSAGE(10) */
	{ 0x28, C, "GET MESSAGE(10)" },
	/* 29  V   VVO         READ GENERATION */
	{ 0x29,	O, "READ GENERATION" },
	/* 2A  O   MOM   MO    WRITE(10) */
	{ 0x2A,	D | W | R | O | B | K, "WRITE(10)" },
	/* 2A                  SEND(10) */
	{ 0x2A, S, "SEND(10)" },
	/* 2A                  SEND MESSAGE(10) */
	{ 0x2A, C, "SEND MESSAGE(10)" },
	/* 2B  Z   OOO    O    SEEK(10) */
	{ 0x2B,	D | W | R | O | K, "SEEK(10)" },
	/* 2B   O              LOCATE(10) */
	{ 0x2B,	T, "LOCATE(10)" },
	/* 2B         O        POSITION TO ELEMENT */
	{ 0x2B,	M, "POSITION TO ELEMENT" },
	/* 2C  V    OO         ERASE(10) */
	{ 0x2C,	R | O, "ERASE(10)" },
	/* 2D        O         READ UPDATED BLOCK */
	{ 0x2D,	O, "READ UPDATED BLOCK" },
	/* 2D  V */
	/* 2E  O   OOO   MO    WRITE AND VERIFY(10) */
	{ 0x2E,	D | W | R | O | B | K, "WRITE AND VERIFY(10)" },
	/* 2F  O   OOO         VERIFY(10) */
	{ 0x2F,	D | W | R | O, "VERIFY(10)" },
	/* 30  Z   ZZZ         SEARCH DATA HIGH(10) */
	{ 0x30,	D | W | R | O, "SEARCH DATA HIGH(10)" },
	/* 31  Z   ZZZ         SEARCH DATA EQUAL(10) */
	{ 0x31,	D | W | R | O, "SEARCH DATA EQUAL(10)" },
	/* 31                  OBJECT POSITION */
	{ 0x31, S, "OBJECT POSITION" },
	/* 32  Z   ZZZ         SEARCH DATA LOW(10) */
	{ 0x32,	D | W | R | O, "SEARCH DATA LOW(10)" },
	/* 33  Z   OZO         SET LIMITS(10) */
	{ 0x33,	D | W | R | O, "SET LIMITS(10)" },
	/* 34  O   O O    O    PRE-FETCH(10) */
	{ 0x34,	D | W | O | K, "PRE-FETCH(10)" },
	/* 34   M              READ POSITION */
	{ 0x34,	T, "READ POSITION" },
	/* 34                  GET DATA BUFFER STATUS */
	{ 0x34, S, "GET DATA BUFFER STATUS" },
	/* 35  O   OOO   MO    SYNCHRONIZE CACHE(10) */
	{ 0x35,	D | W | R | O | B | K, "SYNCHRONIZE CACHE(10)" },
	/* 36  Z   O O    O    LOCK UNLOCK CACHE(10) */
	{ 0x36,	D | W | O | K, "LOCK UNLOCK CACHE(10)" },
	/* 37  O     O         READ DEFECT DATA(10) */
	{ 0x37,	D | O, "READ DEFECT DATA(10)" },
	/* 37         O        INITIALIZE ELEMENT STATUS WITH RANGE */
	{ 0x37,	M, "INITIALIZE ELEMENT STATUS WITH RANGE" },
	/* 38      O O    O    MEDIUM SCAN */
	{ 0x38,	W | O | K, "MEDIUM SCAN" },
	/* 39  ZZZZOZO    Z    COMPARE */
	{ 0x39,	D | T | L | P | W | R | O | K | S, "COMPARE" },
	/* 3A  ZZZZOZO    Z    COPY AND VERIFY */
	{ 0x3A,	D | T | L | P | W | R | O | K | S, "COPY AND VERIFY" },
	/* 3B  OOOOOOOOOOMOOO  WRITE BUFFER */
	{ 0x3B,	ALL, "WRITE BUFFER" },
	/* 3C  OOOOOOOOOO OOO  READ BUFFER */
	{ 0x3C,	ALL & ~(B), "READ BUFFER" },
	/* 3D        O         UPDATE BLOCK */
	{ 0x3D,	O, "UPDATE BLOCK" },
	/* 3E  O   O O         READ LONG(10) */
	{ 0x3E,	D | W | O, "READ LONG(10)" },
	/* 3F  O   O O         WRITE LONG(10) */
	{ 0x3F,	D | W | O, "WRITE LONG(10)" },
	/* 40  ZZZZOZOZ        CHANGE DEFINITION */
	{ 0x40,	D | T | L | P | W | R | O | M | S | C, "CHANGE DEFINITION" },
	/* 41  O               WRITE SAME(10) */
	{ 0x41,	D, "WRITE SAME(10)" },
	/* 42       O          UNMAP */
	{ 0x42,	D, "UNMAP" },
	/* 42       O          READ SUB-CHANNEL */
	{ 0x42,	R, "READ SUB-CHANNEL" },
	/* 43       O          READ TOC/PMA/ATIP */
	{ 0x43,	R, "READ TOC/PMA/ATIP" },
	/* 44   M          M   REPORT DENSITY SUPPORT */
	{ 0x44,	T | V, "REPORT DENSITY SUPPORT" },
	/* 44                  READ HEADER */
	/* 45       O          PLAY AUDIO(10) */
	{ 0x45,	R, "PLAY AUDIO(10)" },
	/* 46       M          GET CONFIGURATION */
	{ 0x46,	R, "GET CONFIGURATION" },
	/* 47       O          PLAY AUDIO MSF */
	{ 0x47,	R, "PLAY AUDIO MSF" },
	/* 48 */
	/* 49 */
	/* 4A       M          GET EVENT STATUS NOTIFICATION */
	{ 0x4A,	R, "GET EVENT STATUS NOTIFICATION" },
	/* 4B       O          PAUSE/RESUME */
	{ 0x4B,	R, "PAUSE/RESUME" },
	/* 4C  OOOOO OOOO OOO  LOG SELECT */
	{ 0x4C,	ALL & ~(R | B), "LOG SELECT" },
	/* 4D  OOOOO OOOO OMO  LOG SENSE */
	{ 0x4D,	ALL & ~(R | B), "LOG SENSE" },
	/* 4E       O          STOP PLAY/SCAN */
	{ 0x4E,	R, "STOP PLAY/SCAN" },
	/* 4F */
	/* 50  O               XDWRITE(10) */
	{ 0x50,	D, "XDWRITE(10)" },
	/* 51  O               XPWRITE(10) */
	{ 0x51,	D, "XPWRITE(10)" },
	/* 51       O          READ DISC INFORMATION */
	{ 0x51,	R, "READ DISC INFORMATION" },
	/* 52  O               XDREAD(10) */
	{ 0x52,	D, "XDREAD(10)" },
	/* 52       O          READ TRACK INFORMATION */
	{ 0x52,	R, "READ TRACK INFORMATION" },
	/* 53       O          RESERVE TRACK */
	{ 0x53,	R, "RESERVE TRACK" },
	/* 54       O          SEND OPC INFORMATION */
	{ 0x54,	R, "SEND OPC INFORMATION" },
	/* 55  OOO OMOOOOMOMO  MODE SELECT(10) */
	{ 0x55,	ALL & ~(P), "MODE SELECT(10)" },
	/* 56  ZZMZO OOOZ      RESERVE(10) */
	{ 0x56,	ALL & ~(R | B | K | V | F | C), "RESERVE(10)" },
	/* 56         Z        RESERVE ELEMENT(10) */
	{ 0x56,	M, "RESERVE ELEMENT(10)" },
	/* 57  ZZMZO OOOZ      RELEASE(10) */
	{ 0x57,	ALL & ~(R | B | K | V | F | C), "RELEASE(10)" },
	/* 57         Z        RELEASE ELEMENT(10) */
	{ 0x57,	M, "RELEASE ELEMENT(10)" },
	/* 58       O          REPAIR TRACK */
	{ 0x58,	R, "REPAIR TRACK" },
	/* 59 */
	/* 5A  OOO OMOOOOMOMO  MODE SENSE(10) */
	{ 0x5A,	ALL & ~(P), "MODE SENSE(10)" },
	/* 5B       O          CLOSE TRACK/SESSION */
	{ 0x5B,	R, "CLOSE TRACK/SESSION" },
	/* 5C       O          READ BUFFER CAPACITY */
	{ 0x5C,	R, "READ BUFFER CAPACITY" },
	/* 5D       O          SEND CUE SHEET */
	{ 0x5D,	R, "SEND CUE SHEET" },
	/* 5E  OOOOO OOOO   M  PERSISTENT RESERVE IN */
	{ 0x5E,	ALL & ~(R | B | K | V | C), "PERSISTENT RESERVE IN" },
	/* 5F  OOOOO OOOO   M  PERSISTENT RESERVE OUT */
	{ 0x5F,	ALL & ~(R | B | K | V | C), "PERSISTENT RESERVE OUT" },
	/* 7E  OO   O OOOO O   extended CDB */
	{ 0x7E,	D | T | R | M | A | E | B | V, "extended CDB" },
	/* 7F  O            M  variable length CDB (more than 16 bytes) */
	{ 0x7F,	D | F, "variable length CDB (more than 16 bytes)" },
	/* 80  Z               XDWRITE EXTENDED(16) */
	{ 0x80,	D, "XDWRITE EXTENDED(16)" },
	/* 80   M              WRITE FILEMARKS(16) */
	{ 0x80,	T, "WRITE FILEMARKS(16)" },
	/* 81  Z               REBUILD(16) */
	{ 0x81,	D, "REBUILD(16)" },
	/* 81   O              READ REVERSE(16) */
	{ 0x81,	T, "READ REVERSE(16)" },
	/* 82  Z               REGENERATE(16) */
	{ 0x82,	D, "REGENERATE(16)" },
	/* 83  OOOOO O    OO   EXTENDED COPY */
	{ 0x83,	D | T | L | P | W | O | K | V, "EXTENDED COPY" },
	/* 84  OOOOO O    OO   RECEIVE COPY RESULTS */
	{ 0x84,	D | T | L | P | W | O | K | V, "RECEIVE COPY RESULTS" },
	/* 85  O    O    O     ATA COMMAND PASS THROUGH(16) */
	{ 0x85,	D | R | B, "ATA COMMAND PASS THROUGH(16)" },
	/* 86  OO OO OOOOOOO   ACCESS CONTROL IN */
	{ 0x86,	ALL & ~(L | R | F), "ACCESS CONTROL IN" },
	/* 87  OO OO OOOOOOO   ACCESS CONTROL OUT */
	{ 0x87,	ALL & ~(L | R | F), "ACCESS CONTROL OUT" },
	/*
	 * XXX READ(16)/WRITE(16) were not listed for CD/DVE in op-num.txt
	 * but we had it since r1.40.  Do we really want them?
	 */
	/* 88  MM  O O   O     READ(16) */
	{ 0x88,	D | T | W | O | B, "READ(16)" },
	/* 89  O               COMPARE AND WRITE*/
	{ 0x89,	D, "COMPARE AND WRITE" },
	/* 8A  OM  O O   O     WRITE(16) */
	{ 0x8A,	D | T | W | O | B, "WRITE(16)" },
	/* 8B  O               ORWRITE */
	{ 0x8B,	D, "ORWRITE" },
	/* 8C  OO  O OO  O M   READ ATTRIBUTE */
	{ 0x8C,	D | T | W | O | M | B | V, "READ ATTRIBUTE" },
	/* 8D  OO  O OO  O O   WRITE ATTRIBUTE */
	{ 0x8D,	D | T | W | O | M | B | V, "WRITE ATTRIBUTE" },
	/* 8E  O   O O   O     WRITE AND VERIFY(16) */
	{ 0x8E,	D | W | O | B, "WRITE AND VERIFY(16)" },
	/* 8F  OO  O O   O     VERIFY(16) */
	{ 0x8F,	D | T | W | O | B, "VERIFY(16)" },
	/* 90  O   O O   O     PRE-FETCH(16) */
	{ 0x90,	D | W | O | B, "PRE-FETCH(16)" },
	/* 91  O   O O   O     SYNCHRONIZE CACHE(16) */
	{ 0x91,	D | W | O | B, "SYNCHRONIZE CACHE(16)" },
	/* 91   O              SPACE(16) */
	{ 0x91,	T, "SPACE(16)" },
	/* 92  Z   O O         LOCK UNLOCK CACHE(16) */
	{ 0x92,	D | W | O, "LOCK UNLOCK CACHE(16)" },
	/* 92   O              LOCATE(16) */
	{ 0x92,	T, "LOCATE(16)" },
	/* 93  O               WRITE SAME(16) */
	{ 0x93,	D, "WRITE SAME(16)" },
	/* 93   M              ERASE(16) */
	{ 0x93,	T, "ERASE(16)" },
	/* 94 [usage proposed by SCSI Socket Services project] */
	/* 95 [usage proposed by SCSI Socket Services project] */
	/* 96 [usage proposed by SCSI Socket Services project] */
	/* 97 [usage proposed by SCSI Socket Services project] */
	/* 98 */
	/* 99 */
	/* 9A */
	/* 9B */
	/* 9C */
	/* 9D */
	/* XXX KDM ALL for this?  op-num.txt defines it for none.. */
	/* 9E                  SERVICE ACTION IN(16) */
	{ 0x9E, ALL, "SERVICE ACTION IN(16)" },
	/* XXX KDM ALL for this?  op-num.txt defines it for ADC.. */
	/* 9F              M   SERVICE ACTION OUT(16) */
	{ 0x9F,	ALL, "SERVICE ACTION OUT(16)" },
	/* A0  MMOOO OMMM OMO  REPORT LUNS */
	{ 0xA0,	ALL & ~(R | B), "REPORT LUNS" },
	/* A1       O          BLANK */
	{ 0xA1,	R, "BLANK" },
	/* A1  O         O     ATA COMMAND PASS THROUGH(12) */
	{ 0xA1,	D | B, "ATA COMMAND PASS THROUGH(12)" },
	/* A2  OO   O      O   SECURITY PROTOCOL IN */
	{ 0xA2,	D | T | R | V, "SECURITY PROTOCOL IN" },
	/* A3  OOO O OOMOOOM   MAINTENANCE (IN) */
	{ 0xA3,	ALL & ~(P | R | F), "MAINTENANCE (IN)" },
	/* A3       O          SEND KEY */
	{ 0xA3,	R, "SEND KEY" },
	/* A4  OOO O OOOOOOO   MAINTENANCE (OUT) */
	{ 0xA4,	ALL & ~(P | R | F), "MAINTENANCE (OUT)" },
	/* A4       O          REPORT KEY */
	{ 0xA4,	R, "REPORT KEY" },
	/* A5   O  O OM        MOVE MEDIUM */
	{ 0xA5,	T | W | O | M, "MOVE MEDIUM" },
	/* A5       O          PLAY AUDIO(12) */
	{ 0xA5,	R, "PLAY AUDIO(12)" },
	/* A6         O        EXCHANGE MEDIUM */
	{ 0xA6,	M, "EXCHANGE MEDIUM" },
	/* A6       O          LOAD/UNLOAD C/DVD */
	{ 0xA6,	R, "LOAD/UNLOAD C/DVD" },
	/* A7  ZZ  O O         MOVE MEDIUM ATTACHED */
	{ 0xA7,	D | T | W | O, "MOVE MEDIUM ATTACHED" },
	/* A7       O          SET READ AHEAD */
	{ 0xA7,	R, "SET READ AHEAD" },
	/* A8  O   OOO         READ(12) */
	{ 0xA8,	D | W | R | O, "READ(12)" },
	/* A8                  GET MESSAGE(12) */
	{ 0xA8, C, "GET MESSAGE(12)" },
	/* A9              O   SERVICE ACTION OUT(12) */
	{ 0xA9,	V, "SERVICE ACTION OUT(12)" },
	/* AA  O   OOO         WRITE(12) */
	{ 0xAA,	D | W | R | O, "WRITE(12)" },
	/* AA                  SEND MESSAGE(12) */
	{ 0xAA, C, "SEND MESSAGE(12)" },
	/* AB       O      O   SERVICE ACTION IN(12) */
	{ 0xAB,	R | V, "SERVICE ACTION IN(12)" },
	/* AC        O         ERASE(12) */
	{ 0xAC,	O, "ERASE(12)" },
	/* AC       O          GET PERFORMANCE */
	{ 0xAC,	R, "GET PERFORMANCE" },
	/* AD       O          READ DVD STRUCTURE */
	{ 0xAD,	R, "READ DVD STRUCTURE" },
	/* AE  O   O O         WRITE AND VERIFY(12) */
	{ 0xAE,	D | W | O, "WRITE AND VERIFY(12)" },
	/* AF  O   OZO         VERIFY(12) */
	{ 0xAF,	D | W | R | O, "VERIFY(12)" },
	/* B0      ZZZ         SEARCH DATA HIGH(12) */
	{ 0xB0,	W | R | O, "SEARCH DATA HIGH(12)" },
	/* B1      ZZZ         SEARCH DATA EQUAL(12) */
	{ 0xB1,	W | R | O, "SEARCH DATA EQUAL(12)" },
	/* B2      ZZZ         SEARCH DATA LOW(12) */
	{ 0xB2,	W | R | O, "SEARCH DATA LOW(12)" },
	/* B3  Z   OZO         SET LIMITS(12) */
	{ 0xB3,	D | W | R | O, "SET LIMITS(12)" },
	/* B4  ZZ  OZO         READ ELEMENT STATUS ATTACHED */
	{ 0xB4,	D | T | W | R | O, "READ ELEMENT STATUS ATTACHED" },
	/* B5  OO   O      O   SECURITY PROTOCOL OUT */
	{ 0xB5,	D | T | R | V, "SECURITY PROTOCOL OUT" },
	/* B5         O        REQUEST VOLUME ELEMENT ADDRESS */
	{ 0xB5,	M, "REQUEST VOLUME ELEMENT ADDRESS" },
	/* B6         O        SEND VOLUME TAG */
	{ 0xB6,	M, "SEND VOLUME TAG" },
	/* B6       O          SET STREAMING */
	{ 0xB6,	R, "SET STREAMING" },
	/* B7  O     O         READ DEFECT DATA(12) */
	{ 0xB7,	D | O, "READ DEFECT DATA(12)" },
	/* B8   O  OZOM        READ ELEMENT STATUS */
	{ 0xB8,	T | W | R | O | M, "READ ELEMENT STATUS" },
	/* B9       O          READ CD MSF */
	{ 0xB9,	R, "READ CD MSF" },
	/* BA  O   O OOMO      REDUNDANCY GROUP (IN) */
	{ 0xBA,	D | W | O | M | A | E, "REDUNDANCY GROUP (IN)" },
	/* BA       O          SCAN */
	{ 0xBA,	R, "SCAN" },
	/* BB  O   O OOOO      REDUNDANCY GROUP (OUT) */
	{ 0xBB,	D | W | O | M | A | E, "REDUNDANCY GROUP (OUT)" },
	/* BB       O          SET CD SPEED */
	{ 0xBB,	R, "SET CD SPEED" },
	/* BC  O   O OOMO      SPARE (IN) */
	{ 0xBC,	D | W | O | M | A | E, "SPARE (IN)" },
	/* BD  O   O OOOO      SPARE (OUT) */
	{ 0xBD,	D | W | O | M | A | E, "SPARE (OUT)" },
	/* BD       O          MECHANISM STATUS */
	{ 0xBD,	R, "MECHANISM STATUS" },
	/* BE  O   O OOMO      VOLUME SET (IN) */
	{ 0xBE,	D | W | O | M | A | E, "VOLUME SET (IN)" },
	/* BE       O          READ CD */
	{ 0xBE,	R, "READ CD" },
	/* BF  O   O OOOO      VOLUME SET (OUT) */
	{ 0xBF,	D | W | O | M | A | E, "VOLUME SET (OUT)" },
	/* BF       O          SEND DVD STRUCTURE */
	{ 0xBF,	R, "SEND DVD STRUCTURE" }
};

const char *
scsi_op_desc(u_int16_t opcode, struct scsi_inquiry_data *inq_data)
{
	caddr_t match;
	int i, j;
	u_int32_t opmask;
	u_int16_t pd_type;
	int       num_ops[2];
	struct op_table_entry *table[2];
	int num_tables;

	/*
	 * If we've got inquiry data, use it to determine what type of
	 * device we're dealing with here.  Otherwise, assume direct
	 * access.
	 */
	if (inq_data == NULL) {
		pd_type = T_DIRECT;
		match = NULL;
	} else {
		pd_type = SID_TYPE(inq_data);

		match = cam_quirkmatch((caddr_t)inq_data,
				       (caddr_t)scsi_op_quirk_table,
				       sizeof(scsi_op_quirk_table)/
				       sizeof(*scsi_op_quirk_table),
				       sizeof(*scsi_op_quirk_table),
				       scsi_inquiry_match);
	}

	if (match != NULL) {
		table[0] = ((struct scsi_op_quirk_entry *)match)->op_table;
		num_ops[0] = ((struct scsi_op_quirk_entry *)match)->num_ops;
		table[1] = scsi_op_codes;
		num_ops[1] = sizeof(scsi_op_codes)/sizeof(scsi_op_codes[0]);
		num_tables = 2;
	} else {
		/*	
		 * If this is true, we have a vendor specific opcode that
		 * wasn't covered in the quirk table.
		 */
		if ((opcode > 0xBF) || ((opcode > 0x5F) && (opcode < 0x80)))
			return("Vendor Specific Command");

		table[0] = scsi_op_codes;
		num_ops[0] = sizeof(scsi_op_codes)/sizeof(scsi_op_codes[0]);
		num_tables = 1;
	}

	/* RBC is 'Simplified' Direct Access Device */
	if (pd_type == T_RBC)
		pd_type = T_DIRECT;

	/* Map NODEVICE to Direct Access Device to handle REPORT LUNS, etc. */
	if (pd_type == T_NODEVICE)
		pd_type = T_DIRECT;

	opmask = 1 << pd_type;

	for (j = 0; j < num_tables; j++) {
		for (i = 0;i < num_ops[j] && table[j][i].opcode <= opcode; i++){
			if ((table[j][i].opcode == opcode) 
			 && ((table[j][i].opmask & opmask) != 0))
				return(table[j][i].desc);
		}
	}
	
	/*
	 * If we can't find a match for the command in the table, we just
	 * assume it's a vendor specifc command.
	 */
	return("Vendor Specific Command");

}

#else /* SCSI_NO_OP_STRINGS */

const char *
scsi_op_desc(u_int16_t opcode, struct scsi_inquiry_data *inq_data)
{
	return("");
}

#endif


#if !defined(SCSI_NO_SENSE_STRINGS)
#define SST(asc, ascq, action, desc) \
	asc, ascq, action, desc
#else 
const char empty_string[] = "";

#define SST(asc, ascq, action, desc) \
	asc, ascq, action, empty_string
#endif 

const struct sense_key_table_entry sense_key_table[] = 
{
	{ SSD_KEY_NO_SENSE, SS_NOP, "NO SENSE" },
	{ SSD_KEY_RECOVERED_ERROR, SS_NOP|SSQ_PRINT_SENSE, "RECOVERED ERROR" },
	{ SSD_KEY_NOT_READY, SS_RDEF, "NOT READY" },
	{ SSD_KEY_MEDIUM_ERROR, SS_RDEF, "MEDIUM ERROR" },
	{ SSD_KEY_HARDWARE_ERROR, SS_RDEF, "HARDWARE FAILURE" },
	{ SSD_KEY_ILLEGAL_REQUEST, SS_FATAL|EINVAL, "ILLEGAL REQUEST" },
	{ SSD_KEY_UNIT_ATTENTION, SS_FATAL|ENXIO, "UNIT ATTENTION" },
	{ SSD_KEY_DATA_PROTECT, SS_FATAL|EACCES, "DATA PROTECT" },
	{ SSD_KEY_BLANK_CHECK, SS_FATAL|ENOSPC, "BLANK CHECK" },
	{ SSD_KEY_Vendor_Specific, SS_FATAL|EIO, "Vendor Specific" },
	{ SSD_KEY_COPY_ABORTED, SS_FATAL|EIO, "COPY ABORTED" },
	{ SSD_KEY_ABORTED_COMMAND, SS_RDEF, "ABORTED COMMAND" },
	{ SSD_KEY_EQUAL, SS_NOP, "EQUAL" },
	{ SSD_KEY_VOLUME_OVERFLOW, SS_FATAL|EIO, "VOLUME OVERFLOW" },
	{ SSD_KEY_MISCOMPARE, SS_NOP, "MISCOMPARE" },
	{ SSD_KEY_COMPLETED, SS_NOP, "COMPLETED" }
};

const int sense_key_table_size =
    sizeof(sense_key_table)/sizeof(sense_key_table[0]);

static struct asc_table_entry quantum_fireball_entries[] = {
	{ SST(0x04, 0x0b, SS_START | SSQ_DECREMENT_COUNT | ENXIO, 
	     "Logical unit not ready, initializing cmd. required") }
};

static struct asc_table_entry sony_mo_entries[] = {
	{ SST(0x04, 0x00, SS_START | SSQ_DECREMENT_COUNT | ENXIO,
	     "Logical unit not ready, cause not reportable") }
};

static struct asc_table_entry hgst_entries[] = {
	{ SST(0x04, 0xF0, SS_RDEF,
	    "Vendor Unique - Logical Unit Not Ready") },
	{ SST(0x0A, 0x01, SS_RDEF,
	    "Unrecovered Super Certification Log Write Error") },
	{ SST(0x0A, 0x02, SS_RDEF,
	    "Unrecovered Super Certification Log Read Error") },
	{ SST(0x15, 0x03, SS_RDEF,
	    "Unrecovered Sector Error") },
	{ SST(0x3E, 0x04, SS_RDEF,
	    "Unrecovered Self-Test Hard-Cache Test Fail") },
	{ SST(0x3E, 0x05, SS_RDEF,
	    "Unrecovered Self-Test OTF-Cache Fail") },
	{ SST(0x40, 0x00, SS_RDEF,
	    "Unrecovered SAT No Buffer Overflow Error") },
	{ SST(0x40, 0x01, SS_RDEF,
	    "Unrecovered SAT Buffer Overflow Error") },
	{ SST(0x40, 0x02, SS_RDEF,
	    "Unrecovered SAT No Buffer Overflow With ECS Fault") },
	{ SST(0x40, 0x03, SS_RDEF,
	    "Unrecovered SAT Buffer Overflow With ECS Fault") },
	{ SST(0x40, 0x81, SS_RDEF,
	    "DRAM Failure") },
	{ SST(0x44, 0x0B, SS_RDEF,
	    "Vendor Unique - Internal Target Failure") },
	{ SST(0x44, 0xF2, SS_RDEF,
	    "Vendor Unique - Internal Target Failure") },
	{ SST(0x44, 0xF6, SS_RDEF,
	    "Vendor Unique - Internal Target Failure") },
	{ SST(0x44, 0xF9, SS_RDEF,
	    "Vendor Unique - Internal Target Failure") },
	{ SST(0x44, 0xFA, SS_RDEF,
	    "Vendor Unique - Internal Target Failure") },
	{ SST(0x5D, 0x22, SS_RDEF,
	    "Extreme Over-Temperature Warning") },
	{ SST(0x5D, 0x50, SS_RDEF,
	    "Load/Unload cycle Count Warning") },
	{ SST(0x81, 0x00, SS_RDEF,
	    "Vendor Unique - Internal Logic Error") },
	{ SST(0x85, 0x00, SS_RDEF,
	    "Vendor Unique - Internal Key Seed Error") },
};

static struct asc_table_entry seagate_entries[] = {
	{ SST(0x04, 0xF0, SS_RDEF,
	    "Logical Unit Not Ready, super certify in Progress") },
	{ SST(0x08, 0x86, SS_RDEF,
	    "Write Fault Data Corruption") },
	{ SST(0x09, 0x0D, SS_RDEF,
	    "Tracking Failure") },
	{ SST(0x09, 0x0E, SS_RDEF,
	    "ETF Failure") },
	{ SST(0x0B, 0x5D, SS_RDEF,
	    "Pre-SMART Warning") },
	{ SST(0x0B, 0x85, SS_RDEF,
	    "5V Voltage Warning") },
	{ SST(0x0B, 0x8C, SS_RDEF,
	    "12V Voltage Warning") },
	{ SST(0x0C, 0xFF, SS_RDEF,
	    "Write Error - Too many error recovery revs") },
	{ SST(0x11, 0xFF, SS_RDEF,
	    "Unrecovered Read Error - Too many error recovery revs") },
	{ SST(0x19, 0x0E, SS_RDEF,
	    "Fewer than 1/2 defect list copies") },
	{ SST(0x20, 0xF3, SS_RDEF,
	    "Illegal CDB linked to skip mask cmd") },
	{ SST(0x24, 0xF0, SS_RDEF,
	    "Illegal byte in CDB, LBA not matching") },
	{ SST(0x24, 0xF1, SS_RDEF,
	    "Illegal byte in CDB, LEN not matching") },
	{ SST(0x24, 0xF2, SS_RDEF,
	    "Mask not matching transfer length") },
	{ SST(0x24, 0xF3, SS_RDEF,
	    "Drive formatted without plist") },
	{ SST(0x26, 0x95, SS_RDEF,
	    "Invalid Field Parameter - CAP File") },
	{ SST(0x26, 0x96, SS_RDEF,
	    "Invalid Field Parameter - RAP File") },
	{ SST(0x26, 0x97, SS_RDEF,
	    "Invalid Field Parameter - TMS Firmware Tag") },
	{ SST(0x26, 0x98, SS_RDEF,
	    "Invalid Field Parameter - Check Sum") },
	{ SST(0x26, 0x99, SS_RDEF,
	    "Invalid Field Parameter - Firmware Tag") },
	{ SST(0x29, 0x08, SS_RDEF,
	    "Write Log Dump data") },
	{ SST(0x29, 0x09, SS_RDEF,
	    "Write Log Dump data") },
	{ SST(0x29, 0x0A, SS_RDEF,
	    "Reserved disk space") },
	{ SST(0x29, 0x0B, SS_RDEF,
	    "SDBP") },
	{ SST(0x29, 0x0C, SS_RDEF,
	    "SDBP") },
	{ SST(0x31, 0x91, SS_RDEF,
	    "Format Corrupted World Wide Name (WWN) is Invalid") },
	{ SST(0x32, 0x03, SS_RDEF,
	    "Defect List - Length exceeds Command Allocated Length") },
	{ SST(0x33, 0x00, SS_RDEF,
	    "Flash not ready for access") },
	{ SST(0x3F, 0x70, SS_RDEF,
	    "Invalid RAP block") },
	{ SST(0x3F, 0x71, SS_RDEF,
	    "RAP/ETF mismatch") },
	{ SST(0x3F, 0x90, SS_RDEF,
	    "Invalid CAP block") },
	{ SST(0x3F, 0x91, SS_RDEF,
	    "World Wide Name (WWN) Mismatch") },
	{ SST(0x40, 0x01, SS_RDEF,
	    "DRAM Parity Error") },
	{ SST(0x40, 0x02, SS_RDEF,
	    "DRAM Parity Error") },
	{ SST(0x42, 0x0A, SS_RDEF,
	    "Loopback Test") },
	{ SST(0x42, 0x0B, SS_RDEF,
	    "Loopback Test") },
	{ SST(0x44, 0xF2, SS_RDEF,
	    "Compare error during data integrity check") },
	{ SST(0x44, 0xF6, SS_RDEF,
	    "Unrecoverable error during data integrity check") },
	{ SST(0x47, 0x80, SS_RDEF,
	    "Fibre Channel Sequence Error") },
	{ SST(0x4E, 0x01, SS_RDEF,
	    "Information Unit Too Short") },
	{ SST(0x80, 0x00, SS_RDEF,
	    "General Firmware Error / Command Timeout") },
	{ SST(0x80, 0x01, SS_RDEF,
	    "Command Timeout") },
	{ SST(0x80, 0x02, SS_RDEF,
	    "Command Timeout") },
	{ SST(0x80, 0x80, SS_RDEF,
	    "FC FIFO Error During Read Transfer") },
	{ SST(0x80, 0x81, SS_RDEF,
	    "FC FIFO Error During Write Transfer") },
	{ SST(0x80, 0x82, SS_RDEF,
	    "DISC FIFO Error During Read Transfer") },
	{ SST(0x80, 0x83, SS_RDEF,
	    "DISC FIFO Error During Write Transfer") },
	{ SST(0x80, 0x84, SS_RDEF,
	    "LBA Seeded LRC Error on Read") },
	{ SST(0x80, 0x85, SS_RDEF,
	    "LBA Seeded LRC Error on Write") },
	{ SST(0x80, 0x86, SS_RDEF,
	    "IOEDC Error on Read") },
	{ SST(0x80, 0x87, SS_RDEF,
	    "IOEDC Error on Write") },
	{ SST(0x80, 0x88, SS_RDEF,
	    "Host Parity Check Failed") },
	{ SST(0x80, 0x89, SS_RDEF,
	    "IOEDC error on read detected by formatter") },
	{ SST(0x80, 0x8A, SS_RDEF,
	    "Host Parity Errors / Host FIFO Initialization Failed") },
	{ SST(0x80, 0x8B, SS_RDEF,
	    "Host Parity Errors") },
	{ SST(0x80, 0x8C, SS_RDEF,
	    "Host Parity Errors") },
	{ SST(0x80, 0x8D, SS_RDEF,
	    "Host Parity Errors") },
	{ SST(0x81, 0x00, SS_RDEF,
	    "LA Check Failed") },
	{ SST(0x82, 0x00, SS_RDEF,
	    "Internal client detected insufficient buffer") },
	{ SST(0x84, 0x00, SS_RDEF,
	    "Scheduled Diagnostic And Repair") },
};

static struct scsi_sense_quirk_entry sense_quirk_table[] = {
	{
		/*
		 * XXX The Quantum Fireball ST and SE like to return 0x04 0x0b
		 * when they really should return 0x04 0x02.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "QUANTUM", "FIREBALL S*", "*"},
		/*num_sense_keys*/0,
		sizeof(quantum_fireball_entries)/sizeof(struct asc_table_entry),
		/*sense key entries*/NULL,
		quantum_fireball_entries
	},
	{
		/*
		 * This Sony MO drive likes to return 0x04, 0x00 when it
		 * isn't spun up.
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "SONY", "SMO-*", "*"},
		/*num_sense_keys*/0,
		sizeof(sony_mo_entries)/sizeof(struct asc_table_entry),
		/*sense key entries*/NULL,
		sony_mo_entries
	},
	{
		/*
		 * HGST vendor-specific error codes
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "HGST", "*", "*"},
		/*num_sense_keys*/0,
		sizeof(hgst_entries)/sizeof(struct asc_table_entry),
		/*sense key entries*/NULL,
		hgst_entries
	},
	{
		/*
		 * SEAGATE vendor-specific error codes
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "SEAGATE", "*", "*"},
		/*num_sense_keys*/0,
		sizeof(seagate_entries)/sizeof(struct asc_table_entry),
		/*sense key entries*/NULL,
		seagate_entries
	}
};

const int sense_quirk_table_size =
    sizeof(sense_quirk_table)/sizeof(sense_quirk_table[0]);

static struct asc_table_entry asc_table[] = {
	/*
	 * From: http://www.t10.org/lists/asc-num.txt
	 * Modifications by Jung-uk Kim (jkim@FreeBSD.org)
	 */
	/*
	 * File: ASC-NUM.TXT
	 *
	 * SCSI ASC/ASCQ Assignments
	 * Numeric Sorted Listing
	 * as of  5/20/12
	 *
	 * D - DIRECT ACCESS DEVICE (SBC-2)                   device column key
	 * .T - SEQUENTIAL ACCESS DEVICE (SSC)               -------------------
	 * . L - PRINTER DEVICE (SSC)                           blank = reserved
	 * .  P - PROCESSOR DEVICE (SPC)                     not blank = allowed
	 * .  .W - WRITE ONCE READ MULTIPLE DEVICE (SBC-2)
	 * .  . R - CD DEVICE (MMC)
	 * .  .  O - OPTICAL MEMORY DEVICE (SBC-2)
	 * .  .  .M - MEDIA CHANGER DEVICE (SMC)
	 * .  .  . A - STORAGE ARRAY DEVICE (SCC)
	 * .  .  .  E - ENCLOSURE SERVICES DEVICE (SES)
	 * .  .  .  .B - SIMPLIFIED DIRECT-ACCESS DEVICE (RBC)
	 * .  .  .  . K - OPTICAL CARD READER/WRITER DEVICE (OCRW)
	 * .  .  .  .  V - AUTOMATION/DRIVE INTERFACE (ADC)
	 * .  .  .  .  .F - OBJECT-BASED STORAGE (OSD)
	 * DTLPWROMAEBKVF
	 * ASC      ASCQ  Action
	 * Description
	 */
	/* DTLPWROMAEBKVF */
	{ SST(0x00, 0x00, SS_NOP,
	    "No additional sense information") },
	/*  T             */
	{ SST(0x00, 0x01, SS_RDEF,
	    "Filemark detected") },
	/*  T             */
	{ SST(0x00, 0x02, SS_RDEF,
	    "End-of-partition/medium detected") },
	/*  T             */
	{ SST(0x00, 0x03, SS_RDEF,
	    "Setmark detected") },
	/*  T             */
	{ SST(0x00, 0x04, SS_RDEF,
	    "Beginning-of-partition/medium detected") },
	/*  TL            */
	{ SST(0x00, 0x05, SS_RDEF,
	    "End-of-data detected") },
	/* DTLPWROMAEBKVF */
	{ SST(0x00, 0x06, SS_RDEF,
	    "I/O process terminated") },
	/*  T             */
	{ SST(0x00, 0x07, SS_RDEF,	/* XXX TBD */
	    "Programmable early warning detected") },
	/*      R         */
	{ SST(0x00, 0x11, SS_FATAL | EBUSY,
	    "Audio play operation in progress") },
	/*      R         */
	{ SST(0x00, 0x12, SS_NOP,
	    "Audio play operation paused") },
	/*      R         */
	{ SST(0x00, 0x13, SS_NOP,
	    "Audio play operation successfully completed") },
	/*      R         */
	{ SST(0x00, 0x14, SS_RDEF,
	    "Audio play operation stopped due to error") },
	/*      R         */
	{ SST(0x00, 0x15, SS_NOP,
	    "No current audio status to return") },
	/* DTLPWROMAEBKVF */
	{ SST(0x00, 0x16, SS_FATAL | EBUSY,
	    "Operation in progress") },
	/* DTL WROMAEBKVF */
	{ SST(0x00, 0x17, SS_RDEF,
	    "Cleaning requested") },
	/*  T             */
	{ SST(0x00, 0x18, SS_RDEF,	/* XXX TBD */
	    "Erase operation in progress") },
	/*  T             */
	{ SST(0x00, 0x19, SS_RDEF,	/* XXX TBD */
	    "Locate operation in progress") },
	/*  T             */
	{ SST(0x00, 0x1A, SS_RDEF,	/* XXX TBD */
	    "Rewind operation in progress") },
	/*  T             */
	{ SST(0x00, 0x1B, SS_RDEF,	/* XXX TBD */
	    "Set capacity operation in progress") },
	/*  T             */
	{ SST(0x00, 0x1C, SS_RDEF,	/* XXX TBD */
	    "Verify operation in progress") },
	/* DT        B    */
	{ SST(0x00, 0x1D, SS_RDEF,	/* XXX TBD */
	    "ATA pass through information available") },
	/* DT   R MAEBKV  */
	{ SST(0x00, 0x1E, SS_RDEF,	/* XXX TBD */
	    "Conflicting SA creation request") },
	/* DT        B    */
	{ SST(0x00, 0x1F, SS_RDEF,	/* XXX TBD */
	    "Logical unit transitioning to another power condition") },
	/* DT P      B    */
	{ SST(0x00, 0x20, SS_RDEF,	/* XXX TBD */
	    "Extended copy information available") },
	/* D   W O   BK   */
	{ SST(0x01, 0x00, SS_RDEF,
	    "No index/sector signal") },
	/* D   WRO   BK   */
	{ SST(0x02, 0x00, SS_RDEF,
	    "No seek complete") },
	/* DTL W O   BK   */
	{ SST(0x03, 0x00, SS_RDEF,
	    "Peripheral device write fault") },
	/*  T             */
	{ SST(0x03, 0x01, SS_RDEF,
	    "No write current") },
	/*  T             */
	{ SST(0x03, 0x02, SS_RDEF,
	    "Excessive write errors") },
	/* DTLPWROMAEBKVF */
	{ SST(0x04, 0x00, SS_RDEF,
	    "Logical unit not ready, cause not reportable") },
	/* DTLPWROMAEBKVF */
	{ SST(0x04, 0x01, SS_TUR | SSQ_MANY | SSQ_DECREMENT_COUNT | EBUSY,
	    "Logical unit is in process of becoming ready") },
	/* DTLPWROMAEBKVF */
	{ SST(0x04, 0x02, SS_START | SSQ_DECREMENT_COUNT | ENXIO,
	    "Logical unit not ready, initializing command required") },
	/* DTLPWROMAEBKVF */
	{ SST(0x04, 0x03, SS_FATAL | ENXIO,
	    "Logical unit not ready, manual intervention required") },
	/* DTL  RO   B    */
	{ SST(0x04, 0x04, SS_FATAL | EBUSY,
	    "Logical unit not ready, format in progress") },
	/* DT  W O A BK F */
	{ SST(0x04, 0x05, SS_FATAL | EBUSY,
	    "Logical unit not ready, rebuild in progress") },
	/* DT  W O A BK   */
	{ SST(0x04, 0x06, SS_FATAL | EBUSY,
	    "Logical unit not ready, recalculation in progress") },
	/* DTLPWROMAEBKVF */
	{ SST(0x04, 0x07, SS_FATAL | EBUSY,
	    "Logical unit not ready, operation in progress") },
	/*      R         */
	{ SST(0x04, 0x08, SS_FATAL | EBUSY,
	    "Logical unit not ready, long write in progress") },
	/* DTLPWROMAEBKVF */
	{ SST(0x04, 0x09, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, self-test in progress") },
	/* DTLPWROMAEBKVF */
	{ SST(0x04, 0x0A, SS_RDEF,	/* XXX TBD */
	    "Logical unit not accessible, asymmetric access state transition")},
	/* DTLPWROMAEBKVF */
	{ SST(0x04, 0x0B, SS_RDEF,	/* XXX TBD */
	    "Logical unit not accessible, target port in standby state") },
	/* DTLPWROMAEBKVF */
	{ SST(0x04, 0x0C, SS_RDEF,	/* XXX TBD */
	    "Logical unit not accessible, target port in unavailable state") },
	/*              F */
	{ SST(0x04, 0x0D, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, structure check required") },
	/* DT  WROM  B    */
	{ SST(0x04, 0x10, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, auxiliary memory not accessible") },
	/* DT  WRO AEB VF */
	{ SST(0x04, 0x11, SS_TUR | SSQ_MANY | SSQ_DECREMENT_COUNT | EBUSY,
	    "Logical unit not ready, notify (enable spinup) required") },
	/*        M    V  */
	{ SST(0x04, 0x12, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, offline") },
	/* DT   R MAEBKV  */
	{ SST(0x04, 0x13, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, SA creation in progress") },
	/* D         B    */
	{ SST(0x04, 0x14, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, space allocation in progress") },
	/*        M       */
	{ SST(0x04, 0x15, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, robotics disabled") },
	/*        M       */
	{ SST(0x04, 0x16, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, configuration required") },
	/*        M       */
	{ SST(0x04, 0x17, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, calibration required") },
	/*        M       */
	{ SST(0x04, 0x18, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, a door is open") },
	/*        M       */
	{ SST(0x04, 0x19, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, operating in sequential mode") },
	/* DT        B    */
	{ SST(0x04, 0x1A, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, START/STOP UNIT command in progress") },
	/* D         B    */
	{ SST(0x04, 0x1B, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, sanitize in progress") },
	/* DT     MAEB    */
	{ SST(0x04, 0x1C, SS_RDEF,	/* XXX TBD */
	    "Logical unit not ready, additional power use not yet granted") },
	/* DTL WROMAEBKVF */
	{ SST(0x05, 0x00, SS_RDEF,
	    "Logical unit does not respond to selection") },
	/* D   WROM  BK   */
	{ SST(0x06, 0x00, SS_RDEF,
	    "No reference position found") },
	/* DTL WROM  BK   */
	{ SST(0x07, 0x00, SS_RDEF,
	    "Multiple peripheral devices selected") },
	/* DTL WROMAEBKVF */
	{ SST(0x08, 0x00, SS_RDEF,
	    "Logical unit communication failure") },
	/* DTL WROMAEBKVF */
	{ SST(0x08, 0x01, SS_RDEF,
	    "Logical unit communication time-out") },
	/* DTL WROMAEBKVF */
	{ SST(0x08, 0x02, SS_RDEF,
	    "Logical unit communication parity error") },
	/* DT   ROM  BK   */
	{ SST(0x08, 0x03, SS_RDEF,
	    "Logical unit communication CRC error (Ultra-DMA/32)") },
	/* DTLPWRO    K   */
	{ SST(0x08, 0x04, SS_RDEF,	/* XXX TBD */
	    "Unreachable copy target") },
	/* DT  WRO   B    */
	{ SST(0x09, 0x00, SS_RDEF,
	    "Track following error") },
	/*     WRO    K   */
	{ SST(0x09, 0x01, SS_RDEF,
	    "Tracking servo failure") },
	/*     WRO    K   */
	{ SST(0x09, 0x02, SS_RDEF,
	    "Focus servo failure") },
	/*     WRO        */
	{ SST(0x09, 0x03, SS_RDEF,
	    "Spindle servo failure") },
	/* DT  WRO   B    */
	{ SST(0x09, 0x04, SS_RDEF,
	    "Head select fault") },
	/* DTLPWROMAEBKVF */
	{ SST(0x0A, 0x00, SS_FATAL | ENOSPC,
	    "Error log overflow") },
	/* DTLPWROMAEBKVF */
	{ SST(0x0B, 0x00, SS_RDEF,
	    "Warning") },
	/* DTLPWROMAEBKVF */
	{ SST(0x0B, 0x01, SS_RDEF,
	    "Warning - specified temperature exceeded") },
	/* DTLPWROMAEBKVF */
	{ SST(0x0B, 0x02, SS_RDEF,
	    "Warning - enclosure degraded") },
	/* DTLPWROMAEBKVF */
	{ SST(0x0B, 0x03, SS_RDEF,	/* XXX TBD */
	    "Warning - background self-test failed") },
	/* DTLPWRO AEBKVF */
	{ SST(0x0B, 0x04, SS_RDEF,	/* XXX TBD */
	    "Warning - background pre-scan detected medium error") },
	/* DTLPWRO AEBKVF */
	{ SST(0x0B, 0x05, SS_RDEF,	/* XXX TBD */
	    "Warning - background medium scan detected medium error") },
	/* DTLPWROMAEBKVF */
	{ SST(0x0B, 0x06, SS_RDEF,	/* XXX TBD */
	    "Warning - non-volatile cache now volatile") },
	/* DTLPWROMAEBKVF */
	{ SST(0x0B, 0x07, SS_RDEF,	/* XXX TBD */
	    "Warning - degraded power to non-volatile cache") },
	/* DTLPWROMAEBKVF */
	{ SST(0x0B, 0x08, SS_RDEF,	/* XXX TBD */
	    "Warning - power loss expected") },
	/* D              */
	{ SST(0x0B, 0x09, SS_RDEF,	/* XXX TBD */
	    "Warning - device statistics notification available") },
	/*  T   R         */
	{ SST(0x0C, 0x00, SS_RDEF,
	    "Write error") },
	/*            K   */
	{ SST(0x0C, 0x01, SS_NOP | SSQ_PRINT_SENSE,
	    "Write error - recovered with auto reallocation") },
	/* D   W O   BK   */
	{ SST(0x0C, 0x02, SS_RDEF,
	    "Write error - auto reallocation failed") },
	/* D   W O   BK   */
	{ SST(0x0C, 0x03, SS_RDEF,
	    "Write error - recommend reassignment") },
	/* DT  W O   B    */
	{ SST(0x0C, 0x04, SS_RDEF,
	    "Compression check miscompare error") },
	/* DT  W O   B    */
	{ SST(0x0C, 0x05, SS_RDEF,
	    "Data expansion occurred during compression") },
	/* DT  W O   B    */
	{ SST(0x0C, 0x06, SS_RDEF,
	    "Block not compressible") },
	/*      R         */
	{ SST(0x0C, 0x07, SS_RDEF,
	    "Write error - recovery needed") },
	/*      R         */
	{ SST(0x0C, 0x08, SS_RDEF,
	    "Write error - recovery failed") },
	/*      R         */
	{ SST(0x0C, 0x09, SS_RDEF,
	    "Write error - loss of streaming") },
	/*      R         */
	{ SST(0x0C, 0x0A, SS_RDEF,
	    "Write error - padding blocks added") },
	/* DT  WROM  B    */
	{ SST(0x0C, 0x0B, SS_RDEF,	/* XXX TBD */
	    "Auxiliary memory write error") },
	/* DTLPWRO AEBKVF */
	{ SST(0x0C, 0x0C, SS_RDEF,	/* XXX TBD */
	    "Write error - unexpected unsolicited data") },
	/* DTLPWRO AEBKVF */
	{ SST(0x0C, 0x0D, SS_RDEF,	/* XXX TBD */
	    "Write error - not enough unsolicited data") },
	/* DT  W O   BK   */
	{ SST(0x0C, 0x0E, SS_RDEF,	/* XXX TBD */
	    "Multiple write errors") },
	/*      R         */
	{ SST(0x0C, 0x0F, SS_RDEF,	/* XXX TBD */
	    "Defects in error window") },
	/* DTLPWRO A  K   */
	{ SST(0x0D, 0x00, SS_RDEF,	/* XXX TBD */
	    "Error detected by third party temporary initiator") },
	/* DTLPWRO A  K   */
	{ SST(0x0D, 0x01, SS_RDEF,	/* XXX TBD */
	    "Third party device failure") },
	/* DTLPWRO A  K   */
	{ SST(0x0D, 0x02, SS_RDEF,	/* XXX TBD */
	    "Copy target device not reachable") },
	/* DTLPWRO A  K   */
	{ SST(0x0D, 0x03, SS_RDEF,	/* XXX TBD */
	    "Incorrect copy target device type") },
	/* DTLPWRO A  K   */
	{ SST(0x0D, 0x04, SS_RDEF,	/* XXX TBD */
	    "Copy target device data underrun") },
	/* DTLPWRO A  K   */
	{ SST(0x0D, 0x05, SS_RDEF,	/* XXX TBD */
	    "Copy target device data overrun") },
	/* DT PWROMAEBK F */
	{ SST(0x0E, 0x00, SS_RDEF,	/* XXX TBD */
	    "Invalid information unit") },
	/* DT PWROMAEBK F */
	{ SST(0x0E, 0x01, SS_RDEF,	/* XXX TBD */
	    "Information unit too short") },
	/* DT PWROMAEBK F */
	{ SST(0x0E, 0x02, SS_RDEF,	/* XXX TBD */
	    "Information unit too long") },
	/* DT P R MAEBK F */
	{ SST(0x0E, 0x03, SS_RDEF,	/* XXX TBD */
	    "Invalid field in command information unit") },
	/* D   W O   BK   */
	{ SST(0x10, 0x00, SS_RDEF,
	    "ID CRC or ECC error") },
	/* DT  W O        */
	{ SST(0x10, 0x01, SS_RDEF,	/* XXX TBD */
	    "Logical block guard check failed") },
	/* DT  W O        */
	{ SST(0x10, 0x02, SS_RDEF,	/* XXX TBD */
	    "Logical block application tag check failed") },
	/* DT  W O        */
	{ SST(0x10, 0x03, SS_RDEF,	/* XXX TBD */
	    "Logical block reference tag check failed") },
	/*  T             */
	{ SST(0x10, 0x04, SS_RDEF,	/* XXX TBD */
	    "Logical block protection error on recovered buffer data") },
	/*  T             */
	{ SST(0x10, 0x05, SS_RDEF,	/* XXX TBD */
	    "Logical block protection method error") },
	/* DT  WRO   BK   */
	{ SST(0x11, 0x00, SS_FATAL|EIO,
	    "Unrecovered read error") },
	/* DT  WRO   BK   */
	{ SST(0x11, 0x01, SS_FATAL|EIO,
	    "Read retries exhausted") },
	/* DT  WRO   BK   */
	{ SST(0x11, 0x02, SS_FATAL|EIO,
	    "Error too long to correct") },
	/* DT  W O   BK   */
	{ SST(0x11, 0x03, SS_FATAL|EIO,
	    "Multiple read errors") },
	/* D   W O   BK   */
	{ SST(0x11, 0x04, SS_FATAL|EIO,
	    "Unrecovered read error - auto reallocate failed") },
	/*     WRO   B    */
	{ SST(0x11, 0x05, SS_FATAL|EIO,
	    "L-EC uncorrectable error") },
	/*     WRO   B    */
	{ SST(0x11, 0x06, SS_FATAL|EIO,
	    "CIRC unrecovered error") },
	/*     W O   B    */
	{ SST(0x11, 0x07, SS_RDEF,
	    "Data re-synchronization error") },
	/*  T             */
	{ SST(0x11, 0x08, SS_RDEF,
	    "Incomplete block read") },
	/*  T             */
	{ SST(0x11, 0x09, SS_RDEF,
	    "No gap found") },
	/* DT    O   BK   */
	{ SST(0x11, 0x0A, SS_RDEF,
	    "Miscorrected error") },
	/* D   W O   BK   */
	{ SST(0x11, 0x0B, SS_FATAL|EIO,
	    "Unrecovered read error - recommend reassignment") },
	/* D   W O   BK   */
	{ SST(0x11, 0x0C, SS_FATAL|EIO,
	    "Unrecovered read error - recommend rewrite the data") },
	/* DT  WRO   B    */
	{ SST(0x11, 0x0D, SS_RDEF,
	    "De-compression CRC error") },
	/* DT  WRO   B    */
	{ SST(0x11, 0x0E, SS_RDEF,
	    "Cannot decompress using declared algorithm") },
	/*      R         */
	{ SST(0x11, 0x0F, SS_RDEF,
	    "Error reading UPC/EAN number") },
	/*      R         */
	{ SST(0x11, 0x10, SS_RDEF,
	    "Error reading ISRC number") },
	/*      R         */
	{ SST(0x11, 0x11, SS_RDEF,
	    "Read error - loss of streaming") },
	/* DT  WROM  B    */
	{ SST(0x11, 0x12, SS_RDEF,	/* XXX TBD */
	    "Auxiliary memory read error") },
	/* DTLPWRO AEBKVF */
	{ SST(0x11, 0x13, SS_RDEF,	/* XXX TBD */
	    "Read error - failed retransmission request") },
	/* D              */
	{ SST(0x11, 0x14, SS_RDEF,	/* XXX TBD */
	    "Read error - LBA marked bad by application client") },
	/* D   W O   BK   */
	{ SST(0x12, 0x00, SS_RDEF,
	    "Address mark not found for ID field") },
	/* D   W O   BK   */
	{ SST(0x13, 0x00, SS_RDEF,
	    "Address mark not found for data field") },
	/* DTL WRO   BK   */
	{ SST(0x14, 0x00, SS_RDEF,
	    "Recorded entity not found") },
	/* DT  WRO   BK   */
	{ SST(0x14, 0x01, SS_RDEF,
	    "Record not found") },
	/*  T             */
	{ SST(0x14, 0x02, SS_RDEF,
	    "Filemark or setmark not found") },
	/*  T             */
	{ SST(0x14, 0x03, SS_RDEF,
	    "End-of-data not found") },
	/*  T             */
	{ SST(0x14, 0x04, SS_RDEF,
	    "Block sequence error") },
	/* DT  W O   BK   */
	{ SST(0x14, 0x05, SS_RDEF,
	    "Record not found - recommend reassignment") },
	/* DT  W O   BK   */
	{ SST(0x14, 0x06, SS_RDEF,
	    "Record not found - data auto-reallocated") },
	/*  T             */
	{ SST(0x14, 0x07, SS_RDEF,	/* XXX TBD */
	    "Locate operation failure") },
	/* DTL WROM  BK   */
	{ SST(0x15, 0x00, SS_RDEF,
	    "Random positioning error") },
	/* DTL WROM  BK   */
	{ SST(0x15, 0x01, SS_RDEF,
	    "Mechanical positioning error") },
	/* DT  WRO   BK   */
	{ SST(0x15, 0x02, SS_RDEF,
	    "Positioning error detected by read of medium") },
	/* D   W O   BK   */
	{ SST(0x16, 0x00, SS_RDEF,
	    "Data synchronization mark error") },
	/* D   W O   BK   */
	{ SST(0x16, 0x01, SS_RDEF,
	    "Data sync error - data rewritten") },
	/* D   W O   BK   */
	{ SST(0x16, 0x02, SS_RDEF,
	    "Data sync error - recommend rewrite") },
	/* D   W O   BK   */
	{ SST(0x16, 0x03, SS_NOP | SSQ_PRINT_SENSE,
	    "Data sync error - data auto-reallocated") },
	/* D   W O   BK   */
	{ SST(0x16, 0x04, SS_RDEF,
	    "Data sync error - recommend reassignment") },
	/* DT  WRO   BK   */
	{ SST(0x17, 0x00, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data with no error correction applied") },
	/* DT  WRO   BK   */
	{ SST(0x17, 0x01, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data with retries") },
	/* DT  WRO   BK   */
	{ SST(0x17, 0x02, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data with positive head offset") },
	/* DT  WRO   BK   */
	{ SST(0x17, 0x03, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data with negative head offset") },
	/*     WRO   B    */
	{ SST(0x17, 0x04, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data with retries and/or CIRC applied") },
	/* D   WRO   BK   */
	{ SST(0x17, 0x05, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data using previous sector ID") },
	/* D   W O   BK   */
	{ SST(0x17, 0x06, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data without ECC - data auto-reallocated") },
	/* D   WRO   BK   */
	{ SST(0x17, 0x07, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data without ECC - recommend reassignment") },
	/* D   WRO   BK   */
	{ SST(0x17, 0x08, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data without ECC - recommend rewrite") },
	/* D   WRO   BK   */
	{ SST(0x17, 0x09, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data without ECC - data rewritten") },
	/* DT  WRO   BK   */
	{ SST(0x18, 0x00, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data with error correction applied") },
	/* D   WRO   BK   */
	{ SST(0x18, 0x01, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data with error corr. & retries applied") },
	/* D   WRO   BK   */
	{ SST(0x18, 0x02, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data - data auto-reallocated") },
	/*      R         */
	{ SST(0x18, 0x03, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data with CIRC") },
	/*      R         */
	{ SST(0x18, 0x04, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data with L-EC") },
	/* D   WRO   BK   */
	{ SST(0x18, 0x05, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data - recommend reassignment") },
	/* D   WRO   BK   */
	{ SST(0x18, 0x06, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data - recommend rewrite") },
	/* D   W O   BK   */
	{ SST(0x18, 0x07, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered data with ECC - data rewritten") },
	/*      R         */
	{ SST(0x18, 0x08, SS_RDEF,	/* XXX TBD */
	    "Recovered data with linking") },
	/* D     O    K   */
	{ SST(0x19, 0x00, SS_RDEF,
	    "Defect list error") },
	/* D     O    K   */
	{ SST(0x19, 0x01, SS_RDEF,
	    "Defect list not available") },
	/* D     O    K   */
	{ SST(0x19, 0x02, SS_RDEF,
	    "Defect list error in primary list") },
	/* D     O    K   */
	{ SST(0x19, 0x03, SS_RDEF,
	    "Defect list error in grown list") },
	/* DTLPWROMAEBKVF */
	{ SST(0x1A, 0x00, SS_RDEF,
	    "Parameter list length error") },
	/* DTLPWROMAEBKVF */
	{ SST(0x1B, 0x00, SS_RDEF,
	    "Synchronous data transfer error") },
	/* D     O   BK   */
	{ SST(0x1C, 0x00, SS_RDEF,
	    "Defect list not found") },
	/* D     O   BK   */
	{ SST(0x1C, 0x01, SS_RDEF,
	    "Primary defect list not found") },
	/* D     O   BK   */
	{ SST(0x1C, 0x02, SS_RDEF,
	    "Grown defect list not found") },
	/* DT  WRO   BK   */
	{ SST(0x1D, 0x00, SS_FATAL,
	    "Miscompare during verify operation") },
	/* D         B    */
	{ SST(0x1D, 0x01, SS_RDEF,	/* XXX TBD */
	    "Miscomparable verify of unmapped LBA") },
	/* D   W O   BK   */
	{ SST(0x1E, 0x00, SS_NOP | SSQ_PRINT_SENSE,
	    "Recovered ID with ECC correction") },
	/* D     O    K   */
	{ SST(0x1F, 0x00, SS_RDEF,
	    "Partial defect list transfer") },
	/* DTLPWROMAEBKVF */
	{ SST(0x20, 0x00, SS_FATAL | EINVAL,
	    "Invalid command operation code") },
	/* DT PWROMAEBK   */
	{ SST(0x20, 0x01, SS_RDEF,	/* XXX TBD */
	    "Access denied - initiator pending-enrolled") },
	/* DT PWROMAEBK   */
	{ SST(0x20, 0x02, SS_RDEF,	/* XXX TBD */
	    "Access denied - no access rights") },
	/* DT PWROMAEBK   */
	{ SST(0x20, 0x03, SS_RDEF,	/* XXX TBD */
	    "Access denied - invalid mgmt ID key") },
	/*  T             */
	{ SST(0x20, 0x04, SS_RDEF,	/* XXX TBD */
	    "Illegal command while in write capable state") },
	/*  T             */
	{ SST(0x20, 0x05, SS_RDEF,	/* XXX TBD */
	    "Obsolete") },
	/*  T             */
	{ SST(0x20, 0x06, SS_RDEF,	/* XXX TBD */
	    "Illegal command while in explicit address mode") },
	/*  T             */
	{ SST(0x20, 0x07, SS_RDEF,	/* XXX TBD */
	    "Illegal command while in implicit address mode") },
	/* DT PWROMAEBK   */
	{ SST(0x20, 0x08, SS_RDEF,	/* XXX TBD */
	    "Access denied - enrollment conflict") },
	/* DT PWROMAEBK   */
	{ SST(0x20, 0x09, SS_RDEF,	/* XXX TBD */
	    "Access denied - invalid LU identifier") },
	/* DT PWROMAEBK   */
	{ SST(0x20, 0x0A, SS_RDEF,	/* XXX TBD */
	    "Access denied - invalid proxy token") },
	/* DT PWROMAEBK   */
	{ SST(0x20, 0x0B, SS_RDEF,	/* XXX TBD */
	    "Access denied - ACL LUN conflict") },
	/*  T             */
	{ SST(0x20, 0x0C, SS_FATAL | EINVAL,
	    "Illegal command when not in append-only mode") },
	/* DT  WRO   BK   */
	{ SST(0x21, 0x00, SS_FATAL | EINVAL,
	    "Logical block address out of range") },
	/* DT  WROM  BK   */
	{ SST(0x21, 0x01, SS_FATAL | EINVAL,
	    "Invalid element address") },
	/*      R         */
	{ SST(0x21, 0x02, SS_RDEF,	/* XXX TBD */
	    "Invalid address for write") },
	/*      R         */
	{ SST(0x21, 0x03, SS_RDEF,	/* XXX TBD */
	    "Invalid write crossing layer jump") },
	/* D              */
	{ SST(0x22, 0x00, SS_FATAL | EINVAL,
	    "Illegal function (use 20 00, 24 00, or 26 00)") },
	/* DT P      B    */
	{ SST(0x23, 0x00, SS_RDEF,	/* XXX TBD */
	    "Invalid token operation, cause not reportable") },
	/* DT P      B    */
	{ SST(0x23, 0x01, SS_RDEF,	/* XXX TBD */
	    "Invalid token operation, unsupported token type") },
	/* DT P      B    */
	{ SST(0x23, 0x02, SS_RDEF,	/* XXX TBD */
	    "Invalid token operation, remote token usage not supported") },
	/* DT P      B    */
	{ SST(0x23, 0x03, SS_RDEF,	/* XXX TBD */
	    "Invalid token operation, remote ROD token creation not supported") },
	/* DT P      B    */
	{ SST(0x23, 0x04, SS_RDEF,	/* XXX TBD */
	    "Invalid token operation, token unknown") },
	/* DT P      B    */
	{ SST(0x23, 0x05, SS_RDEF,	/* XXX TBD */
	    "Invalid token operation, token corrupt") },
	/* DT P      B    */
	{ SST(0x23, 0x06, SS_RDEF,	/* XXX TBD */
	    "Invalid token operation, token revoked") },
	/* DT P      B    */
	{ SST(0x23, 0x07, SS_RDEF,	/* XXX TBD */
	    "Invalid token operation, token expired") },
	/* DT P      B    */
	{ SST(0x23, 0x08, SS_RDEF,	/* XXX TBD */
	    "Invalid token operation, token cancelled") },
	/* DT P      B    */
	{ SST(0x23, 0x09, SS_RDEF,	/* XXX TBD */
	    "Invalid token operation, token deleted") },
	/* DT P      B    */
	{ SST(0x23, 0x0A, SS_RDEF,	/* XXX TBD */
	    "Invalid token operation, invalid token length") },
	/* DTLPWROMAEBKVF */
	{ SST(0x24, 0x00, SS_FATAL | EINVAL,
	    "Invalid field in CDB") },
	/* DTLPWRO AEBKVF */
	{ SST(0x24, 0x01, SS_RDEF,	/* XXX TBD */
	    "CDB decryption error") },
	/*  T             */
	{ SST(0x24, 0x02, SS_RDEF,	/* XXX TBD */
	    "Obsolete") },
	/*  T             */
	{ SST(0x24, 0x03, SS_RDEF,	/* XXX TBD */
	    "Obsolete") },
	/*              F */
	{ SST(0x24, 0x04, SS_RDEF,	/* XXX TBD */
	    "Security audit value frozen") },
	/*              F */
	{ SST(0x24, 0x05, SS_RDEF,	/* XXX TBD */
	    "Security working key frozen") },
	/*              F */
	{ SST(0x24, 0x06, SS_RDEF,	/* XXX TBD */
	    "NONCE not unique") },
	/*              F */
	{ SST(0x24, 0x07, SS_RDEF,	/* XXX TBD */
	    "NONCE timestamp out of range") },
	/* DT   R MAEBKV  */
	{ SST(0x24, 0x08, SS_RDEF,	/* XXX TBD */
	    "Invalid XCDB") },
	/* DTLPWROMAEBKVF */
	{ SST(0x25, 0x00, SS_FATAL | ENXIO | SSQ_LOST,
	    "Logical unit not supported") },
	/* DTLPWROMAEBKVF */
	{ SST(0x26, 0x00, SS_FATAL | EINVAL,
	    "Invalid field in parameter list") },
	/* DTLPWROMAEBKVF */
	{ SST(0x26, 0x01, SS_FATAL | EINVAL,
	    "Parameter not supported") },
	/* DTLPWROMAEBKVF */
	{ SST(0x26, 0x02, SS_FATAL | EINVAL,
	    "Parameter value invalid") },
	/* DTLPWROMAE K   */
	{ SST(0x26, 0x03, SS_FATAL | EINVAL,
	    "Threshold parameters not supported") },
	/* DTLPWROMAEBKVF */
	{ SST(0x26, 0x04, SS_FATAL | EINVAL,
	    "Invalid release of persistent reservation") },
	/* DTLPWRO A BK   */
	{ SST(0x26, 0x05, SS_RDEF,	/* XXX TBD */
	    "Data decryption error") },
	/* DTLPWRO    K   */
	{ SST(0x26, 0x06, SS_RDEF,	/* XXX TBD */
	    "Too many target descriptors") },
	/* DTLPWRO    K   */
	{ SST(0x26, 0x07, SS_RDEF,	/* XXX TBD */
	    "Unsupported target descriptor type code") },
	/* DTLPWRO    K   */
	{ SST(0x26, 0x08, SS_RDEF,	/* XXX TBD */
	    "Too many segment descriptors") },
	/* DTLPWRO    K   */
	{ SST(0x26, 0x09, SS_RDEF,	/* XXX TBD */
	    "Unsupported segment descriptor type code") },
	/* DTLPWRO    K   */
	{ SST(0x26, 0x0A, SS_RDEF,	/* XXX TBD */
	    "Unexpected inexact segment") },
	/* DTLPWRO    K   */
	{ SST(0x26, 0x0B, SS_RDEF,	/* XXX TBD */
	    "Inline data length exceeded") },
	/* DTLPWRO    K   */
	{ SST(0x26, 0x0C, SS_RDEF,	/* XXX TBD */
	    "Invalid operation for copy source or destination") },
	/* DTLPWRO    K   */
	{ SST(0x26, 0x0D, SS_RDEF,	/* XXX TBD */
	    "Copy segment granularity violation") },
	/* DT PWROMAEBK   */
	{ SST(0x26, 0x0E, SS_RDEF,	/* XXX TBD */
	    "Invalid parameter while port is enabled") },
	/*              F */
	{ SST(0x26, 0x0F, SS_RDEF,	/* XXX TBD */
	    "Invalid data-out buffer integrity check value") },
	/*  T             */
	{ SST(0x26, 0x10, SS_RDEF,	/* XXX TBD */
	    "Data decryption key fail limit reached") },
	/*  T             */
	{ SST(0x26, 0x11, SS_RDEF,	/* XXX TBD */
	    "Incomplete key-associated data set") },
	/*  T             */
	{ SST(0x26, 0x12, SS_RDEF,	/* XXX TBD */
	    "Vendor specific key reference not found") },
	/* DT  WRO   BK   */
	{ SST(0x27, 0x00, SS_FATAL | EACCES,
	    "Write protected") },
	/* DT  WRO   BK   */
	{ SST(0x27, 0x01, SS_FATAL | EACCES,
	    "Hardware write protected") },
	/* DT  WRO   BK   */
	{ SST(0x27, 0x02, SS_FATAL | EACCES,
	    "Logical unit software write protected") },
	/*  T   R         */
	{ SST(0x27, 0x03, SS_FATAL | EACCES,
	    "Associated write protect") },
	/*  T   R         */
	{ SST(0x27, 0x04, SS_FATAL | EACCES,
	    "Persistent write protect") },
	/*  T   R         */
	{ SST(0x27, 0x05, SS_FATAL | EACCES,
	    "Permanent write protect") },
	/*      R       F */
	{ SST(0x27, 0x06, SS_RDEF,	/* XXX TBD */
	    "Conditional write protect") },
	/* D         B    */
	{ SST(0x27, 0x07, SS_RDEF,	/* XXX TBD */
	    "Space allocation failed write protect") },
	/* DTLPWROMAEBKVF */
	{ SST(0x28, 0x00, SS_FATAL | ENXIO,
	    "Not ready to ready change, medium may have changed") },
	/* DT  WROM  B    */
	{ SST(0x28, 0x01, SS_FATAL | ENXIO,
	    "Import or export element accessed") },
	/*      R         */
	{ SST(0x28, 0x02, SS_RDEF,	/* XXX TBD */
	    "Format-layer may have changed") },
	/*        M       */
	{ SST(0x28, 0x03, SS_RDEF,	/* XXX TBD */
	    "Import/export element accessed, medium changed") },
	/*
	 * XXX JGibbs - All of these should use the same errno, but I don't
	 * think ENXIO is the correct choice.  Should we borrow from
	 * the networking errnos?  ECONNRESET anyone?
	 */
	/* DTLPWROMAEBKVF */
	{ SST(0x29, 0x00, SS_FATAL | ENXIO,
	    "Power on, reset, or bus device reset occurred") },
	/* DTLPWROMAEBKVF */
	{ SST(0x29, 0x01, SS_RDEF,
	    "Power on occurred") },
	/* DTLPWROMAEBKVF */
	{ SST(0x29, 0x02, SS_RDEF,
	    "SCSI bus reset occurred") },
	/* DTLPWROMAEBKVF */
	{ SST(0x29, 0x03, SS_RDEF,
	    "Bus device reset function occurred") },
	/* DTLPWROMAEBKVF */
	{ SST(0x29, 0x04, SS_RDEF,
	    "Device internal reset") },
	/* DTLPWROMAEBKVF */
	{ SST(0x29, 0x05, SS_RDEF,
	    "Transceiver mode changed to single-ended") },
	/* DTLPWROMAEBKVF */
	{ SST(0x29, 0x06, SS_RDEF,
	    "Transceiver mode changed to LVD") },
	/* DTLPWROMAEBKVF */
	{ SST(0x29, 0x07, SS_RDEF,	/* XXX TBD */
	    "I_T nexus loss occurred") },
	/* DTL WROMAEBKVF */
	{ SST(0x2A, 0x00, SS_RDEF,
	    "Parameters changed") },
	/* DTL WROMAEBKVF */
	{ SST(0x2A, 0x01, SS_RDEF,
	    "Mode parameters changed") },
	/* DTL WROMAE K   */
	{ SST(0x2A, 0x02, SS_RDEF,
	    "Log parameters changed") },
	/* DTLPWROMAE K   */
	{ SST(0x2A, 0x03, SS_RDEF,
	    "Reservations preempted") },
	/* DTLPWROMAE     */
	{ SST(0x2A, 0x04, SS_RDEF,	/* XXX TBD */
	    "Reservations released") },
	/* DTLPWROMAE     */
	{ SST(0x2A, 0x05, SS_RDEF,	/* XXX TBD */
	    "Registrations preempted") },
	/* DTLPWROMAEBKVF */
	{ SST(0x2A, 0x06, SS_RDEF,	/* XXX TBD */
	    "Asymmetric access state changed") },
	/* DTLPWROMAEBKVF */
	{ SST(0x2A, 0x07, SS_RDEF,	/* XXX TBD */
	    "Implicit asymmetric access state transition failed") },
	/* DT  WROMAEBKVF */
	{ SST(0x2A, 0x08, SS_RDEF,	/* XXX TBD */
	    "Priority changed") },
	/* D              */
	{ SST(0x2A, 0x09, SS_RDEF,	/* XXX TBD */
	    "Capacity data has changed") },
	/* DT             */
	{ SST(0x2A, 0x0A, SS_RDEF,	/* XXX TBD */
	    "Error history I_T nexus cleared") },
	/* DT             */
	{ SST(0x2A, 0x0B, SS_RDEF,	/* XXX TBD */
	    "Error history snapshot released") },
	/*              F */
	{ SST(0x2A, 0x0C, SS_RDEF,	/* XXX TBD */
	    "Error recovery attributes have changed") },
	/*  T             */
	{ SST(0x2A, 0x0D, SS_RDEF,	/* XXX TBD */
	    "Data encryption capabilities changed") },
	/* DT     M E  V  */
	{ SST(0x2A, 0x10, SS_RDEF,	/* XXX TBD */
	    "Timestamp changed") },
	/*  T             */
	{ SST(0x2A, 0x11, SS_RDEF,	/* XXX TBD */
	    "Data encryption parameters changed by another I_T nexus") },
	/*  T             */
	{ SST(0x2A, 0x12, SS_RDEF,	/* XXX TBD */
	    "Data encryption parameters changed by vendor specific event") },
	/*  T             */
	{ SST(0x2A, 0x13, SS_RDEF,	/* XXX TBD */
	    "Data encryption key instance counter has changed") },
	/* DT   R MAEBKV  */
	{ SST(0x2A, 0x14, SS_RDEF,	/* XXX TBD */
	    "SA creation capabilities data has changed") },
	/*  T     M    V  */
	{ SST(0x2A, 0x15, SS_RDEF,	/* XXX TBD */
	    "Medium removal prevention preempted") },
	/* DTLPWRO    K   */
	{ SST(0x2B, 0x00, SS_RDEF,
	    "Copy cannot execute since host cannot disconnect") },
	/* DTLPWROMAEBKVF */
	{ SST(0x2C, 0x00, SS_RDEF,
	    "Command sequence error") },
	/*                */
	{ SST(0x2C, 0x01, SS_RDEF,
	    "Too many windows specified") },
	/*                */
	{ SST(0x2C, 0x02, SS_RDEF,
	    "Invalid combination of windows specified") },
	/*      R         */
	{ SST(0x2C, 0x03, SS_RDEF,
	    "Current program area is not empty") },
	/*      R         */
	{ SST(0x2C, 0x04, SS_RDEF,
	    "Current program area is empty") },
	/*           B    */
	{ SST(0x2C, 0x05, SS_RDEF,	/* XXX TBD */
	    "Illegal power condition request") },
	/*      R         */
	{ SST(0x2C, 0x06, SS_RDEF,	/* XXX TBD */
	    "Persistent prevent conflict") },
	/* DTLPWROMAEBKVF */
	{ SST(0x2C, 0x07, SS_RDEF,	/* XXX TBD */
	    "Previous busy status") },
	/* DTLPWROMAEBKVF */
	{ SST(0x2C, 0x08, SS_RDEF,	/* XXX TBD */
	    "Previous task set full status") },
	/* DTLPWROM EBKVF */
	{ SST(0x2C, 0x09, SS_RDEF,	/* XXX TBD */
	    "Previous reservation conflict status") },
	/*              F */
	{ SST(0x2C, 0x0A, SS_RDEF,	/* XXX TBD */
	    "Partition or collection contains user objects") },
	/*  T             */
	{ SST(0x2C, 0x0B, SS_RDEF,	/* XXX TBD */
	    "Not reserved") },
	/* D              */
	{ SST(0x2C, 0x0C, SS_RDEF,	/* XXX TBD */
	    "ORWRITE generation does not match") },
	/*  T             */
	{ SST(0x2D, 0x00, SS_RDEF,
	    "Overwrite error on update in place") },
	/*      R         */
	{ SST(0x2E, 0x00, SS_RDEF,	/* XXX TBD */
	    "Insufficient time for operation") },
	/* DTLPWROMAEBKVF */
	{ SST(0x2F, 0x00, SS_RDEF,
	    "Commands cleared by another initiator") },
	/* D              */
	{ SST(0x2F, 0x01, SS_RDEF,	/* XXX TBD */
	    "Commands cleared by power loss notification") },
	/* DTLPWROMAEBKVF */
	{ SST(0x2F, 0x02, SS_RDEF,	/* XXX TBD */
	    "Commands cleared by device server") },
	/* DT  WROM  BK   */
	{ SST(0x30, 0x00, SS_RDEF,
	    "Incompatible medium installed") },
	/* DT  WRO   BK   */
	{ SST(0x30, 0x01, SS_RDEF,
	    "Cannot read medium - unknown format") },
	/* DT  WRO   BK   */
	{ SST(0x30, 0x02, SS_RDEF,
	    "Cannot read medium - incompatible format") },
	/* DT   R     K   */
	{ SST(0x30, 0x03, SS_RDEF,
	    "Cleaning cartridge installed") },
	/* DT  WRO   BK   */
	{ SST(0x30, 0x04, SS_RDEF,
	    "Cannot write medium - unknown format") },
	/* DT  WRO   BK   */
	{ SST(0x30, 0x05, SS_RDEF,
	    "Cannot write medium - incompatible format") },
	/* DT  WRO   B    */
	{ SST(0x30, 0x06, SS_RDEF,
	    "Cannot format medium - incompatible medium") },
	/* DTL WROMAEBKVF */
	{ SST(0x30, 0x07, SS_RDEF,
	    "Cleaning failure") },
	/*      R         */
	{ SST(0x30, 0x08, SS_RDEF,
	    "Cannot write - application code mismatch") },
	/*      R         */
	{ SST(0x30, 0x09, SS_RDEF,
	    "Current session not fixated for append") },
	/* DT  WRO AEBK   */
	{ SST(0x30, 0x0A, SS_RDEF,	/* XXX TBD */
	    "Cleaning request rejected") },
	/*  T             */
	{ SST(0x30, 0x0C, SS_RDEF,	/* XXX TBD */
	    "WORM medium - overwrite attempted") },
	/*  T             */
	{ SST(0x30, 0x0D, SS_RDEF,	/* XXX TBD */
	    "WORM medium - integrity check") },
	/*      R         */
	{ SST(0x30, 0x10, SS_RDEF,	/* XXX TBD */
	    "Medium not formatted") },
	/*        M       */
	{ SST(0x30, 0x11, SS_RDEF,	/* XXX TBD */
	    "Incompatible volume type") },
	/*        M       */
	{ SST(0x30, 0x12, SS_RDEF,	/* XXX TBD */
	    "Incompatible volume qualifier") },
	/*        M       */
	{ SST(0x30, 0x13, SS_RDEF,	/* XXX TBD */
	    "Cleaning volume expired") },
	/* DT  WRO   BK   */
	{ SST(0x31, 0x00, SS_RDEF,
	    "Medium format corrupted") },
	/* D L  RO   B    */
	{ SST(0x31, 0x01, SS_RDEF,
	    "Format command failed") },
	/*      R         */
	{ SST(0x31, 0x02, SS_RDEF,	/* XXX TBD */
	    "Zoned formatting failed due to spare linking") },
	/* D         B    */
	{ SST(0x31, 0x03, SS_RDEF,	/* XXX TBD */
	    "SANITIZE command failed") },
	/* D   W O   BK   */
	{ SST(0x32, 0x00, SS_RDEF,
	    "No defect spare location available") },
	/* D   W O   BK   */
	{ SST(0x32, 0x01, SS_RDEF,
	    "Defect list update failure") },
	/*  T             */
	{ SST(0x33, 0x00, SS_RDEF,
	    "Tape length error") },
	/* DTLPWROMAEBKVF */
	{ SST(0x34, 0x00, SS_RDEF,
	    "Enclosure failure") },
	/* DTLPWROMAEBKVF */
	{ SST(0x35, 0x00, SS_RDEF,
	    "Enclosure services failure") },
	/* DTLPWROMAEBKVF */
	{ SST(0x35, 0x01, SS_RDEF,
	    "Unsupported enclosure function") },
	/* DTLPWROMAEBKVF */
	{ SST(0x35, 0x02, SS_RDEF,
	    "Enclosure services unavailable") },
	/* DTLPWROMAEBKVF */
	{ SST(0x35, 0x03, SS_RDEF,
	    "Enclosure services transfer failure") },
	/* DTLPWROMAEBKVF */
	{ SST(0x35, 0x04, SS_RDEF,
	    "Enclosure services transfer refused") },
	/* DTL WROMAEBKVF */
	{ SST(0x35, 0x05, SS_RDEF,	/* XXX TBD */
	    "Enclosure services checksum error") },
	/*   L            */
	{ SST(0x36, 0x00, SS_RDEF,
	    "Ribbon, ink, or toner failure") },
	/* DTL WROMAEBKVF */
	{ SST(0x37, 0x00, SS_RDEF,
	    "Rounded parameter") },
	/*           B    */
	{ SST(0x38, 0x00, SS_RDEF,	/* XXX TBD */
	    "Event status notification") },
	/*           B    */
	{ SST(0x38, 0x02, SS_RDEF,	/* XXX TBD */
	    "ESN - power management class event") },
	/*           B    */
	{ SST(0x38, 0x04, SS_RDEF,	/* XXX TBD */
	    "ESN - media class event") },
	/*           B    */
	{ SST(0x38, 0x06, SS_RDEF,	/* XXX TBD */
	    "ESN - device busy class event") },
	/* D              */
	{ SST(0x38, 0x07, SS_RDEF,	/* XXX TBD */
	    "Thin provisioning soft threshold reached") },
	/* DTL WROMAE K   */
	{ SST(0x39, 0x00, SS_RDEF,
	    "Saving parameters not supported") },
	/* DTL WROM  BK   */
	{ SST(0x3A, 0x00, SS_FATAL | ENXIO,
	    "Medium not present") },
	/* DT  WROM  BK   */
	{ SST(0x3A, 0x01, SS_FATAL | ENXIO,
	    "Medium not present - tray closed") },
	/* DT  WROM  BK   */
	{ SST(0x3A, 0x02, SS_FATAL | ENXIO,
	    "Medium not present - tray open") },
	/* DT  WROM  B    */
	{ SST(0x3A, 0x03, SS_RDEF,	/* XXX TBD */
	    "Medium not present - loadable") },
	/* DT  WRO   B    */
	{ SST(0x3A, 0x04, SS_RDEF,	/* XXX TBD */
	    "Medium not present - medium auxiliary memory accessible") },
	/*  TL            */
	{ SST(0x3B, 0x00, SS_RDEF,
	    "Sequential positioning error") },
	/*  T             */
	{ SST(0x3B, 0x01, SS_RDEF,
	    "Tape position error at beginning-of-medium") },
	/*  T             */
	{ SST(0x3B, 0x02, SS_RDEF,
	    "Tape position error at end-of-medium") },
	/*   L            */
	{ SST(0x3B, 0x03, SS_RDEF,
	    "Tape or electronic vertical forms unit not ready") },
	/*   L            */
	{ SST(0x3B, 0x04, SS_RDEF,
	    "Slew failure") },
	/*   L            */
	{ SST(0x3B, 0x05, SS_RDEF,
	    "Paper jam") },
	/*   L            */
	{ SST(0x3B, 0x06, SS_RDEF,
	    "Failed to sense top-of-form") },
	/*   L            */
	{ SST(0x3B, 0x07, SS_RDEF,
	    "Failed to sense bottom-of-form") },
	/*  T             */
	{ SST(0x3B, 0x08, SS_RDEF,
	    "Reposition error") },
	/*                */
	{ SST(0x3B, 0x09, SS_RDEF,
	    "Read past end of medium") },
	/*                */
	{ SST(0x3B, 0x0A, SS_RDEF,
	    "Read past beginning of medium") },
	/*                */
	{ SST(0x3B, 0x0B, SS_RDEF,
	    "Position past end of medium") },
	/*  T             */
	{ SST(0x3B, 0x0C, SS_RDEF,
	    "Position past beginning of medium") },
	/* DT  WROM  BK   */
	{ SST(0x3B, 0x0D, SS_FATAL | ENOSPC,
	    "Medium destination element full") },
	/* DT  WROM  BK   */
	{ SST(0x3B, 0x0E, SS_RDEF,
	    "Medium source element empty") },
	/*      R         */
	{ SST(0x3B, 0x0F, SS_RDEF,
	    "End of medium reached") },
	/* DT  WROM  BK   */
	{ SST(0x3B, 0x11, SS_RDEF,
	    "Medium magazine not accessible") },
	/* DT  WROM  BK   */
	{ SST(0x3B, 0x12, SS_RDEF,
	    "Medium magazine removed") },
	/* DT  WROM  BK   */
	{ SST(0x3B, 0x13, SS_RDEF,
	    "Medium magazine inserted") },
	/* DT  WROM  BK   */
	{ SST(0x3B, 0x14, SS_RDEF,
	    "Medium magazine locked") },
	/* DT  WROM  BK   */
	{ SST(0x3B, 0x15, SS_RDEF,
	    "Medium magazine unlocked") },
	/*      R         */
	{ SST(0x3B, 0x16, SS_RDEF,	/* XXX TBD */
	    "Mechanical positioning or changer error") },
	/*              F */
	{ SST(0x3B, 0x17, SS_RDEF,	/* XXX TBD */
	    "Read past end of user object") },
	/*        M       */
	{ SST(0x3B, 0x18, SS_RDEF,	/* XXX TBD */
	    "Element disabled") },
	/*        M       */
	{ SST(0x3B, 0x19, SS_RDEF,	/* XXX TBD */
	    "Element enabled") },
	/*        M       */
	{ SST(0x3B, 0x1A, SS_RDEF,	/* XXX TBD */
	    "Data transfer device removed") },
	/*        M       */
	{ SST(0x3B, 0x1B, SS_RDEF,	/* XXX TBD */
	    "Data transfer device inserted") },
	/*  T             */
	{ SST(0x3B, 0x1C, SS_RDEF,	/* XXX TBD */
	    "Too many logical objects on partition to support operation") },
	/* DTLPWROMAE K   */
	{ SST(0x3D, 0x00, SS_RDEF,
	    "Invalid bits in IDENTIFY message") },
	/* DTLPWROMAEBKVF */
	{ SST(0x3E, 0x00, SS_RDEF,
	    "Logical unit has not self-configured yet") },
	/* DTLPWROMAEBKVF */
	{ SST(0x3E, 0x01, SS_RDEF,
	    "Logical unit failure") },
	/* DTLPWROMAEBKVF */
	{ SST(0x3E, 0x02, SS_RDEF,
	    "Timeout on logical unit") },
	/* DTLPWROMAEBKVF */
	{ SST(0x3E, 0x03, SS_RDEF,	/* XXX TBD */
	    "Logical unit failed self-test") },
	/* DTLPWROMAEBKVF */
	{ SST(0x3E, 0x04, SS_RDEF,	/* XXX TBD */
	    "Logical unit unable to update self-test log") },
	/* DTLPWROMAEBKVF */
	{ SST(0x3F, 0x00, SS_RDEF,
	    "Target operating conditions have changed") },
	/* DTLPWROMAEBKVF */
	{ SST(0x3F, 0x01, SS_RDEF,
	    "Microcode has been changed") },
	/* DTLPWROM  BK   */
	{ SST(0x3F, 0x02, SS_RDEF,
	    "Changed operating definition") },
	/* DTLPWROMAEBKVF */
	{ SST(0x3F, 0x03, SS_RDEF,
	    "INQUIRY data has changed") },
	/* DT  WROMAEBK   */
	{ SST(0x3F, 0x04, SS_RDEF,
	    "Component device attached") },
	/* DT  WROMAEBK   */
	{ SST(0x3F, 0x05, SS_RDEF,
	    "Device identifier changed") },
	/* DT  WROMAEB    */
	{ SST(0x3F, 0x06, SS_RDEF,
	    "Redundancy group created or modified") },
	/* DT  WROMAEB    */
	{ SST(0x3F, 0x07, SS_RDEF,
	    "Redundancy group deleted") },
	/* DT  WROMAEB    */
	{ SST(0x3F, 0x08, SS_RDEF,
	    "Spare created or modified") },
	/* DT  WROMAEB    */
	{ SST(0x3F, 0x09, SS_RDEF,
	    "Spare deleted") },
	/* DT  WROMAEBK   */
	{ SST(0x3F, 0x0A, SS_RDEF,
	    "Volume set created or modified") },
	/* DT  WROMAEBK   */
	{ SST(0x3F, 0x0B, SS_RDEF,
	    "Volume set deleted") },
	/* DT  WROMAEBK   */
	{ SST(0x3F, 0x0C, SS_RDEF,
	    "Volume set deassigned") },
	/* DT  WROMAEBK   */
	{ SST(0x3F, 0x0D, SS_RDEF,
	    "Volume set reassigned") },
	/* DTLPWROMAE     */
	{ SST(0x3F, 0x0E, SS_RDEF | SSQ_RESCAN ,
	    "Reported LUNs data has changed") },
	/* DTLPWROMAEBKVF */
	{ SST(0x3F, 0x0F, SS_RDEF,	/* XXX TBD */
	    "Echo buffer overwritten") },
	/* DT  WROM  B    */
	{ SST(0x3F, 0x10, SS_RDEF,	/* XXX TBD */
	    "Medium loadable") },
	/* DT  WROM  B    */
	{ SST(0x3F, 0x11, SS_RDEF,	/* XXX TBD */
	    "Medium auxiliary memory accessible") },
	/* DTLPWR MAEBK F */
	{ SST(0x3F, 0x12, SS_RDEF,	/* XXX TBD */
	    "iSCSI IP address added") },
	/* DTLPWR MAEBK F */
	{ SST(0x3F, 0x13, SS_RDEF,	/* XXX TBD */
	    "iSCSI IP address removed") },
	/* DTLPWR MAEBK F */
	{ SST(0x3F, 0x14, SS_RDEF,	/* XXX TBD */
	    "iSCSI IP address changed") },
	/* D              */
	{ SST(0x40, 0x00, SS_RDEF,
	    "RAM failure") },		/* deprecated - use 40 NN instead */
	/* DTLPWROMAEBKVF */
	{ SST(0x40, 0x80, SS_RDEF,
	    "Diagnostic failure: ASCQ = Component ID") },
	/* DTLPWROMAEBKVF */
	{ SST(0x40, 0xFF, SS_RDEF | SSQ_RANGE,
	    NULL) },			/* Range 0x80->0xFF */
	/* D              */
	{ SST(0x41, 0x00, SS_RDEF,
	    "Data path failure") },	/* deprecated - use 40 NN instead */
	/* D              */
	{ SST(0x42, 0x00, SS_RDEF,
	    "Power-on or self-test failure") },
					/* deprecated - use 40 NN instead */
	/* DTLPWROMAEBKVF */
	{ SST(0x43, 0x00, SS_RDEF,
	    "Message error") },
	/* DTLPWROMAEBKVF */
	{ SST(0x44, 0x00, SS_RDEF,
	    "Internal target failure") },
	/* DT P   MAEBKVF */
	{ SST(0x44, 0x01, SS_RDEF,	/* XXX TBD */
	    "Persistent reservation information lost") },
	/* DT        B    */
	{ SST(0x44, 0x71, SS_RDEF,	/* XXX TBD */
	    "ATA device failed set features") },
	/* DTLPWROMAEBKVF */
	{ SST(0x45, 0x00, SS_RDEF,
	    "Select or reselect failure") },
	/* DTLPWROM  BK   */
	{ SST(0x46, 0x00, SS_RDEF,
	    "Unsuccessful soft reset") },
	/* DTLPWROMAEBKVF */
	{ SST(0x47, 0x00, SS_RDEF,
	    "SCSI parity error") },
	/* DTLPWROMAEBKVF */
	{ SST(0x47, 0x01, SS_RDEF,	/* XXX TBD */
	    "Data phase CRC error detected") },
	/* DTLPWROMAEBKVF */
	{ SST(0x47, 0x02, SS_RDEF,	/* XXX TBD */
	    "SCSI parity error detected during ST data phase") },
	/* DTLPWROMAEBKVF */
	{ SST(0x47, 0x03, SS_RDEF,	/* XXX TBD */
	    "Information unit iuCRC error detected") },
	/* DTLPWROMAEBKVF */
	{ SST(0x47, 0x04, SS_RDEF,	/* XXX TBD */
	    "Asynchronous information protection error detected") },
	/* DTLPWROMAEBKVF */
	{ SST(0x47, 0x05, SS_RDEF,	/* XXX TBD */
	    "Protocol service CRC error") },
	/* DT     MAEBKVF */
	{ SST(0x47, 0x06, SS_RDEF,	/* XXX TBD */
	    "PHY test function in progress") },
	/* DT PWROMAEBK   */
	{ SST(0x47, 0x7F, SS_RDEF,	/* XXX TBD */
	    "Some commands cleared by iSCSI protocol event") },
	/* DTLPWROMAEBKVF */
	{ SST(0x48, 0x00, SS_RDEF,
	    "Initiator detected error message received") },
	/* DTLPWROMAEBKVF */
	{ SST(0x49, 0x00, SS_RDEF,
	    "Invalid message error") },
	/* DTLPWROMAEBKVF */
	{ SST(0x4A, 0x00, SS_RDEF,
	    "Command phase error") },
	/* DTLPWROMAEBKVF */
	{ SST(0x4B, 0x00, SS_RDEF,
	    "Data phase error") },
	/* DT PWROMAEBK   */
	{ SST(0x4B, 0x01, SS_RDEF,	/* XXX TBD */
	    "Invalid target port transfer tag received") },
	/* DT PWROMAEBK   */
	{ SST(0x4B, 0x02, SS_RDEF,	/* XXX TBD */
	    "Too much write data") },
	/* DT PWROMAEBK   */
	{ SST(0x4B, 0x03, SS_RDEF,	/* XXX TBD */
	    "ACK/NAK timeout") },
	/* DT PWROMAEBK   */
	{ SST(0x4B, 0x04, SS_RDEF,	/* XXX TBD */
	    "NAK received") },
	/* DT PWROMAEBK   */
	{ SST(0x4B, 0x05, SS_RDEF,	/* XXX TBD */
	    "Data offset error") },
	/* DT PWROMAEBK   */
	{ SST(0x4B, 0x06, SS_RDEF,	/* XXX TBD */
	    "Initiator response timeout") },
	/* DT PWROMAEBK F */
	{ SST(0x4B, 0x07, SS_RDEF,	/* XXX TBD */
	    "Connection lost") },
	/* DT PWROMAEBK F */
	{ SST(0x4B, 0x08, SS_RDEF,	/* XXX TBD */
	    "Data-in buffer overflow - data buffer size") },
	/* DT PWROMAEBK F */
	{ SST(0x4B, 0x09, SS_RDEF,	/* XXX TBD */
	    "Data-in buffer overflow - data buffer descriptor area") },
	/* DT PWROMAEBK F */
	{ SST(0x4B, 0x0A, SS_RDEF,	/* XXX TBD */
	    "Data-in buffer error") },
	/* DT PWROMAEBK F */
	{ SST(0x4B, 0x0B, SS_RDEF,	/* XXX TBD */
	    "Data-out buffer overflow - data buffer size") },
	/* DT PWROMAEBK F */
	{ SST(0x4B, 0x0C, SS_RDEF,	/* XXX TBD */
	    "Data-out buffer overflow - data buffer descriptor area") },
	/* DT PWROMAEBK F */
	{ SST(0x4B, 0x0D, SS_RDEF,	/* XXX TBD */
	    "Data-out buffer error") },
	/* DTLPWROMAEBKVF */
	{ SST(0x4C, 0x00, SS_RDEF,
	    "Logical unit failed self-configuration") },
	/* DTLPWROMAEBKVF */
	{ SST(0x4D, 0x00, SS_RDEF,
	    "Tagged overlapped commands: ASCQ = Queue tag ID") },
	/* DTLPWROMAEBKVF */
	{ SST(0x4D, 0xFF, SS_RDEF | SSQ_RANGE,
	    NULL) },			/* Range 0x00->0xFF */
	/* DTLPWROMAEBKVF */
	{ SST(0x4E, 0x00, SS_RDEF,
	    "Overlapped commands attempted") },
	/*  T             */
	{ SST(0x50, 0x00, SS_RDEF,
	    "Write append error") },
	/*  T             */
	{ SST(0x50, 0x01, SS_RDEF,
	    "Write append position error") },
	/*  T             */
	{ SST(0x50, 0x02, SS_RDEF,
	    "Position error related to timing") },
	/*  T   RO        */
	{ SST(0x51, 0x00, SS_RDEF,
	    "Erase failure") },
	/*      R         */
	{ SST(0x51, 0x01, SS_RDEF,	/* XXX TBD */
	    "Erase failure - incomplete erase operation detected") },
	/*  T             */
	{ SST(0x52, 0x00, SS_RDEF,
	    "Cartridge fault") },
	/* DTL WROM  BK   */
	{ SST(0x53, 0x00, SS_RDEF,
	    "Media load or eject failed") },
	/*  T             */
	{ SST(0x53, 0x01, SS_RDEF,
	    "Unload tape failure") },
	/* DT  WROM  BK   */
	{ SST(0x53, 0x02, SS_RDEF,
	    "Medium removal prevented") },
	/*        M       */
	{ SST(0x53, 0x03, SS_RDEF,	/* XXX TBD */
	    "Medium removal prevented by data transfer element") },
	/*  T             */
	{ SST(0x53, 0x04, SS_RDEF,	/* XXX TBD */
	    "Medium thread or unthread failure") },
	/*        M       */
	{ SST(0x53, 0x05, SS_RDEF,	/* XXX TBD */
	    "Volume identifier invalid") },
	/*  T             */
	{ SST(0x53, 0x06, SS_RDEF,	/* XXX TBD */
	    "Volume identifier missing") },
	/*        M       */
	{ SST(0x53, 0x07, SS_RDEF,	/* XXX TBD */
	    "Duplicate volume identifier") },
	/*        M       */
	{ SST(0x53, 0x08, SS_RDEF,	/* XXX TBD */
	    "Element status unknown") },
	/*    P           */
	{ SST(0x54, 0x00, SS_RDEF,
	    "SCSI to host system interface failure") },
	/*    P           */
	{ SST(0x55, 0x00, SS_RDEF,
	    "System resource failure") },
	/* D     O   BK   */
	{ SST(0x55, 0x01, SS_FATAL | ENOSPC,
	    "System buffer full") },
	/* DTLPWROMAE K   */
	{ SST(0x55, 0x02, SS_RDEF,	/* XXX TBD */
	    "Insufficient reservation resources") },
	/* DTLPWROMAE K   */
	{ SST(0x55, 0x03, SS_RDEF,	/* XXX TBD */
	    "Insufficient resources") },
	/* DTLPWROMAE K   */
	{ SST(0x55, 0x04, SS_RDEF,	/* XXX TBD */
	    "Insufficient registration resources") },
	/* DT PWROMAEBK   */
	{ SST(0x55, 0x05, SS_RDEF,	/* XXX TBD */
	    "Insufficient access control resources") },
	/* DT  WROM  B    */
	{ SST(0x55, 0x06, SS_RDEF,	/* XXX TBD */
	    "Auxiliary memory out of space") },
	/*              F */
	{ SST(0x55, 0x07, SS_RDEF,	/* XXX TBD */
	    "Quota error") },
	/*  T             */
	{ SST(0x55, 0x08, SS_RDEF,	/* XXX TBD */
	    "Maximum number of supplemental decryption keys exceeded") },
	/*        M       */
	{ SST(0x55, 0x09, SS_RDEF,	/* XXX TBD */
	    "Medium auxiliary memory not accessible") },
	/*        M       */
	{ SST(0x55, 0x0A, SS_RDEF,	/* XXX TBD */
	    "Data currently unavailable") },
	/* DTLPWROMAEBKVF */
	{ SST(0x55, 0x0B, SS_RDEF,	/* XXX TBD */
	    "Insufficient power for operation") },
	/* DT P      B    */
	{ SST(0x55, 0x0C, SS_RDEF,	/* XXX TBD */
	    "Insufficient resources to create ROD") },
	/* DT P      B    */
	{ SST(0x55, 0x0D, SS_RDEF,	/* XXX TBD */
	    "Insufficient resources to create ROD token") },
	/*      R         */
	{ SST(0x57, 0x00, SS_RDEF,
	    "Unable to recover table-of-contents") },
	/*       O        */
	{ SST(0x58, 0x00, SS_RDEF,
	    "Generation does not exist") },
	/*       O        */
	{ SST(0x59, 0x00, SS_RDEF,
	    "Updated block read") },
	/* DTLPWRO   BK   */
	{ SST(0x5A, 0x00, SS_RDEF,
	    "Operator request or state change input") },
	/* DT  WROM  BK   */
	{ SST(0x5A, 0x01, SS_RDEF,
	    "Operator medium removal request") },
	/* DT  WRO A BK   */
	{ SST(0x5A, 0x02, SS_RDEF,
	    "Operator selected write protect") },
	/* DT  WRO A BK   */
	{ SST(0x5A, 0x03, SS_RDEF,
	    "Operator selected write permit") },
	/* DTLPWROM   K   */
	{ SST(0x5B, 0x00, SS_RDEF,
	    "Log exception") },
	/* DTLPWROM   K   */
	{ SST(0x5B, 0x01, SS_RDEF,
	    "Threshold condition met") },
	/* DTLPWROM   K   */
	{ SST(0x5B, 0x02, SS_RDEF,
	    "Log counter at maximum") },
	/* DTLPWROM   K   */
	{ SST(0x5B, 0x03, SS_RDEF,
	    "Log list codes exhausted") },
	/* D     O        */
	{ SST(0x5C, 0x00, SS_RDEF,
	    "RPL status change") },
	/* D     O        */
	{ SST(0x5C, 0x01, SS_NOP | SSQ_PRINT_SENSE,
	    "Spindles synchronized") },
	/* D     O        */
	{ SST(0x5C, 0x02, SS_RDEF,
	    "Spindles not synchronized") },
	/* DTLPWROMAEBKVF */
	{ SST(0x5D, 0x00, SS_RDEF,
	    "Failure prediction threshold exceeded") },
	/*      R    B    */
	{ SST(0x5D, 0x01, SS_RDEF,	/* XXX TBD */
	    "Media failure prediction threshold exceeded") },
	/*      R         */
	{ SST(0x5D, 0x02, SS_RDEF,	/* XXX TBD */
	    "Logical unit failure prediction threshold exceeded") },
	/*      R         */
	{ SST(0x5D, 0x03, SS_RDEF,	/* XXX TBD */
	    "Spare area exhaustion prediction threshold exceeded") },
	/* D         B    */
	{ SST(0x5D, 0x10, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure general hard drive failure") },
	/* D         B    */
	{ SST(0x5D, 0x11, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure drive error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x12, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure data error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x13, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure seek error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x14, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure too many block reassigns") },
	/* D         B    */
	{ SST(0x5D, 0x15, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure access times too high") },
	/* D         B    */
	{ SST(0x5D, 0x16, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure start unit times too high") },
	/* D         B    */
	{ SST(0x5D, 0x17, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure channel parametrics") },
	/* D         B    */
	{ SST(0x5D, 0x18, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure controller detected") },
	/* D         B    */
	{ SST(0x5D, 0x19, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure throughput performance") },
	/* D         B    */
	{ SST(0x5D, 0x1A, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure seek time performance") },
	/* D         B    */
	{ SST(0x5D, 0x1B, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure spin-up retry count") },
	/* D         B    */
	{ SST(0x5D, 0x1C, SS_RDEF,	/* XXX TBD */
	    "Hardware impending failure drive calibration retry count") },
	/* D         B    */
	{ SST(0x5D, 0x20, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure general hard drive failure") },
	/* D         B    */
	{ SST(0x5D, 0x21, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure drive error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x22, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure data error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x23, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure seek error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x24, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure too many block reassigns") },
	/* D         B    */
	{ SST(0x5D, 0x25, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure access times too high") },
	/* D         B    */
	{ SST(0x5D, 0x26, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure start unit times too high") },
	/* D         B    */
	{ SST(0x5D, 0x27, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure channel parametrics") },
	/* D         B    */
	{ SST(0x5D, 0x28, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure controller detected") },
	/* D         B    */
	{ SST(0x5D, 0x29, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure throughput performance") },
	/* D         B    */
	{ SST(0x5D, 0x2A, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure seek time performance") },
	/* D         B    */
	{ SST(0x5D, 0x2B, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure spin-up retry count") },
	/* D         B    */
	{ SST(0x5D, 0x2C, SS_RDEF,	/* XXX TBD */
	    "Controller impending failure drive calibration retry count") },
	/* D         B    */
	{ SST(0x5D, 0x30, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure general hard drive failure") },
	/* D         B    */
	{ SST(0x5D, 0x31, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure drive error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x32, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure data error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x33, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure seek error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x34, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure too many block reassigns") },
	/* D         B    */
	{ SST(0x5D, 0x35, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure access times too high") },
	/* D         B    */
	{ SST(0x5D, 0x36, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure start unit times too high") },
	/* D         B    */
	{ SST(0x5D, 0x37, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure channel parametrics") },
	/* D         B    */
	{ SST(0x5D, 0x38, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure controller detected") },
	/* D         B    */
	{ SST(0x5D, 0x39, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure throughput performance") },
	/* D         B    */
	{ SST(0x5D, 0x3A, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure seek time performance") },
	/* D         B    */
	{ SST(0x5D, 0x3B, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure spin-up retry count") },
	/* D         B    */
	{ SST(0x5D, 0x3C, SS_RDEF,	/* XXX TBD */
	    "Data channel impending failure drive calibration retry count") },
	/* D         B    */
	{ SST(0x5D, 0x40, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure general hard drive failure") },
	/* D         B    */
	{ SST(0x5D, 0x41, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure drive error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x42, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure data error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x43, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure seek error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x44, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure too many block reassigns") },
	/* D         B    */
	{ SST(0x5D, 0x45, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure access times too high") },
	/* D         B    */
	{ SST(0x5D, 0x46, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure start unit times too high") },
	/* D         B    */
	{ SST(0x5D, 0x47, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure channel parametrics") },
	/* D         B    */
	{ SST(0x5D, 0x48, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure controller detected") },
	/* D         B    */
	{ SST(0x5D, 0x49, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure throughput performance") },
	/* D         B    */
	{ SST(0x5D, 0x4A, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure seek time performance") },
	/* D         B    */
	{ SST(0x5D, 0x4B, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure spin-up retry count") },
	/* D         B    */
	{ SST(0x5D, 0x4C, SS_RDEF,	/* XXX TBD */
	    "Servo impending failure drive calibration retry count") },
	/* D         B    */
	{ SST(0x5D, 0x50, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure general hard drive failure") },
	/* D         B    */
	{ SST(0x5D, 0x51, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure drive error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x52, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure data error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x53, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure seek error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x54, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure too many block reassigns") },
	/* D         B    */
	{ SST(0x5D, 0x55, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure access times too high") },
	/* D         B    */
	{ SST(0x5D, 0x56, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure start unit times too high") },
	/* D         B    */
	{ SST(0x5D, 0x57, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure channel parametrics") },
	/* D         B    */
	{ SST(0x5D, 0x58, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure controller detected") },
	/* D         B    */
	{ SST(0x5D, 0x59, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure throughput performance") },
	/* D         B    */
	{ SST(0x5D, 0x5A, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure seek time performance") },
	/* D         B    */
	{ SST(0x5D, 0x5B, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure spin-up retry count") },
	/* D         B    */
	{ SST(0x5D, 0x5C, SS_RDEF,	/* XXX TBD */
	    "Spindle impending failure drive calibration retry count") },
	/* D         B    */
	{ SST(0x5D, 0x60, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure general hard drive failure") },
	/* D         B    */
	{ SST(0x5D, 0x61, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure drive error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x62, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure data error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x63, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure seek error rate too high") },
	/* D         B    */
	{ SST(0x5D, 0x64, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure too many block reassigns") },
	/* D         B    */
	{ SST(0x5D, 0x65, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure access times too high") },
	/* D         B    */
	{ SST(0x5D, 0x66, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure start unit times too high") },
	/* D         B    */
	{ SST(0x5D, 0x67, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure channel parametrics") },
	/* D         B    */
	{ SST(0x5D, 0x68, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure controller detected") },
	/* D         B    */
	{ SST(0x5D, 0x69, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure throughput performance") },
	/* D         B    */
	{ SST(0x5D, 0x6A, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure seek time performance") },
	/* D         B    */
	{ SST(0x5D, 0x6B, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure spin-up retry count") },
	/* D         B    */
	{ SST(0x5D, 0x6C, SS_RDEF,	/* XXX TBD */
	    "Firmware impending failure drive calibration retry count") },
	/* DTLPWROMAEBKVF */
	{ SST(0x5D, 0xFF, SS_RDEF,
	    "Failure prediction threshold exceeded (false)") },
	/* DTLPWRO A  K   */
	{ SST(0x5E, 0x00, SS_RDEF,
	    "Low power condition on") },
	/* DTLPWRO A  K   */
	{ SST(0x5E, 0x01, SS_RDEF,
	    "Idle condition activated by timer") },
	/* DTLPWRO A  K   */
	{ SST(0x5E, 0x02, SS_RDEF,
	    "Standby condition activated by timer") },
	/* DTLPWRO A  K   */
	{ SST(0x5E, 0x03, SS_RDEF,
	    "Idle condition activated by command") },
	/* DTLPWRO A  K   */
	{ SST(0x5E, 0x04, SS_RDEF,
	    "Standby condition activated by command") },
	/* DTLPWRO A  K   */
	{ SST(0x5E, 0x05, SS_RDEF,
	    "Idle-B condition activated by timer") },
	/* DTLPWRO A  K   */
	{ SST(0x5E, 0x06, SS_RDEF,
	    "Idle-B condition activated by command") },
	/* DTLPWRO A  K   */
	{ SST(0x5E, 0x07, SS_RDEF,
	    "Idle-C condition activated by timer") },
	/* DTLPWRO A  K   */
	{ SST(0x5E, 0x08, SS_RDEF,
	    "Idle-C condition activated by command") },
	/* DTLPWRO A  K   */
	{ SST(0x5E, 0x09, SS_RDEF,
	    "Standby-Y condition activated by timer") },
	/* DTLPWRO A  K   */
	{ SST(0x5E, 0x0A, SS_RDEF,
	    "Standby-Y condition activated by command") },
	/*           B    */
	{ SST(0x5E, 0x41, SS_RDEF,	/* XXX TBD */
	    "Power state change to active") },
	/*           B    */
	{ SST(0x5E, 0x42, SS_RDEF,	/* XXX TBD */
	    "Power state change to idle") },
	/*           B    */
	{ SST(0x5E, 0x43, SS_RDEF,	/* XXX TBD */
	    "Power state change to standby") },
	/*           B    */
	{ SST(0x5E, 0x45, SS_RDEF,	/* XXX TBD */
	    "Power state change to sleep") },
	/*           BK   */
	{ SST(0x5E, 0x47, SS_RDEF,	/* XXX TBD */
	    "Power state change to device control") },
	/*                */
	{ SST(0x60, 0x00, SS_RDEF,
	    "Lamp failure") },
	/*                */
	{ SST(0x61, 0x00, SS_RDEF,
	    "Video acquisition error") },
	/*                */
	{ SST(0x61, 0x01, SS_RDEF,
	    "Unable to acquire video") },
	/*                */
	{ SST(0x61, 0x02, SS_RDEF,
	    "Out of focus") },
	/*                */
	{ SST(0x62, 0x00, SS_RDEF,
	    "Scan head positioning error") },
	/*      R         */
	{ SST(0x63, 0x00, SS_RDEF,
	    "End of user area encountered on this track") },
	/*      R         */
	{ SST(0x63, 0x01, SS_FATAL | ENOSPC,
	    "Packet does not fit in available space") },
	/*      R         */
	{ SST(0x64, 0x00, SS_FATAL | ENXIO,
	    "Illegal mode for this track") },
	/*      R         */
	{ SST(0x64, 0x01, SS_RDEF,
	    "Invalid packet size") },
	/* DTLPWROMAEBKVF */
	{ SST(0x65, 0x00, SS_RDEF,
	    "Voltage fault") },
	/*                */
	{ SST(0x66, 0x00, SS_RDEF,
	    "Automatic document feeder cover up") },
	/*                */
	{ SST(0x66, 0x01, SS_RDEF,
	    "Automatic document feeder lift up") },
	/*                */
	{ SST(0x66, 0x02, SS_RDEF,
	    "Document jam in automatic document feeder") },
	/*                */
	{ SST(0x66, 0x03, SS_RDEF,
	    "Document miss feed automatic in document feeder") },
	/*         A      */
	{ SST(0x67, 0x00, SS_RDEF,
	    "Configuration failure") },
	/*         A      */
	{ SST(0x67, 0x01, SS_RDEF,
	    "Configuration of incapable logical units failed") },
	/*         A      */
	{ SST(0x67, 0x02, SS_RDEF,
	    "Add logical unit failed") },
	/*         A      */
	{ SST(0x67, 0x03, SS_RDEF,
	    "Modification of logical unit failed") },
	/*         A      */
	{ SST(0x67, 0x04, SS_RDEF,
	    "Exchange of logical unit failed") },
	/*         A      */
	{ SST(0x67, 0x05, SS_RDEF,
	    "Remove of logical unit failed") },
	/*         A      */
	{ SST(0x67, 0x06, SS_RDEF,
	    "Attachment of logical unit failed") },
	/*         A      */
	{ SST(0x67, 0x07, SS_RDEF,
	    "Creation of logical unit failed") },
	/*         A      */
	{ SST(0x67, 0x08, SS_RDEF,	/* XXX TBD */
	    "Assign failure occurred") },
	/*         A      */
	{ SST(0x67, 0x09, SS_RDEF,	/* XXX TBD */
	    "Multiply assigned logical unit") },
	/* DTLPWROMAEBKVF */
	{ SST(0x67, 0x0A, SS_RDEF,	/* XXX TBD */
	    "Set target port groups command failed") },
	/* DT        B    */
	{ SST(0x67, 0x0B, SS_RDEF,	/* XXX TBD */
	    "ATA device feature not enabled") },
	/*         A      */
	{ SST(0x68, 0x00, SS_RDEF,
	    "Logical unit not configured") },
	/*         A      */
	{ SST(0x69, 0x00, SS_RDEF,
	    "Data loss on logical unit") },
	/*         A      */
	{ SST(0x69, 0x01, SS_RDEF,
	    "Multiple logical unit failures") },
	/*         A      */
	{ SST(0x69, 0x02, SS_RDEF,
	    "Parity/data mismatch") },
	/*         A      */
	{ SST(0x6A, 0x00, SS_RDEF,
	    "Informational, refer to log") },
	/*         A      */
	{ SST(0x6B, 0x00, SS_RDEF,
	    "State change has occurred") },
	/*         A      */
	{ SST(0x6B, 0x01, SS_RDEF,
	    "Redundancy level got better") },
	/*         A      */
	{ SST(0x6B, 0x02, SS_RDEF,
	    "Redundancy level got worse") },
	/*         A      */
	{ SST(0x6C, 0x00, SS_RDEF,
	    "Rebuild failure occurred") },
	/*         A      */
	{ SST(0x6D, 0x00, SS_RDEF,
	    "Recalculate failure occurred") },
	/*         A      */
	{ SST(0x6E, 0x00, SS_RDEF,
	    "Command to logical unit failed") },
	/*      R         */
	{ SST(0x6F, 0x00, SS_RDEF,	/* XXX TBD */
	    "Copy protection key exchange failure - authentication failure") },
	/*      R         */
	{ SST(0x6F, 0x01, SS_RDEF,	/* XXX TBD */
	    "Copy protection key exchange failure - key not present") },
	/*      R         */
	{ SST(0x6F, 0x02, SS_RDEF,	/* XXX TBD */
	    "Copy protection key exchange failure - key not established") },
	/*      R         */
	{ SST(0x6F, 0x03, SS_RDEF,	/* XXX TBD */
	    "Read of scrambled sector without authentication") },
	/*      R         */
	{ SST(0x6F, 0x04, SS_RDEF,	/* XXX TBD */
	    "Media region code is mismatched to logical unit region") },
	/*      R         */
	{ SST(0x6F, 0x05, SS_RDEF,	/* XXX TBD */
	    "Drive region must be permanent/region reset count error") },
	/*      R         */
	{ SST(0x6F, 0x06, SS_RDEF,	/* XXX TBD */
	    "Insufficient block count for binding NONCE recording") },
	/*      R         */
	{ SST(0x6F, 0x07, SS_RDEF,	/* XXX TBD */
	    "Conflict in binding NONCE recording") },
	/*  T             */
	{ SST(0x70, 0x00, SS_RDEF,
	    "Decompression exception short: ASCQ = Algorithm ID") },
	/*  T             */
	{ SST(0x70, 0xFF, SS_RDEF | SSQ_RANGE,
	    NULL) },			/* Range 0x00 -> 0xFF */
	/*  T             */
	{ SST(0x71, 0x00, SS_RDEF,
	    "Decompression exception long: ASCQ = Algorithm ID") },
	/*  T             */
	{ SST(0x71, 0xFF, SS_RDEF | SSQ_RANGE,
	    NULL) },			/* Range 0x00 -> 0xFF */
	/*      R         */
	{ SST(0x72, 0x00, SS_RDEF,
	    "Session fixation error") },
	/*      R         */
	{ SST(0x72, 0x01, SS_RDEF,
	    "Session fixation error writing lead-in") },
	/*      R         */
	{ SST(0x72, 0x02, SS_RDEF,
	    "Session fixation error writing lead-out") },
	/*      R         */
	{ SST(0x72, 0x03, SS_RDEF,
	    "Session fixation error - incomplete track in session") },
	/*      R         */
	{ SST(0x72, 0x04, SS_RDEF,
	    "Empty or partially written reserved track") },
	/*      R         */
	{ SST(0x72, 0x05, SS_RDEF,	/* XXX TBD */
	    "No more track reservations allowed") },
	/*      R         */
	{ SST(0x72, 0x06, SS_RDEF,	/* XXX TBD */
	    "RMZ extension is not allowed") },
	/*      R         */
	{ SST(0x72, 0x07, SS_RDEF,	/* XXX TBD */
	    "No more test zone extensions are allowed") },
	/*      R         */
	{ SST(0x73, 0x00, SS_RDEF,
	    "CD control error") },
	/*      R         */
	{ SST(0x73, 0x01, SS_RDEF,
	    "Power calibration area almost full") },
	/*      R         */
	{ SST(0x73, 0x02, SS_FATAL | ENOSPC,
	    "Power calibration area is full") },
	/*      R         */
	{ SST(0x73, 0x03, SS_RDEF,
	    "Power calibration area error") },
	/*      R         */
	{ SST(0x73, 0x04, SS_RDEF,
	    "Program memory area update failure") },
	/*      R         */
	{ SST(0x73, 0x05, SS_RDEF,
	    "Program memory area is full") },
	/*      R         */
	{ SST(0x73, 0x06, SS_RDEF,	/* XXX TBD */
	    "RMA/PMA is almost full") },
	/*      R         */
	{ SST(0x73, 0x10, SS_RDEF,	/* XXX TBD */
	    "Current power calibration area almost full") },
	/*      R         */
	{ SST(0x73, 0x11, SS_RDEF,	/* XXX TBD */
	    "Current power calibration area is full") },
	/*      R         */
	{ SST(0x73, 0x17, SS_RDEF,	/* XXX TBD */
	    "RDZ is full") },
	/*  T             */
	{ SST(0x74, 0x00, SS_RDEF,	/* XXX TBD */
	    "Security error") },
	/*  T             */
	{ SST(0x74, 0x01, SS_RDEF,	/* XXX TBD */
	    "Unable to decrypt data") },
	/*  T             */
	{ SST(0x74, 0x02, SS_RDEF,	/* XXX TBD */
	    "Unencrypted data encountered while decrypting") },
	/*  T             */
	{ SST(0x74, 0x03, SS_RDEF,	/* XXX TBD */
	    "Incorrect data encryption key") },
	/*  T             */
	{ SST(0x74, 0x04, SS_RDEF,	/* XXX TBD */
	    "Cryptographic integrity validation failed") },
	/*  T             */
	{ SST(0x74, 0x05, SS_RDEF,	/* XXX TBD */
	    "Error decrypting data") },
	/*  T             */
	{ SST(0x74, 0x06, SS_RDEF,	/* XXX TBD */
	    "Unknown signature verification key") },
	/*  T             */
	{ SST(0x74, 0x07, SS_RDEF,	/* XXX TBD */
	    "Encryption parameters not useable") },
	/* DT   R M E  VF */
	{ SST(0x74, 0x08, SS_RDEF,	/* XXX TBD */
	    "Digital signature validation failure") },
	/*  T             */
	{ SST(0x74, 0x09, SS_RDEF,	/* XXX TBD */
	    "Encryption mode mismatch on read") },
	/*  T             */
	{ SST(0x74, 0x0A, SS_RDEF,	/* XXX TBD */
	    "Encrypted block not raw read enabled") },
	/*  T             */
	{ SST(0x74, 0x0B, SS_RDEF,	/* XXX TBD */
	    "Incorrect encryption parameters") },
	/* DT   R MAEBKV  */
	{ SST(0x74, 0x0C, SS_RDEF,	/* XXX TBD */
	    "Unable to decrypt parameter list") },
	/*  T             */
	{ SST(0x74, 0x0D, SS_RDEF,	/* XXX TBD */
	    "Encryption algorithm disabled") },
	/* DT   R MAEBKV  */
	{ SST(0x74, 0x10, SS_RDEF,	/* XXX TBD */
	    "SA creation parameter value invalid") },
	/* DT   R MAEBKV  */
	{ SST(0x74, 0x11, SS_RDEF,	/* XXX TBD */
	    "SA creation parameter value rejected") },
	/* DT   R MAEBKV  */
	{ SST(0x74, 0x12, SS_RDEF,	/* XXX TBD */
	    "Invalid SA usage") },
	/*  T             */
	{ SST(0x74, 0x21, SS_RDEF,	/* XXX TBD */
	    "Data encryption configuration prevented") },
	/* DT   R MAEBKV  */
	{ SST(0x74, 0x30, SS_RDEF,	/* XXX TBD */
	    "SA creation parameter not supported") },
	/* DT   R MAEBKV  */
	{ SST(0x74, 0x40, SS_RDEF,	/* XXX TBD */
	    "Authentication failed") },
	/*             V  */
	{ SST(0x74, 0x61, SS_RDEF,	/* XXX TBD */
	    "External data encryption key manager access error") },
	/*             V  */
	{ SST(0x74, 0x62, SS_RDEF,	/* XXX TBD */
	    "External data encryption key manager error") },
	/*             V  */
	{ SST(0x74, 0x63, SS_RDEF,	/* XXX TBD */
	    "External data encryption key not found") },
	/*             V  */
	{ SST(0x74, 0x64, SS_RDEF,	/* XXX TBD */
	    "External data encryption request not authorized") },
	/*  T             */
	{ SST(0x74, 0x6E, SS_RDEF,	/* XXX TBD */
	    "External data encryption control timeout") },
	/*  T             */
	{ SST(0x74, 0x6F, SS_RDEF,	/* XXX TBD */
	    "External data encryption control error") },
	/* DT   R M E  V  */
	{ SST(0x74, 0x71, SS_RDEF,	/* XXX TBD */
	    "Logical unit access not authorized") },
	/* D              */
	{ SST(0x74, 0x79, SS_RDEF,	/* XXX TBD */
	    "Security conflict in translated device") }
};

const int asc_table_size = sizeof(asc_table)/sizeof(asc_table[0]);

struct asc_key
{
	int asc;
	int ascq;
};

static int
ascentrycomp(const void *key, const void *member)
{
	int asc;
	int ascq;
	const struct asc_table_entry *table_entry;

	asc = ((const struct asc_key *)key)->asc;
	ascq = ((const struct asc_key *)key)->ascq;
	table_entry = (const struct asc_table_entry *)member;

	if (asc >= table_entry->asc) {

		if (asc > table_entry->asc)
			return (1);

		if (ascq <= table_entry->ascq) {
			/* Check for ranges */
			if (ascq == table_entry->ascq
		 	 || ((table_entry->action & SSQ_RANGE) != 0
		  	   && ascq >= (table_entry - 1)->ascq))
				return (0);
			return (-1);
		}
		return (1);
	}
	return (-1);
}

static int
senseentrycomp(const void *key, const void *member)
{
	int sense_key;
	const struct sense_key_table_entry *table_entry;

	sense_key = *((const int *)key);
	table_entry = (const struct sense_key_table_entry *)member;

	if (sense_key >= table_entry->sense_key) {
		if (sense_key == table_entry->sense_key)
			return (0);
		return (1);
	}
	return (-1);
}

static void
fetchtableentries(int sense_key, int asc, int ascq,
		  struct scsi_inquiry_data *inq_data,
		  const struct sense_key_table_entry **sense_entry,
		  const struct asc_table_entry **asc_entry)
{
	caddr_t match;
	const struct asc_table_entry *asc_tables[2];
	const struct sense_key_table_entry *sense_tables[2];
	struct asc_key asc_ascq;
	size_t asc_tables_size[2];
	size_t sense_tables_size[2];
	int num_asc_tables;
	int num_sense_tables;
	int i;

	/* Default to failure */
	*sense_entry = NULL;
	*asc_entry = NULL;
	match = NULL;
	if (inq_data != NULL)
		match = cam_quirkmatch((caddr_t)inq_data,
				       (caddr_t)sense_quirk_table,
				       sense_quirk_table_size,
				       sizeof(*sense_quirk_table),
				       scsi_inquiry_match);

	if (match != NULL) {
		struct scsi_sense_quirk_entry *quirk;

		quirk = (struct scsi_sense_quirk_entry *)match;
		asc_tables[0] = quirk->asc_info;
		asc_tables_size[0] = quirk->num_ascs;
		asc_tables[1] = asc_table;
		asc_tables_size[1] = asc_table_size;
		num_asc_tables = 2;
		sense_tables[0] = quirk->sense_key_info;
		sense_tables_size[0] = quirk->num_sense_keys;
		sense_tables[1] = sense_key_table;
		sense_tables_size[1] = sense_key_table_size;
		num_sense_tables = 2;
	} else {
		asc_tables[0] = asc_table;
		asc_tables_size[0] = asc_table_size;
		num_asc_tables = 1;
		sense_tables[0] = sense_key_table;
		sense_tables_size[0] = sense_key_table_size;
		num_sense_tables = 1;
	}

	asc_ascq.asc = asc;
	asc_ascq.ascq = ascq;
	for (i = 0; i < num_asc_tables; i++) {
		void *found_entry;

		found_entry = bsearch(&asc_ascq, asc_tables[i],
				      asc_tables_size[i],
				      sizeof(**asc_tables),
				      ascentrycomp);

		if (found_entry) {
			*asc_entry = (struct asc_table_entry *)found_entry;
			break;
		}
	}

	for (i = 0; i < num_sense_tables; i++) {
		void *found_entry;

		found_entry = bsearch(&sense_key, sense_tables[i],
				      sense_tables_size[i],
				      sizeof(**sense_tables),
				      senseentrycomp);

		if (found_entry) {
			*sense_entry =
			    (struct sense_key_table_entry *)found_entry;
			break;
		}
	}
}

void
scsi_sense_desc(int sense_key, int asc, int ascq,
		struct scsi_inquiry_data *inq_data,
		const char **sense_key_desc, const char **asc_desc)
{
	const struct asc_table_entry *asc_entry;
	const struct sense_key_table_entry *sense_entry;

	fetchtableentries(sense_key, asc, ascq,
			  inq_data,
			  &sense_entry,
			  &asc_entry);

	if (sense_entry != NULL)
		*sense_key_desc = sense_entry->desc;
	else
		*sense_key_desc = "Invalid Sense Key";

	if (asc_entry != NULL)
		*asc_desc = asc_entry->desc;
	else if (asc >= 0x80 && asc <= 0xff)
		*asc_desc = "Vendor Specific ASC";
	else if (ascq >= 0x80 && ascq <= 0xff)
		*asc_desc = "Vendor Specific ASCQ";
	else
		*asc_desc = "Reserved ASC/ASCQ pair";
}

/*
 * Given sense and device type information, return the appropriate action.
 * If we do not understand the specific error as identified by the ASC/ASCQ
 * pair, fall back on the more generic actions derived from the sense key.
 */
scsi_sense_action
scsi_error_action(struct ccb_scsiio *csio, struct scsi_inquiry_data *inq_data,
		  u_int32_t sense_flags)
{
	const struct asc_table_entry *asc_entry;
	const struct sense_key_table_entry *sense_entry;
	int error_code, sense_key, asc, ascq;
	scsi_sense_action action;

	if (!scsi_extract_sense_ccb((union ccb *)csio,
	    &error_code, &sense_key, &asc, &ascq)) {
		action = SS_RETRY | SSQ_DECREMENT_COUNT | SSQ_PRINT_SENSE | EIO;
	} else if ((error_code == SSD_DEFERRED_ERROR)
	 || (error_code == SSD_DESC_DEFERRED_ERROR)) {
		/*
		 * XXX dufault@FreeBSD.org
		 * This error doesn't relate to the command associated
		 * with this request sense.  A deferred error is an error
		 * for a command that has already returned GOOD status
		 * (see SCSI2 8.2.14.2).
		 *
		 * By my reading of that section, it looks like the current
		 * command has been cancelled, we should now clean things up
		 * (hopefully recovering any lost data) and then retry the
		 * current command.  There are two easy choices, both wrong:
		 *
		 * 1. Drop through (like we had been doing), thus treating
		 *    this as if the error were for the current command and
		 *    return and stop the current command.
		 * 
		 * 2. Issue a retry (like I made it do) thus hopefully
		 *    recovering the current transfer, and ignoring the
		 *    fact that we've dropped a command.
		 *
		 * These should probably be handled in a device specific
		 * sense handler or punted back up to a user mode daemon
		 */
		action = SS_RETRY|SSQ_DECREMENT_COUNT|SSQ_PRINT_SENSE;
	} else {
		fetchtableentries(sense_key, asc, ascq,
				  inq_data,
				  &sense_entry,
				  &asc_entry);

		/*
		 * Override the 'No additional Sense' entry (0,0)
		 * with the error action of the sense key.
		 */
		if (asc_entry != NULL
		 && (asc != 0 || ascq != 0))
			action = asc_entry->action;
		else if (sense_entry != NULL)
			action = sense_entry->action;
		else
			action = SS_RETRY|SSQ_DECREMENT_COUNT|SSQ_PRINT_SENSE; 

		if (sense_key == SSD_KEY_RECOVERED_ERROR) {
			/*
			 * The action succeeded but the device wants
			 * the user to know that some recovery action
			 * was required.
			 */
			action &= ~(SS_MASK|SSQ_MASK|SS_ERRMASK);
			action |= SS_NOP|SSQ_PRINT_SENSE;
		} else if (sense_key == SSD_KEY_ILLEGAL_REQUEST) {
			if ((sense_flags & SF_QUIET_IR) != 0)
				action &= ~SSQ_PRINT_SENSE;
		} else if (sense_key == SSD_KEY_UNIT_ATTENTION) {
			if ((sense_flags & SF_RETRY_UA) != 0
			 && (action & SS_MASK) == SS_FAIL) {
				action &= ~(SS_MASK|SSQ_MASK);
				action |= SS_RETRY|SSQ_DECREMENT_COUNT|
					  SSQ_PRINT_SENSE;
			}
			action |= SSQ_UA;
		}
	}
	if ((action & SS_MASK) >= SS_START &&
	    (sense_flags & SF_NO_RECOVERY)) {
		action &= ~SS_MASK;
		action |= SS_FAIL;
	} else if ((action & SS_MASK) == SS_RETRY &&
	    (sense_flags & SF_NO_RETRY)) {
		action &= ~SS_MASK;
		action |= SS_FAIL;
	}
	if ((sense_flags & SF_PRINT_ALWAYS) != 0)
		action |= SSQ_PRINT_SENSE;
	else if ((sense_flags & SF_NO_PRINT) != 0)
		action &= ~SSQ_PRINT_SENSE;

	return (action);
}

char *
scsi_cdb_string(u_int8_t *cdb_ptr, char *cdb_string, size_t len)
{
	u_int8_t cdb_len;
	int i;

	if (cdb_ptr == NULL)
		return("");

	/* Silence warnings */
	cdb_len = 0;

	/*
	 * This is taken from the SCSI-3 draft spec.
	 * (T10/1157D revision 0.3)
	 * The top 3 bits of an opcode are the group code.  The next 5 bits
	 * are the command code.
	 * Group 0:  six byte commands
	 * Group 1:  ten byte commands
	 * Group 2:  ten byte commands
	 * Group 3:  reserved
	 * Group 4:  sixteen byte commands
	 * Group 5:  twelve byte commands
	 * Group 6:  vendor specific
	 * Group 7:  vendor specific
	 */
	switch((*cdb_ptr >> 5) & 0x7) {
		case 0:
			cdb_len = 6;
			break;
		case 1:
		case 2:
			cdb_len = 10;
			break;
		case 3:
		case 6:
		case 7:
			/* in this case, just print out the opcode */
			cdb_len = 1;
			break;
		case 4:
			cdb_len = 16;
			break;
		case 5:
			cdb_len = 12;
			break;
	}
	*cdb_string = '\0';
	for (i = 0; i < cdb_len; i++)
		snprintf(cdb_string + strlen(cdb_string),
			 len - strlen(cdb_string), "%02hhx ", cdb_ptr[i]);

	return(cdb_string);
}

const char *
scsi_status_string(struct ccb_scsiio *csio)
{
	switch(csio->scsi_status) {
	case SCSI_STATUS_OK:
		return("OK");
	case SCSI_STATUS_CHECK_COND:
		return("Check Condition");
	case SCSI_STATUS_BUSY:
		return("Busy");
	case SCSI_STATUS_INTERMED:
		return("Intermediate");
	case SCSI_STATUS_INTERMED_COND_MET:
		return("Intermediate-Condition Met");
	case SCSI_STATUS_RESERV_CONFLICT:
		return("Reservation Conflict");
	case SCSI_STATUS_CMD_TERMINATED:
		return("Command Terminated");
	case SCSI_STATUS_QUEUE_FULL:
		return("Queue Full");
	case SCSI_STATUS_ACA_ACTIVE:
		return("ACA Active");
	case SCSI_STATUS_TASK_ABORTED:
		return("Task Aborted");
	default: {
		static char unkstr[64];
		snprintf(unkstr, sizeof(unkstr), "Unknown %#x",
			 csio->scsi_status);
		return(unkstr);
	}
	}
}

/*
 * scsi_command_string() returns 0 for success and -1 for failure.
 */
#ifdef _KERNEL
int
scsi_command_string(struct ccb_scsiio *csio, struct sbuf *sb)
#else /* !_KERNEL */
int
scsi_command_string(struct cam_device *device, struct ccb_scsiio *csio, 
		    struct sbuf *sb)
#endif /* _KERNEL/!_KERNEL */
{
	struct scsi_inquiry_data *inq_data;
	char cdb_str[(SCSI_MAX_CDBLEN * 3) + 1];
#ifdef _KERNEL
	struct	  ccb_getdev *cgd;
#endif /* _KERNEL */

#ifdef _KERNEL
	if ((cgd = (struct ccb_getdev*)xpt_alloc_ccb_nowait()) == NULL)
		return(-1);
	/*
	 * Get the device information.
	 */
	xpt_setup_ccb(&cgd->ccb_h,
		      csio->ccb_h.path,
		      CAM_PRIORITY_NORMAL);
	cgd->ccb_h.func_code = XPT_GDEV_TYPE;
	xpt_action((union ccb *)cgd);

	/*
	 * If the device is unconfigured, just pretend that it is a hard
	 * drive.  scsi_op_desc() needs this.
	 */
	if (cgd->ccb_h.status == CAM_DEV_NOT_THERE)
		cgd->inq_data.device = T_DIRECT;

	inq_data = &cgd->inq_data;

#else /* !_KERNEL */

	inq_data = &device->inq_data;

#endif /* _KERNEL/!_KERNEL */

	if ((csio->ccb_h.flags & CAM_CDB_POINTER) != 0) {
		sbuf_printf(sb, "%s. CDB: %s", 
			    scsi_op_desc(csio->cdb_io.cdb_ptr[0], inq_data),
			    scsi_cdb_string(csio->cdb_io.cdb_ptr, cdb_str,
					    sizeof(cdb_str)));
	} else {
		sbuf_printf(sb, "%s. CDB: %s",
			    scsi_op_desc(csio->cdb_io.cdb_bytes[0], inq_data),
			    scsi_cdb_string(csio->cdb_io.cdb_bytes, cdb_str,
					    sizeof(cdb_str)));
	}

#ifdef _KERNEL
	xpt_free_ccb((union ccb *)cgd);
#endif

	return(0);
}

/*
 * Iterate over sense descriptors.  Each descriptor is passed into iter_func(). 
 * If iter_func() returns 0, list traversal continues.  If iter_func()
 * returns non-zero, list traversal is stopped.
 */
void
scsi_desc_iterate(struct scsi_sense_data_desc *sense, u_int sense_len,
		  int (*iter_func)(struct scsi_sense_data_desc *sense,
				   u_int, struct scsi_sense_desc_header *,
				   void *), void *arg)
{
	int cur_pos;
	int desc_len;

	/*
	 * First make sure the extra length field is present.
	 */
	if (SSD_DESC_IS_PRESENT(sense, sense_len, extra_len) == 0)
		return;

	/*
	 * The length of data actually returned may be different than the
	 * extra_len recorded in the sturcture.
	 */
	desc_len = sense_len -offsetof(struct scsi_sense_data_desc, sense_desc);

	/*
	 * Limit this further by the extra length reported, and the maximum
	 * allowed extra length.
	 */
	desc_len = MIN(desc_len, MIN(sense->extra_len, SSD_EXTRA_MAX));

	/*
	 * Subtract the size of the header from the descriptor length.
	 * This is to ensure that we have at least the header left, so we
	 * don't have to check that inside the loop.  This can wind up
	 * being a negative value.
	 */
	desc_len -= sizeof(struct scsi_sense_desc_header);

	for (cur_pos = 0; cur_pos < desc_len;) {
		struct scsi_sense_desc_header *header;

		header = (struct scsi_sense_desc_header *)
			&sense->sense_desc[cur_pos];

		/*
		 * Check to make sure we have the entire descriptor.  We
		 * don't call iter_func() unless we do.
		 *
		 * Note that although cur_pos is at the beginning of the
		 * descriptor, desc_len already has the header length
		 * subtracted.  So the comparison of the length in the
		 * header (which does not include the header itself) to
		 * desc_len - cur_pos is correct.
		 */
		if (header->length > (desc_len - cur_pos)) 
			break;

		if (iter_func(sense, sense_len, header, arg) != 0)
			break;

		cur_pos += sizeof(*header) + header->length;
	}
}

struct scsi_find_desc_info {
	uint8_t desc_type;
	struct scsi_sense_desc_header *header;
};

static int
scsi_find_desc_func(struct scsi_sense_data_desc *sense, u_int sense_len,
		    struct scsi_sense_desc_header *header, void *arg)
{
	struct scsi_find_desc_info *desc_info;

	desc_info = (struct scsi_find_desc_info *)arg;

	if (header->desc_type == desc_info->desc_type) {
		desc_info->header = header;

		/* We found the descriptor, tell the iterator to stop. */
		return (1);
	} else
		return (0);
}

/*
 * Given a descriptor type, return a pointer to it if it is in the sense
 * data and not truncated.  Avoiding truncating sense data will simplify
 * things significantly for the caller.
 */
uint8_t *
scsi_find_desc(struct scsi_sense_data_desc *sense, u_int sense_len,
	       uint8_t desc_type)
{
	struct scsi_find_desc_info desc_info;

	desc_info.desc_type = desc_type;
	desc_info.header = NULL;

	scsi_desc_iterate(sense, sense_len, scsi_find_desc_func, &desc_info);

	return ((uint8_t *)desc_info.header);
}

/*
 * Fill in SCSI sense data with the specified parameters.  This routine can
 * fill in either fixed or descriptor type sense data.
 */
void
scsi_set_sense_data_va(struct scsi_sense_data *sense_data,
		      scsi_sense_data_type sense_format, int current_error,
		      int sense_key, int asc, int ascq, va_list ap) 
{
	int descriptor_sense;
	scsi_sense_elem_type elem_type;

	/*
	 * Determine whether to return fixed or descriptor format sense
	 * data.  If the user specifies SSD_TYPE_NONE for some reason,
	 * they'll just get fixed sense data.
	 */
	if (sense_format == SSD_TYPE_DESC)
		descriptor_sense = 1;
	else
		descriptor_sense = 0;

	/*
	 * Zero the sense data, so that we don't pass back any garbage data
	 * to the user.
	 */
	memset(sense_data, 0, sizeof(*sense_data));

	if (descriptor_sense != 0) {
		struct scsi_sense_data_desc *sense;

		sense = (struct scsi_sense_data_desc *)sense_data;
		/*
		 * The descriptor sense format eliminates the use of the
		 * valid bit.
		 */
		if (current_error != 0)
			sense->error_code = SSD_DESC_CURRENT_ERROR;
		else
			sense->error_code = SSD_DESC_DEFERRED_ERROR;
		sense->sense_key = sense_key;
		sense->add_sense_code = asc;
		sense->add_sense_code_qual = ascq;
		/*
		 * Start off with no extra length, since the above data
		 * fits in the standard descriptor sense information.
		 */
		sense->extra_len = 0;
		while ((elem_type = (scsi_sense_elem_type)va_arg(ap,
			scsi_sense_elem_type)) != SSD_ELEM_NONE) {
			int sense_len, len_to_copy;
			uint8_t *data;

			if (elem_type >= SSD_ELEM_MAX) {
				printf("%s: invalid sense type %d\n", __func__,
				       elem_type);
				break;
			}

			sense_len = (int)va_arg(ap, int);
			len_to_copy = MIN(sense_len, SSD_EXTRA_MAX -
					  sense->extra_len);
			data = (uint8_t *)va_arg(ap, uint8_t *);

			/*
			 * We've already consumed the arguments for this one.
			 */
			if (elem_type == SSD_ELEM_SKIP)
				continue;

			switch (elem_type) {
			case SSD_ELEM_DESC: {

				/*
				 * This is a straight descriptor.  All we
				 * need to do is copy the data in.
				 */
				bcopy(data, &sense->sense_desc[
				      sense->extra_len], len_to_copy);
				sense->extra_len += len_to_copy;
				break;
			}
			case SSD_ELEM_SKS: {
				struct scsi_sense_sks sks;

				bzero(&sks, sizeof(sks));

				/*
				 * This is already-formatted sense key
				 * specific data.  We just need to fill out
				 * the header and copy everything in.
				 */
				bcopy(data, &sks.sense_key_spec,
				      MIN(len_to_copy,
				          sizeof(sks.sense_key_spec)));

				sks.desc_type = SSD_DESC_SKS;
				sks.length = sizeof(sks) -
				    offsetof(struct scsi_sense_sks, reserved1);
				bcopy(&sks,&sense->sense_desc[sense->extra_len],
				      sizeof(sks));
				sense->extra_len += sizeof(sks);
				break;
			}
			case SSD_ELEM_INFO:
			case SSD_ELEM_COMMAND: {
				struct scsi_sense_command cmd;
				struct scsi_sense_info info;
				uint8_t *data_dest;
				uint8_t *descriptor;
				int descriptor_size, i, copy_len;

				bzero(&cmd, sizeof(cmd));
				bzero(&info, sizeof(info));

				/*
				 * Command or information data.  The
				 * operate in pretty much the same way.
				 */
				if (elem_type == SSD_ELEM_COMMAND) {
					len_to_copy = MIN(len_to_copy,
					    sizeof(cmd.command_info));
					descriptor = (uint8_t *)&cmd;
					descriptor_size  = sizeof(cmd);
					data_dest =(uint8_t *)&cmd.command_info;
					cmd.desc_type = SSD_DESC_COMMAND;
					cmd.length = sizeof(cmd) -
					    offsetof(struct scsi_sense_command,
						     reserved);
				} else {
					len_to_copy = MIN(len_to_copy,
					    sizeof(info.info));
					descriptor = (uint8_t *)&info;
					descriptor_size = sizeof(cmd);
					data_dest = (uint8_t *)&info.info;
					info.desc_type = SSD_DESC_INFO;
					info.byte2 = SSD_INFO_VALID;
					info.length = sizeof(info) -
					    offsetof(struct scsi_sense_info,
						     byte2);
				}

				/*
				 * Copy this in reverse because the spec
				 * (SPC-4) says that when 4 byte quantities
				 * are stored in this 8 byte field, the
				 * first four bytes shall be 0.
				 *
				 * So we fill the bytes in from the end, and
				 * if we have less than 8 bytes to copy,
				 * the initial, most significant bytes will
				 * be 0.
				 */
				for (i = sense_len - 1; i >= 0 &&
				     len_to_copy > 0; i--, len_to_copy--)
					data_dest[len_to_copy - 1] = data[i];

				/*
				 * This calculation looks much like the
				 * initial len_to_copy calculation, but
				 * we have to do it again here, because
				 * we're looking at a larger amount that
				 * may or may not fit.  It's not only the
				 * data the user passed in, but also the
				 * rest of the descriptor.
				 */
				copy_len = MIN(descriptor_size,
				    SSD_EXTRA_MAX - sense->extra_len);
				bcopy(descriptor, &sense->sense_desc[
				      sense->extra_len], copy_len);
				sense->extra_len += copy_len;
				break;
			}
			case SSD_ELEM_FRU: {
				struct scsi_sense_fru fru;
				int copy_len;

				bzero(&fru, sizeof(fru));

				fru.desc_type = SSD_DESC_FRU;
				fru.length = sizeof(fru) -
				    offsetof(struct scsi_sense_fru, reserved);
				fru.fru = *data;

				copy_len = MIN(sizeof(fru), SSD_EXTRA_MAX -
					       sense->extra_len);
				bcopy(&fru, &sense->sense_desc[
				      sense->extra_len], copy_len);
				sense->extra_len += copy_len;
				break;
			}
			case SSD_ELEM_STREAM: {
				struct scsi_sense_stream stream_sense;
				int copy_len;

				bzero(&stream_sense, sizeof(stream_sense));
				stream_sense.desc_type = SSD_DESC_STREAM;
				stream_sense.length = sizeof(stream_sense) -
				   offsetof(struct scsi_sense_stream, reserved);
				stream_sense.byte3 = *data;

				copy_len = MIN(sizeof(stream_sense),
				    SSD_EXTRA_MAX - sense->extra_len);
				bcopy(&stream_sense, &sense->sense_desc[
				      sense->extra_len], copy_len);
				sense->extra_len += copy_len;
				break;
			}
			default:
				/*
				 * We shouldn't get here, but if we do, do
				 * nothing.  We've already consumed the
				 * arguments above.
				 */
				break;
			}
		}
	} else {
		struct scsi_sense_data_fixed *sense;

		sense = (struct scsi_sense_data_fixed *)sense_data;

		if (current_error != 0)
			sense->error_code = SSD_CURRENT_ERROR;
		else
			sense->error_code = SSD_DEFERRED_ERROR;

		sense->flags = sense_key;
		sense->add_sense_code = asc;
		sense->add_sense_code_qual = ascq;
		/*
		 * We've set the ASC and ASCQ, so we have 6 more bytes of
		 * valid data.  If we wind up setting any of the other
		 * fields, we'll bump this to 10 extra bytes.
		 */
		sense->extra_len = 6;

		while ((elem_type = (scsi_sense_elem_type)va_arg(ap,
			scsi_sense_elem_type)) != SSD_ELEM_NONE) {
			int sense_len, len_to_copy;
			uint8_t *data;

			if (elem_type >= SSD_ELEM_MAX) {
				printf("%s: invalid sense type %d\n", __func__,
				       elem_type);
				break;
			}
			/*
			 * If we get in here, just bump the extra length to
			 * 10 bytes.  That will encompass anything we're
			 * going to set here.
			 */
			sense->extra_len = 10;
			sense_len = (int)va_arg(ap, int);
			len_to_copy = MIN(sense_len, SSD_EXTRA_MAX -
					  sense->extra_len);
			data = (uint8_t *)va_arg(ap, uint8_t *);

			switch (elem_type) {
			case SSD_ELEM_SKS:
				/*
				 * The user passed in pre-formatted sense
				 * key specific data.
				 */
				bcopy(data, &sense->sense_key_spec[0],
				      MIN(sizeof(sense->sense_key_spec),
				      sense_len));
				break;
			case SSD_ELEM_INFO:
			case SSD_ELEM_COMMAND: {
				uint8_t *data_dest;
				int i;

				if (elem_type == SSD_ELEM_COMMAND)
					data_dest = &sense->cmd_spec_info[0];
				else {
					data_dest = &sense->info[0];
					/*
					 * We're setting the info field, so
					 * set the valid bit.
					 */
					sense->error_code |= SSD_ERRCODE_VALID;
				}

				/*
			 	 * Copy this in reverse so that if we have
				 * less than 4 bytes to fill, the least
				 * significant bytes will be at the end.
				 * If we have more than 4 bytes, only the
				 * least significant bytes will be included.
				 */
				for (i = sense_len - 1; i >= 0 &&
				     len_to_copy > 0; i--, len_to_copy--)
					data_dest[len_to_copy - 1] = data[i];

				break;
			}
			case SSD_ELEM_FRU:
				sense->fru = *data;
				break;
			case SSD_ELEM_STREAM:
				sense->flags |= *data;
				break;
			case SSD_ELEM_DESC:
			default:

				/*
				 * If the user passes in descriptor sense,
				 * we can't handle that in fixed format.
				 * So just skip it, and any unknown argument
				 * types.
				 */
				break;
			}
		}
	}
}

void
scsi_set_sense_data(struct scsi_sense_data *sense_data, 
		    scsi_sense_data_type sense_format, int current_error,
		    int sense_key, int asc, int ascq, ...) 
{
	va_list ap;

	va_start(ap, ascq);
	scsi_set_sense_data_va(sense_data, sense_format, current_error,
			       sense_key, asc, ascq, ap);
	va_end(ap);
}

/*
 * Get sense information for three similar sense data types.
 */
int
scsi_get_sense_info(struct scsi_sense_data *sense_data, u_int sense_len,
		    uint8_t info_type, uint64_t *info, int64_t *signed_info)
{
	scsi_sense_data_type sense_type;

	if (sense_len == 0)
		goto bailout;

	sense_type = scsi_sense_type(sense_data);

	switch (sense_type) {
	case SSD_TYPE_DESC: {
		struct scsi_sense_data_desc *sense;
		uint8_t *desc;

		sense = (struct scsi_sense_data_desc *)sense_data;

		desc = scsi_find_desc(sense, sense_len, info_type);
		if (desc == NULL)
			goto bailout;

		switch (info_type) {
		case SSD_DESC_INFO: {
			struct scsi_sense_info *info_desc;

			info_desc = (struct scsi_sense_info *)desc;
			*info = scsi_8btou64(info_desc->info);
			if (signed_info != NULL)
				*signed_info = *info;
			break;
		}
		case SSD_DESC_COMMAND: {
			struct scsi_sense_command *cmd_desc;

			cmd_desc = (struct scsi_sense_command *)desc;

			*info = scsi_8btou64(cmd_desc->command_info);
			if (signed_info != NULL)
				*signed_info = *info;
			break;
		}
		case SSD_DESC_FRU: {
			struct scsi_sense_fru *fru_desc;

			fru_desc = (struct scsi_sense_fru *)desc;

			*info = fru_desc->fru;
			if (signed_info != NULL)
				*signed_info = (int8_t)fru_desc->fru;
			break;
		}
		default:
			goto bailout;
			break;
		}
		break;
	}
	case SSD_TYPE_FIXED: {
		struct scsi_sense_data_fixed *sense;

		sense = (struct scsi_sense_data_fixed *)sense_data;

		switch (info_type) {
		case SSD_DESC_INFO: {
			uint32_t info_val;

			if ((sense->error_code & SSD_ERRCODE_VALID) == 0)
				goto bailout;

			if (SSD_FIXED_IS_PRESENT(sense, sense_len, info) == 0)
				goto bailout;

			info_val = scsi_4btoul(sense->info);

			*info = info_val;
			if (signed_info != NULL)
				*signed_info = (int32_t)info_val;
			break;
		}
		case SSD_DESC_COMMAND: {
			uint32_t cmd_val;

			if ((SSD_FIXED_IS_PRESENT(sense, sense_len,
			     cmd_spec_info) == 0)
			 || (SSD_FIXED_IS_FILLED(sense, cmd_spec_info) == 0)) 
				goto bailout;

			cmd_val = scsi_4btoul(sense->cmd_spec_info);
			if (cmd_val == 0)
				goto bailout;

			*info = cmd_val;
			if (signed_info != NULL)
				*signed_info = (int32_t)cmd_val;
			break;
		}
		case SSD_DESC_FRU:
			if ((SSD_FIXED_IS_PRESENT(sense, sense_len, fru) == 0)
			 || (SSD_FIXED_IS_FILLED(sense, fru) == 0))
				goto bailout;

			if (sense->fru == 0)
				goto bailout;

			*info = sense->fru;
			if (signed_info != NULL)
				*signed_info = (int8_t)sense->fru;
			break;
		default:
			goto bailout;
			break;
		}
		break;
	}
	default: 
		goto bailout;
		break;
	}

	return (0);
bailout:
	return (1);
}

int
scsi_get_sks(struct scsi_sense_data *sense_data, u_int sense_len, uint8_t *sks)
{
	scsi_sense_data_type sense_type;

	if (sense_len == 0)
		goto bailout;

	sense_type = scsi_sense_type(sense_data);

	switch (sense_type) {
	case SSD_TYPE_DESC: {
		struct scsi_sense_data_desc *sense;
		struct scsi_sense_sks *desc;

		sense = (struct scsi_sense_data_desc *)sense_data;

		desc = (struct scsi_sense_sks *)scsi_find_desc(sense, sense_len,
							       SSD_DESC_SKS);
		if (desc == NULL)
			goto bailout;

		/*
		 * No need to check the SKS valid bit for descriptor sense.
		 * If the descriptor is present, it is valid.
		 */
		bcopy(desc->sense_key_spec, sks, sizeof(desc->sense_key_spec));
		break;
	}
	case SSD_TYPE_FIXED: {
		struct scsi_sense_data_fixed *sense;

		sense = (struct scsi_sense_data_fixed *)sense_data;

		if ((SSD_FIXED_IS_PRESENT(sense, sense_len, sense_key_spec)== 0)
		 || (SSD_FIXED_IS_FILLED(sense, sense_key_spec) == 0))
			goto bailout;

		if ((sense->sense_key_spec[0] & SSD_SCS_VALID) == 0)
			goto bailout;

		bcopy(sense->sense_key_spec, sks,sizeof(sense->sense_key_spec));
		break;
	}
	default:
		goto bailout;
		break;
	}
	return (0);
bailout:
	return (1);
}

/*
 * Provide a common interface for fixed and descriptor sense to detect
 * whether we have block-specific sense information.  It is clear by the
 * presence of the block descriptor in descriptor mode, but we have to
 * infer from the inquiry data and ILI bit in fixed mode.
 */
int
scsi_get_block_info(struct scsi_sense_data *sense_data, u_int sense_len,
		    struct scsi_inquiry_data *inq_data, uint8_t *block_bits)
{
	scsi_sense_data_type sense_type;

	if (inq_data != NULL) {
		switch (SID_TYPE(inq_data)) {
		case T_DIRECT:
		case T_RBC:
			break;
		default:
			goto bailout;
			break;
		}
	}

	sense_type = scsi_sense_type(sense_data);

	switch (sense_type) {
	case SSD_TYPE_DESC: {
		struct scsi_sense_data_desc *sense;
		struct scsi_sense_block *block;

		sense = (struct scsi_sense_data_desc *)sense_data;

		block = (struct scsi_sense_block *)scsi_find_desc(sense,
		    sense_len, SSD_DESC_BLOCK);
		if (block == NULL)
			goto bailout;

		*block_bits = block->byte3;
		break;
	}
	case SSD_TYPE_FIXED: {
		struct scsi_sense_data_fixed *sense;

		sense = (struct scsi_sense_data_fixed *)sense_data;

		if (SSD_FIXED_IS_PRESENT(sense, sense_len, flags) == 0)
			goto bailout;

		if ((sense->flags & SSD_ILI) == 0)
			goto bailout;

		*block_bits = sense->flags & SSD_ILI;
		break;
	}
	default:
		goto bailout;
		break;
	}
	return (0);
bailout:
	return (1);
}

int
scsi_get_stream_info(struct scsi_sense_data *sense_data, u_int sense_len,
		     struct scsi_inquiry_data *inq_data, uint8_t *stream_bits)
{
	scsi_sense_data_type sense_type;

	if (inq_data != NULL) {
		switch (SID_TYPE(inq_data)) {
		case T_SEQUENTIAL:
			break;
		default:
			goto bailout;
			break;
		}
	}

	sense_type = scsi_sense_type(sense_data);

	switch (sense_type) {
	case SSD_TYPE_DESC: {
		struct scsi_sense_data_desc *sense;
		struct scsi_sense_stream *stream;

		sense = (struct scsi_sense_data_desc *)sense_data;

		stream = (struct scsi_sense_stream *)scsi_find_desc(sense,
		    sense_len, SSD_DESC_STREAM);
		if (stream == NULL)
			goto bailout;

		*stream_bits = stream->byte3;
		break;
	}
	case SSD_TYPE_FIXED: {
		struct scsi_sense_data_fixed *sense;

		sense = (struct scsi_sense_data_fixed *)sense_data;

		if (SSD_FIXED_IS_PRESENT(sense, sense_len, flags) == 0)
			goto bailout;

		if ((sense->flags & (SSD_ILI|SSD_EOM|SSD_FILEMARK)) == 0)
			goto bailout;

		*stream_bits = sense->flags & (SSD_ILI|SSD_EOM|SSD_FILEMARK);
		break;
	}
	default:
		goto bailout;
		break;
	}
	return (0);
bailout:
	return (1);
}

void
scsi_info_sbuf(struct sbuf *sb, uint8_t *cdb, int cdb_len,
	       struct scsi_inquiry_data *inq_data, uint64_t info)
{
	sbuf_printf(sb, "Info: %#jx", info);
}

void
scsi_command_sbuf(struct sbuf *sb, uint8_t *cdb, int cdb_len,
		  struct scsi_inquiry_data *inq_data, uint64_t csi)
{
	sbuf_printf(sb, "Command Specific Info: %#jx", csi);
}


void
scsi_progress_sbuf(struct sbuf *sb, uint16_t progress)
{
	sbuf_printf(sb, "Progress: %d%% (%d/%d) complete",
		    (progress * 100) / SSD_SKS_PROGRESS_DENOM,
		    progress, SSD_SKS_PROGRESS_DENOM);
}

/*
 * Returns 1 for failure (i.e. SKS isn't valid) and 0 for success.
 */
int
scsi_sks_sbuf(struct sbuf *sb, int sense_key, uint8_t *sks)
{
	if ((sks[0] & SSD_SKS_VALID) == 0)
		return (1);

	switch (sense_key) {
	case SSD_KEY_ILLEGAL_REQUEST: {
		struct scsi_sense_sks_field *field;
		int bad_command;
		char tmpstr[40];

		/*Field Pointer*/
		field = (struct scsi_sense_sks_field *)sks;

		if (field->byte0 & SSD_SKS_FIELD_CMD)
			bad_command = 1;
		else
			bad_command = 0;

		tmpstr[0] = '\0';

		/* Bit pointer is valid */
		if (field->byte0 & SSD_SKS_BPV)
			snprintf(tmpstr, sizeof(tmpstr), "bit %d ",
				 field->byte0 & SSD_SKS_BIT_VALUE);

		sbuf_printf(sb, "%s byte %d %sis invalid",
			    bad_command ? "Command" : "Data",
			    scsi_2btoul(field->field), tmpstr);
		break;
	}
	case SSD_KEY_UNIT_ATTENTION: {
		struct scsi_sense_sks_overflow *overflow;

		overflow = (struct scsi_sense_sks_overflow *)sks;

		/*UA Condition Queue Overflow*/
		sbuf_printf(sb, "Unit Attention Condition Queue %s",
			    (overflow->byte0 & SSD_SKS_OVERFLOW_SET) ?
			    "Overflowed" : "Did Not Overflow??");
		break;
	}
	case SSD_KEY_RECOVERED_ERROR:
	case SSD_KEY_HARDWARE_ERROR:
	case SSD_KEY_MEDIUM_ERROR: {
		struct scsi_sense_sks_retry *retry;

		/*Actual Retry Count*/
		retry = (struct scsi_sense_sks_retry *)sks;

		sbuf_printf(sb, "Actual Retry Count: %d",
			    scsi_2btoul(retry->actual_retry_count));
		break;
	}
	case SSD_KEY_NO_SENSE:
	case SSD_KEY_NOT_READY: {
		struct scsi_sense_sks_progress *progress;
		int progress_val;

		/*Progress Indication*/
		progress = (struct scsi_sense_sks_progress *)sks;
		progress_val = scsi_2btoul(progress->progress);

		scsi_progress_sbuf(sb, progress_val);
		break;
	}
	case SSD_KEY_COPY_ABORTED: {
		struct scsi_sense_sks_segment *segment;
		char tmpstr[40];

		/*Segment Pointer*/
		segment = (struct scsi_sense_sks_segment *)sks;

		tmpstr[0] = '\0';

		if (segment->byte0 & SSD_SKS_SEGMENT_BPV)
			snprintf(tmpstr, sizeof(tmpstr), "bit %d ",
				 segment->byte0 & SSD_SKS_SEGMENT_BITPTR);

		sbuf_printf(sb, "%s byte %d %sis invalid", (segment->byte0 &
			    SSD_SKS_SEGMENT_SD) ? "Segment" : "Data",
			    scsi_2btoul(segment->field), tmpstr);
		break;
	}
	default:
		sbuf_printf(sb, "Sense Key Specific: %#x,%#x", sks[0],
			    scsi_2btoul(&sks[1]));
		break;
	}

	return (0);
}

void
scsi_fru_sbuf(struct sbuf *sb, uint64_t fru)
{
	sbuf_printf(sb, "Field Replaceable Unit: %d", (int)fru);
}

void
scsi_stream_sbuf(struct sbuf *sb, uint8_t stream_bits, uint64_t info)
{
	int need_comma;

	need_comma = 0;
	/*
	 * XXX KDM this needs more descriptive decoding.
	 */
	if (stream_bits & SSD_DESC_STREAM_FM) {
		sbuf_printf(sb, "Filemark");
		need_comma = 1;
	}

	if (stream_bits & SSD_DESC_STREAM_EOM) {
		sbuf_printf(sb, "%sEOM", (need_comma) ? "," : "");
		need_comma = 1;
	}

	if (stream_bits & SSD_DESC_STREAM_ILI)
		sbuf_printf(sb, "%sILI", (need_comma) ? "," : "");

	sbuf_printf(sb, ": Info: %#jx", (uintmax_t) info);
}

void
scsi_block_sbuf(struct sbuf *sb, uint8_t block_bits, uint64_t info)
{
	if (block_bits & SSD_DESC_BLOCK_ILI)
		sbuf_printf(sb, "ILI: residue %#jx", (uintmax_t) info);
}

void
scsi_sense_info_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
		     u_int sense_len, uint8_t *cdb, int cdb_len,
		     struct scsi_inquiry_data *inq_data,
		     struct scsi_sense_desc_header *header)
{
	struct scsi_sense_info *info;

	info = (struct scsi_sense_info *)header;

	scsi_info_sbuf(sb, cdb, cdb_len, inq_data, scsi_8btou64(info->info));
}

void
scsi_sense_command_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			u_int sense_len, uint8_t *cdb, int cdb_len,
			struct scsi_inquiry_data *inq_data,
			struct scsi_sense_desc_header *header)
{
	struct scsi_sense_command *command;

	command = (struct scsi_sense_command *)header;

	scsi_command_sbuf(sb, cdb, cdb_len, inq_data,
			  scsi_8btou64(command->command_info));
}

void
scsi_sense_sks_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
		    u_int sense_len, uint8_t *cdb, int cdb_len,
		    struct scsi_inquiry_data *inq_data,
		    struct scsi_sense_desc_header *header)
{
	struct scsi_sense_sks *sks;
	int error_code, sense_key, asc, ascq;

	sks = (struct scsi_sense_sks *)header;

	scsi_extract_sense_len(sense, sense_len, &error_code, &sense_key,
			       &asc, &ascq, /*show_errors*/ 1);

	scsi_sks_sbuf(sb, sense_key, sks->sense_key_spec);
}

void
scsi_sense_fru_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
		    u_int sense_len, uint8_t *cdb, int cdb_len,
		    struct scsi_inquiry_data *inq_data,
		    struct scsi_sense_desc_header *header)
{
	struct scsi_sense_fru *fru;

	fru = (struct scsi_sense_fru *)header;

	scsi_fru_sbuf(sb, (uint64_t)fru->fru);
}

void
scsi_sense_stream_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
		       u_int sense_len, uint8_t *cdb, int cdb_len,
		       struct scsi_inquiry_data *inq_data,
		       struct scsi_sense_desc_header *header)
{
	struct scsi_sense_stream *stream;
	uint64_t info;

	stream = (struct scsi_sense_stream *)header;
	info = 0;

	scsi_get_sense_info(sense, sense_len, SSD_DESC_INFO, &info, NULL);

	scsi_stream_sbuf(sb, stream->byte3, info);
}

void
scsi_sense_block_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
		      u_int sense_len, uint8_t *cdb, int cdb_len,
		      struct scsi_inquiry_data *inq_data,
		      struct scsi_sense_desc_header *header)
{
	struct scsi_sense_block *block;
	uint64_t info;

	block = (struct scsi_sense_block *)header;
	info = 0;

	scsi_get_sense_info(sense, sense_len, SSD_DESC_INFO, &info, NULL);

	scsi_block_sbuf(sb, block->byte3, info);
}

void
scsi_sense_progress_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			 u_int sense_len, uint8_t *cdb, int cdb_len,
			 struct scsi_inquiry_data *inq_data,
			 struct scsi_sense_desc_header *header)
{
	struct scsi_sense_progress *progress;
	const char *sense_key_desc;
	const char *asc_desc;
	int progress_val;

	progress = (struct scsi_sense_progress *)header;

	/*
	 * Get descriptions for the sense key, ASC, and ASCQ in the
	 * progress descriptor.  These could be different than the values
	 * in the overall sense data.
	 */
	scsi_sense_desc(progress->sense_key, progress->add_sense_code,
			progress->add_sense_code_qual, inq_data,
			&sense_key_desc, &asc_desc);

	progress_val = scsi_2btoul(progress->progress);

	/*
	 * The progress indicator is for the operation described by the
	 * sense key, ASC, and ASCQ in the descriptor.
	 */
	sbuf_cat(sb, sense_key_desc);
	sbuf_printf(sb, " asc:%x,%x (%s): ", progress->add_sense_code, 
		    progress->add_sense_code_qual, asc_desc);
	scsi_progress_sbuf(sb, progress_val);
}

/*
 * Generic sense descriptor printing routine.  This is used when we have
 * not yet implemented a specific printing routine for this descriptor.
 */
void
scsi_sense_generic_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			u_int sense_len, uint8_t *cdb, int cdb_len,
			struct scsi_inquiry_data *inq_data,
			struct scsi_sense_desc_header *header)
{
	int i;
	uint8_t *buf_ptr;

	sbuf_printf(sb, "Descriptor %#x:", header->desc_type);

	buf_ptr = (uint8_t *)&header[1];

	for (i = 0; i < header->length; i++, buf_ptr++)
		sbuf_printf(sb, " %02x", *buf_ptr);
}

/*
 * Keep this list in numeric order.  This speeds the array traversal.
 */
struct scsi_sense_desc_printer {
	uint8_t desc_type;
	/*
	 * The function arguments here are the superset of what is needed
	 * to print out various different descriptors.  Command and
	 * information descriptors need inquiry data and command type.
	 * Sense key specific descriptors need the sense key.
	 *
	 * The sense, cdb, and inquiry data arguments may be NULL, but the
	 * information printed may not be fully decoded as a result.
	 */
	void (*print_func)(struct sbuf *sb, struct scsi_sense_data *sense,
			   u_int sense_len, uint8_t *cdb, int cdb_len,
			   struct scsi_inquiry_data *inq_data,
			   struct scsi_sense_desc_header *header);
} scsi_sense_printers[] = {
	{SSD_DESC_INFO, scsi_sense_info_sbuf},
	{SSD_DESC_COMMAND, scsi_sense_command_sbuf},
	{SSD_DESC_SKS, scsi_sense_sks_sbuf},
	{SSD_DESC_FRU, scsi_sense_fru_sbuf},
	{SSD_DESC_STREAM, scsi_sense_stream_sbuf},
	{SSD_DESC_BLOCK, scsi_sense_block_sbuf},
	{SSD_DESC_PROGRESS, scsi_sense_progress_sbuf}
};

void
scsi_sense_desc_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
		     u_int sense_len, uint8_t *cdb, int cdb_len,
		     struct scsi_inquiry_data *inq_data,
		     struct scsi_sense_desc_header *header)
{
	int i;

	for (i = 0; i < (sizeof(scsi_sense_printers) /
	     sizeof(scsi_sense_printers[0])); i++) {
		struct scsi_sense_desc_printer *printer;

		printer = &scsi_sense_printers[i];

		/*
		 * The list is sorted, so quit if we've passed our
		 * descriptor number.
		 */
		if (printer->desc_type > header->desc_type)
			break;

		if (printer->desc_type != header->desc_type)
			continue;

		printer->print_func(sb, sense, sense_len, cdb, cdb_len,
				    inq_data, header);

		return;
	}

	/*
	 * No specific printing routine, so use the generic routine.
	 */
	scsi_sense_generic_sbuf(sb, sense, sense_len, cdb, cdb_len,
				inq_data, header);
}

scsi_sense_data_type
scsi_sense_type(struct scsi_sense_data *sense_data)
{
	switch (sense_data->error_code & SSD_ERRCODE) {
	case SSD_DESC_CURRENT_ERROR:
	case SSD_DESC_DEFERRED_ERROR:
		return (SSD_TYPE_DESC);
		break;
	case SSD_CURRENT_ERROR:
	case SSD_DEFERRED_ERROR:
		return (SSD_TYPE_FIXED);
		break;
	default:
		break;
	}

	return (SSD_TYPE_NONE);
}

struct scsi_print_sense_info {
	struct sbuf *sb;
	char *path_str;
	uint8_t *cdb;
	int cdb_len;
	struct scsi_inquiry_data *inq_data;
};

static int
scsi_print_desc_func(struct scsi_sense_data_desc *sense, u_int sense_len,
		     struct scsi_sense_desc_header *header, void *arg)
{
	struct scsi_print_sense_info *print_info;

	print_info = (struct scsi_print_sense_info *)arg;

	switch (header->desc_type) {
	case SSD_DESC_INFO:
	case SSD_DESC_FRU:
	case SSD_DESC_COMMAND:
	case SSD_DESC_SKS:
	case SSD_DESC_BLOCK:
	case SSD_DESC_STREAM:
		/*
		 * We have already printed these descriptors, if they are
		 * present.
		 */
		break;
	default: {
		sbuf_printf(print_info->sb, "%s", print_info->path_str);
		scsi_sense_desc_sbuf(print_info->sb,
				     (struct scsi_sense_data *)sense, sense_len,
				     print_info->cdb, print_info->cdb_len,
				     print_info->inq_data, header);
		sbuf_printf(print_info->sb, "\n");
		break;
	}
	}

	/*
	 * Tell the iterator that we want to see more descriptors if they
	 * are present.
	 */
	return (0);
}

void
scsi_sense_only_sbuf(struct scsi_sense_data *sense, u_int sense_len,
		     struct sbuf *sb, char *path_str,
		     struct scsi_inquiry_data *inq_data, uint8_t *cdb,
		     int cdb_len)
{
	int error_code, sense_key, asc, ascq;

	sbuf_cat(sb, path_str);

	scsi_extract_sense_len(sense, sense_len, &error_code, &sense_key,
			       &asc, &ascq, /*show_errors*/ 1);

	sbuf_printf(sb, "SCSI sense: ");
	switch (error_code) {
	case SSD_DEFERRED_ERROR:
	case SSD_DESC_DEFERRED_ERROR:
		sbuf_printf(sb, "Deferred error: ");

		/* FALLTHROUGH */
	case SSD_CURRENT_ERROR:
	case SSD_DESC_CURRENT_ERROR:
	{
		struct scsi_sense_data_desc *desc_sense;
		struct scsi_print_sense_info print_info;
		const char *sense_key_desc;
		const char *asc_desc;
		uint8_t sks[3];
		uint64_t val;
		int info_valid;

		/*
		 * Get descriptions for the sense key, ASC, and ASCQ.  If
		 * these aren't present in the sense data (i.e. the sense
		 * data isn't long enough), the -1 values that
		 * scsi_extract_sense_len() returns will yield default
		 * or error descriptions.
		 */
		scsi_sense_desc(sense_key, asc, ascq, inq_data,
				&sense_key_desc, &asc_desc);

		/*
		 * We first print the sense key and ASC/ASCQ.
		 */
		sbuf_cat(sb, sense_key_desc);
		sbuf_printf(sb, " asc:%x,%x (%s)\n", asc, ascq, asc_desc);

		/*
		 * Get the info field if it is valid.
		 */
		if (scsi_get_sense_info(sense, sense_len, SSD_DESC_INFO,
					&val, NULL) == 0)
			info_valid = 1;
		else
			info_valid = 0;

		if (info_valid != 0) {
			uint8_t bits;

			/*
			 * Determine whether we have any block or stream
			 * device-specific information.
			 */
			if (scsi_get_block_info(sense, sense_len, inq_data,
						&bits) == 0) {
				sbuf_cat(sb, path_str);
				scsi_block_sbuf(sb, bits, val);
				sbuf_printf(sb, "\n");
			} else if (scsi_get_stream_info(sense, sense_len,
							inq_data, &bits) == 0) {
				sbuf_cat(sb, path_str);
				scsi_stream_sbuf(sb, bits, val);
				sbuf_printf(sb, "\n");
			} else if (val != 0) {
				/*
				 * The information field can be valid but 0.
				 * If the block or stream bits aren't set,
				 * and this is 0, it isn't terribly useful
				 * to print it out.
				 */
				sbuf_cat(sb, path_str);
				scsi_info_sbuf(sb, cdb, cdb_len, inq_data, val);
				sbuf_printf(sb, "\n");
			}
		}

		/* 
		 * Print the FRU.
		 */
		if (scsi_get_sense_info(sense, sense_len, SSD_DESC_FRU,
					&val, NULL) == 0) {
			sbuf_cat(sb, path_str);
			scsi_fru_sbuf(sb, val);
			sbuf_printf(sb, "\n");
		}

		/*
		 * Print any command-specific information.
		 */
		if (scsi_get_sense_info(sense, sense_len, SSD_DESC_COMMAND,
					&val, NULL) == 0) {
			sbuf_cat(sb, path_str);
			scsi_command_sbuf(sb, cdb, cdb_len, inq_data, val);
			sbuf_printf(sb, "\n");
		}

		/*
		 * Print out any sense-key-specific information.
		 */
		if (scsi_get_sks(sense, sense_len, sks) == 0) {
			sbuf_cat(sb, path_str);
			scsi_sks_sbuf(sb, sense_key, sks);
			sbuf_printf(sb, "\n");
		}

		/*
		 * If this is fixed sense, we're done.  If we have
		 * descriptor sense, we might have more information
		 * available.
		 */
		if (scsi_sense_type(sense) != SSD_TYPE_DESC)
			break;

		desc_sense = (struct scsi_sense_data_desc *)sense;

		print_info.sb = sb;
		print_info.path_str = path_str;
		print_info.cdb = cdb;
		print_info.cdb_len = cdb_len;
		print_info.inq_data = inq_data;

		/*
		 * Print any sense descriptors that we have not already printed.
		 */
		scsi_desc_iterate(desc_sense, sense_len, scsi_print_desc_func,
				  &print_info);
		break;

	}
	case -1:
		/*
		 * scsi_extract_sense_len() sets values to -1 if the
		 * show_errors flag is set and they aren't present in the
		 * sense data.  This means that sense_len is 0.
		 */
		sbuf_printf(sb, "No sense data present\n");
		break;
	default: {
		sbuf_printf(sb, "Error code 0x%x", error_code);
		if (sense->error_code & SSD_ERRCODE_VALID) {
			struct scsi_sense_data_fixed *fixed_sense;

			fixed_sense = (struct scsi_sense_data_fixed *)sense;

			if (SSD_FIXED_IS_PRESENT(fixed_sense, sense_len, info)){
				uint32_t info;

				info = scsi_4btoul(fixed_sense->info);

				sbuf_printf(sb, " at block no. %d (decimal)",
					    info);
			}
		}
		sbuf_printf(sb, "\n");
		break;
	}
	}
}

/*
 * scsi_sense_sbuf() returns 0 for success and -1 for failure.
 */
#ifdef _KERNEL
int
scsi_sense_sbuf(struct ccb_scsiio *csio, struct sbuf *sb,
		scsi_sense_string_flags flags)
#else /* !_KERNEL */
int
scsi_sense_sbuf(struct cam_device *device, struct ccb_scsiio *csio, 
		struct sbuf *sb, scsi_sense_string_flags flags)
#endif /* _KERNEL/!_KERNEL */
{
	struct	  scsi_sense_data *sense;
	struct	  scsi_inquiry_data *inq_data;
#ifdef _KERNEL
	struct	  ccb_getdev *cgd;
#endif /* _KERNEL */
	char	  path_str[64];
	uint8_t	  *cdb;

#ifndef _KERNEL
	if (device == NULL)
		return(-1);
#endif /* !_KERNEL */
	if ((csio == NULL) || (sb == NULL))
		return(-1);

	/*
	 * If the CDB is a physical address, we can't deal with it..
	 */
	if ((csio->ccb_h.flags & CAM_CDB_PHYS) != 0)
		flags &= ~SSS_FLAG_PRINT_COMMAND;

#ifdef _KERNEL
	xpt_path_string(csio->ccb_h.path, path_str, sizeof(path_str));
#else /* !_KERNEL */
	cam_path_string(device, path_str, sizeof(path_str));
#endif /* _KERNEL/!_KERNEL */

#ifdef _KERNEL
	if ((cgd = (struct ccb_getdev*)xpt_alloc_ccb_nowait()) == NULL)
		return(-1);
	/*
	 * Get the device information.
	 */
	xpt_setup_ccb(&cgd->ccb_h,
		      csio->ccb_h.path,
		      CAM_PRIORITY_NORMAL);
	cgd->ccb_h.func_code = XPT_GDEV_TYPE;
	xpt_action((union ccb *)cgd);

	/*
	 * If the device is unconfigured, just pretend that it is a hard
	 * drive.  scsi_op_desc() needs this.
	 */
	if (cgd->ccb_h.status == CAM_DEV_NOT_THERE)
		cgd->inq_data.device = T_DIRECT;

	inq_data = &cgd->inq_data;

#else /* !_KERNEL */

	inq_data = &device->inq_data;

#endif /* _KERNEL/!_KERNEL */

	sense = NULL;

	if (flags & SSS_FLAG_PRINT_COMMAND) {

		sbuf_cat(sb, path_str);

#ifdef _KERNEL
		scsi_command_string(csio, sb);
#else /* !_KERNEL */
		scsi_command_string(device, csio, sb);
#endif /* _KERNEL/!_KERNEL */
		sbuf_printf(sb, "\n");
	}

	/*
	 * If the sense data is a physical pointer, forget it.
	 */
	if (csio->ccb_h.flags & CAM_SENSE_PTR) {
		if (csio->ccb_h.flags & CAM_SENSE_PHYS) {
#ifdef _KERNEL
			xpt_free_ccb((union ccb*)cgd);
#endif /* _KERNEL/!_KERNEL */
			return(-1);
		} else {
			/* 
			 * bcopy the pointer to avoid unaligned access
			 * errors on finicky architectures.  We don't
			 * ensure that the sense data is pointer aligned.
			 */
			bcopy(&csio->sense_data, &sense, 
			      sizeof(struct scsi_sense_data *));
		}
	} else {
		/*
		 * If the physical sense flag is set, but the sense pointer
		 * is not also set, we assume that the user is an idiot and
		 * return.  (Well, okay, it could be that somehow, the
		 * entire csio is physical, but we would have probably core
		 * dumped on one of the bogus pointer deferences above
		 * already.)
		 */
		if (csio->ccb_h.flags & CAM_SENSE_PHYS) {
#ifdef _KERNEL
			xpt_free_ccb((union ccb*)cgd);
#endif /* _KERNEL/!_KERNEL */
			return(-1);
		} else
			sense = &csio->sense_data;
	}

	if (csio->ccb_h.flags & CAM_CDB_POINTER)
		cdb = csio->cdb_io.cdb_ptr;
	else
		cdb = csio->cdb_io.cdb_bytes;

	scsi_sense_only_sbuf(sense, csio->sense_len - csio->sense_resid, sb,
			     path_str, inq_data, cdb, csio->cdb_len);
			 
#ifdef _KERNEL
	xpt_free_ccb((union ccb*)cgd);
#endif /* _KERNEL/!_KERNEL */
	return(0);
}



#ifdef _KERNEL
char *
scsi_sense_string(struct ccb_scsiio *csio, char *str, int str_len)
#else /* !_KERNEL */
char *
scsi_sense_string(struct cam_device *device, struct ccb_scsiio *csio,
		  char *str, int str_len)
#endif /* _KERNEL/!_KERNEL */
{
	struct sbuf sb;

	sbuf_new(&sb, str, str_len, 0);

#ifdef _KERNEL
	scsi_sense_sbuf(csio, &sb, SSS_FLAG_PRINT_COMMAND);
#else /* !_KERNEL */
	scsi_sense_sbuf(device, csio, &sb, SSS_FLAG_PRINT_COMMAND);
#endif /* _KERNEL/!_KERNEL */

	sbuf_finish(&sb);

	return(sbuf_data(&sb));
}

#ifdef _KERNEL
void 
scsi_sense_print(struct ccb_scsiio *csio)
{
	struct sbuf sb;
	char str[512];

	sbuf_new(&sb, str, sizeof(str), 0);

	scsi_sense_sbuf(csio, &sb, SSS_FLAG_PRINT_COMMAND);

	sbuf_finish(&sb);

	printf("%s", sbuf_data(&sb));
}

#else /* !_KERNEL */
void
scsi_sense_print(struct cam_device *device, struct ccb_scsiio *csio, 
		 FILE *ofile)
{
	struct sbuf sb;
	char str[512];

	if ((device == NULL) || (csio == NULL) || (ofile == NULL))
		return;

	sbuf_new(&sb, str, sizeof(str), 0);

	scsi_sense_sbuf(device, csio, &sb, SSS_FLAG_PRINT_COMMAND);

	sbuf_finish(&sb);

	fprintf(ofile, "%s", sbuf_data(&sb));
}

#endif /* _KERNEL/!_KERNEL */

/*
 * Extract basic sense information.  This is backward-compatible with the
 * previous implementation.  For new implementations,
 * scsi_extract_sense_len() is recommended.
 */
void
scsi_extract_sense(struct scsi_sense_data *sense_data, int *error_code,
		   int *sense_key, int *asc, int *ascq)
{
	scsi_extract_sense_len(sense_data, sizeof(*sense_data), error_code,
			       sense_key, asc, ascq, /*show_errors*/ 0);
}

/*
 * Extract basic sense information from SCSI I/O CCB structure.
 */
int
scsi_extract_sense_ccb(union ccb *ccb,
    int *error_code, int *sense_key, int *asc, int *ascq)
{
	struct scsi_sense_data *sense_data;

	/* Make sure there are some sense data we can access. */
	if (ccb->ccb_h.func_code != XPT_SCSI_IO ||
	    (ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_SCSI_STATUS_ERROR ||
	    (ccb->csio.scsi_status != SCSI_STATUS_CHECK_COND) ||
	    (ccb->ccb_h.status & CAM_AUTOSNS_VALID) == 0 ||
	    (ccb->ccb_h.flags & CAM_SENSE_PHYS))
		return (0);

	if (ccb->ccb_h.flags & CAM_SENSE_PTR)
		bcopy(&ccb->csio.sense_data, &sense_data,
		    sizeof(struct scsi_sense_data *));
	else
		sense_data = &ccb->csio.sense_data;
	scsi_extract_sense_len(sense_data,
	    ccb->csio.sense_len - ccb->csio.sense_resid,
	    error_code, sense_key, asc, ascq, 1);
	if (*error_code == -1)
		return (0);
	return (1);
}

/*
 * Extract basic sense information.  If show_errors is set, sense values
 * will be set to -1 if they are not present.
 */
void
scsi_extract_sense_len(struct scsi_sense_data *sense_data, u_int sense_len,
		       int *error_code, int *sense_key, int *asc, int *ascq,
		       int show_errors)
{
	/*
	 * If we have no length, we have no sense.
	 */
	if (sense_len == 0) {
		if (show_errors == 0) {
			*error_code = 0;
			*sense_key = 0;
			*asc = 0;
			*ascq = 0;
		} else {
			*error_code = -1;
			*sense_key = -1;
			*asc = -1;
			*ascq = -1;
		}
		return;
	}

	*error_code = sense_data->error_code & SSD_ERRCODE;

	switch (*error_code) {
	case SSD_DESC_CURRENT_ERROR:
	case SSD_DESC_DEFERRED_ERROR: {
		struct scsi_sense_data_desc *sense;

		sense = (struct scsi_sense_data_desc *)sense_data;

		if (SSD_DESC_IS_PRESENT(sense, sense_len, sense_key))
			*sense_key = sense->sense_key & SSD_KEY;
		else
			*sense_key = (show_errors) ? -1 : 0;

		if (SSD_DESC_IS_PRESENT(sense, sense_len, add_sense_code))
			*asc = sense->add_sense_code;
		else
			*asc = (show_errors) ? -1 : 0;

		if (SSD_DESC_IS_PRESENT(sense, sense_len, add_sense_code_qual))
			*ascq = sense->add_sense_code_qual;
		else
			*ascq = (show_errors) ? -1 : 0;
		break;
	}
	case SSD_CURRENT_ERROR:
	case SSD_DEFERRED_ERROR:
	default: {
		struct scsi_sense_data_fixed *sense;

		sense = (struct scsi_sense_data_fixed *)sense_data;

		if (SSD_FIXED_IS_PRESENT(sense, sense_len, flags))
			*sense_key = sense->flags & SSD_KEY;
		else
			*sense_key = (show_errors) ? -1 : 0;

		if ((SSD_FIXED_IS_PRESENT(sense, sense_len, add_sense_code))
		 && (SSD_FIXED_IS_FILLED(sense, add_sense_code)))
			*asc = sense->add_sense_code;
		else
			*asc = (show_errors) ? -1 : 0;

		if ((SSD_FIXED_IS_PRESENT(sense, sense_len,add_sense_code_qual))
		 && (SSD_FIXED_IS_FILLED(sense, add_sense_code_qual)))
			*ascq = sense->add_sense_code_qual;
		else
			*ascq = (show_errors) ? -1 : 0;
		break;
	}
	}
}

int
scsi_get_sense_key(struct scsi_sense_data *sense_data, u_int sense_len,
		   int show_errors)
{
	int error_code, sense_key, asc, ascq;

	scsi_extract_sense_len(sense_data, sense_len, &error_code,
			       &sense_key, &asc, &ascq, show_errors);

	return (sense_key);
}

int
scsi_get_asc(struct scsi_sense_data *sense_data, u_int sense_len,
	     int show_errors)
{
	int error_code, sense_key, asc, ascq;

	scsi_extract_sense_len(sense_data, sense_len, &error_code,
			       &sense_key, &asc, &ascq, show_errors);

	return (asc);
}

int
scsi_get_ascq(struct scsi_sense_data *sense_data, u_int sense_len,
	      int show_errors)
{
	int error_code, sense_key, asc, ascq;

	scsi_extract_sense_len(sense_data, sense_len, &error_code,
			       &sense_key, &asc, &ascq, show_errors);

	return (ascq);
}

/*
 * This function currently requires at least 36 bytes, or
 * SHORT_INQUIRY_LENGTH, worth of data to function properly.  If this
 * function needs more or less data in the future, another length should be
 * defined in scsi_all.h to indicate the minimum amount of data necessary
 * for this routine to function properly.
 */
void
scsi_print_inquiry(struct scsi_inquiry_data *inq_data)
{
	u_int8_t type;
	char *dtype, *qtype;
	char vendor[16], product[48], revision[16], rstr[4];

	type = SID_TYPE(inq_data);

	/*
	 * Figure out basic device type and qualifier.
	 */
	if (SID_QUAL_IS_VENDOR_UNIQUE(inq_data)) {
		qtype = "(vendor-unique qualifier)";
	} else {
		switch (SID_QUAL(inq_data)) {
		case SID_QUAL_LU_CONNECTED:
			qtype = "";
			break;

		case SID_QUAL_LU_OFFLINE:
			qtype = "(offline)";
			break;

		case SID_QUAL_RSVD:
			qtype = "(reserved qualifier)";
			break;
		default:
		case SID_QUAL_BAD_LU:
			qtype = "(LUN not supported)";
			break;
		}
	}

	switch (type) {
	case T_DIRECT:
		dtype = "Direct Access";
		break;
	case T_SEQUENTIAL:
		dtype = "Sequential Access";
		break;
	case T_PRINTER:
		dtype = "Printer";
		break;
	case T_PROCESSOR:
		dtype = "Processor";
		break;
	case T_WORM:
		dtype = "WORM";
		break;
	case T_CDROM:
		dtype = "CD-ROM";
		break;
	case T_SCANNER:
		dtype = "Scanner";
		break;
	case T_OPTICAL:
		dtype = "Optical";
		break;
	case T_CHANGER:
		dtype = "Changer";
		break;
	case T_COMM:
		dtype = "Communication";
		break;
	case T_STORARRAY:
		dtype = "Storage Array";
		break;
	case T_ENCLOSURE:
		dtype = "Enclosure Services";
		break;
	case T_RBC:
		dtype = "Simplified Direct Access";
		break;
	case T_OCRW:
		dtype = "Optical Card Read/Write";
		break;
	case T_OSD:
		dtype = "Object-Based Storage";
		break;
	case T_ADC:
		dtype = "Automation/Drive Interface";
		break;
	case T_NODEVICE:
		dtype = "Uninstalled";
		break;
	default:
		dtype = "unknown";
		break;
	}

	cam_strvis(vendor, inq_data->vendor, sizeof(inq_data->vendor),
		   sizeof(vendor));
	cam_strvis(product, inq_data->product, sizeof(inq_data->product),
		   sizeof(product));
	cam_strvis(revision, inq_data->revision, sizeof(inq_data->revision),
		   sizeof(revision));

	if (SID_ANSI_REV(inq_data) == SCSI_REV_CCS)
		bcopy("CCS", rstr, 4);
	else
		snprintf(rstr, sizeof (rstr), "%d", SID_ANSI_REV(inq_data));
	printf("<%s %s %s> %s %s SCSI-%s device %s\n",
	       vendor, product, revision,
	       SID_IS_REMOVABLE(inq_data) ? "Removable" : "Fixed",
	       dtype, rstr, qtype);
}

void
scsi_print_inquiry_short(struct scsi_inquiry_data *inq_data)
{
	char vendor[16], product[48], revision[16];

	cam_strvis(vendor, inq_data->vendor, sizeof(inq_data->vendor),
		   sizeof(vendor));
	cam_strvis(product, inq_data->product, sizeof(inq_data->product),
		   sizeof(product));
	cam_strvis(revision, inq_data->revision, sizeof(inq_data->revision),
		   sizeof(revision));

	printf("<%s %s %s>", vendor, product, revision);
}

/*
 * Table of syncrates that don't follow the "divisible by 4"
 * rule. This table will be expanded in future SCSI specs.
 */
static struct {
	u_int period_factor;
	u_int period;	/* in 100ths of ns */
} scsi_syncrates[] = {
	{ 0x08, 625 },	/* FAST-160 */
	{ 0x09, 1250 },	/* FAST-80 */
	{ 0x0a, 2500 },	/* FAST-40 40MHz */
	{ 0x0b, 3030 },	/* FAST-40 33MHz */
	{ 0x0c, 5000 }	/* FAST-20 */
};

/*
 * Return the frequency in kHz corresponding to the given
 * sync period factor.
 */
u_int
scsi_calc_syncsrate(u_int period_factor)
{
	int i;
	int num_syncrates;

	/*
	 * It's a bug if period is zero, but if it is anyway, don't
	 * die with a divide fault- instead return something which
	 * 'approximates' async
	 */
	if (period_factor == 0) {
		return (3300);
	}

	num_syncrates = sizeof(scsi_syncrates) / sizeof(scsi_syncrates[0]);
	/* See if the period is in the "exception" table */
	for (i = 0; i < num_syncrates; i++) {

		if (period_factor == scsi_syncrates[i].period_factor) {
			/* Period in kHz */
			return (100000000 / scsi_syncrates[i].period);
		}
	}

	/*
	 * Wasn't in the table, so use the standard
	 * 4 times conversion.
	 */
	return (10000000 / (period_factor * 4 * 10));
}

/*
 * Return the SCSI sync parameter that corresponsd to
 * the passed in period in 10ths of ns.
 */
u_int
scsi_calc_syncparam(u_int period)
{
	int i;
	int num_syncrates;

	if (period == 0)
		return (~0);	/* Async */

	/* Adjust for exception table being in 100ths. */
	period *= 10;
	num_syncrates = sizeof(scsi_syncrates) / sizeof(scsi_syncrates[0]);
	/* See if the period is in the "exception" table */
	for (i = 0; i < num_syncrates; i++) {

		if (period <= scsi_syncrates[i].period) {
			/* Period in 100ths of ns */
			return (scsi_syncrates[i].period_factor);
		}
	}

	/*
	 * Wasn't in the table, so use the standard
	 * 1/4 period in ns conversion.
	 */
	return (period/400);
}

int
scsi_devid_is_naa_ieee_reg(uint8_t *bufp)
{
	struct scsi_vpd_id_descriptor *descr;
	struct scsi_vpd_id_naa_basic *naa;

	descr = (struct scsi_vpd_id_descriptor *)bufp;
	naa = (struct scsi_vpd_id_naa_basic *)descr->identifier;
	if ((descr->id_type & SVPD_ID_TYPE_MASK) != SVPD_ID_TYPE_NAA)
		return 0;
	if (descr->length < sizeof(struct scsi_vpd_id_naa_ieee_reg))
		return 0;
	if ((naa->naa >> SVPD_ID_NAA_NAA_SHIFT) != SVPD_ID_NAA_IEEE_REG)
		return 0;
	return 1;
}

int
scsi_devid_is_sas_target(uint8_t *bufp)
{
	struct scsi_vpd_id_descriptor *descr;

	descr = (struct scsi_vpd_id_descriptor *)bufp;
	if (!scsi_devid_is_naa_ieee_reg(bufp))
		return 0;
	if ((descr->id_type & SVPD_ID_PIV) == 0) /* proto field reserved */
		return 0;
	if ((descr->proto_codeset >> SVPD_ID_PROTO_SHIFT) != SCSI_PROTO_SAS)
		return 0;
	return 1;
}

int
scsi_devid_is_lun_eui64(uint8_t *bufp)
{
	struct scsi_vpd_id_descriptor *descr;

	descr = (struct scsi_vpd_id_descriptor *)bufp;
	if ((descr->id_type & SVPD_ID_ASSOC_MASK) != SVPD_ID_ASSOC_LUN)
		return 0;
	if ((descr->id_type & SVPD_ID_TYPE_MASK) != SVPD_ID_TYPE_EUI64)
		return 0;
	return 1;
}

int
scsi_devid_is_lun_naa(uint8_t *bufp)
{
	struct scsi_vpd_id_descriptor *descr;

	descr = (struct scsi_vpd_id_descriptor *)bufp;
	if ((descr->id_type & SVPD_ID_ASSOC_MASK) != SVPD_ID_ASSOC_LUN)
		return 0;
	if ((descr->id_type & SVPD_ID_TYPE_MASK) != SVPD_ID_TYPE_NAA)
		return 0;
	return 1;
}

int
scsi_devid_is_lun_t10(uint8_t *bufp)
{
	struct scsi_vpd_id_descriptor *descr;

	descr = (struct scsi_vpd_id_descriptor *)bufp;
	if ((descr->id_type & SVPD_ID_ASSOC_MASK) != SVPD_ID_ASSOC_LUN)
		return 0;
	if ((descr->id_type & SVPD_ID_TYPE_MASK) != SVPD_ID_TYPE_T10)
		return 0;
	return 1;
}

int
scsi_devid_is_lun_name(uint8_t *bufp)
{
	struct scsi_vpd_id_descriptor *descr;

	descr = (struct scsi_vpd_id_descriptor *)bufp;
	if ((descr->id_type & SVPD_ID_ASSOC_MASK) != SVPD_ID_ASSOC_LUN)
		return 0;
	if ((descr->id_type & SVPD_ID_TYPE_MASK) != SVPD_ID_TYPE_SCSI_NAME)
		return 0;
	return 1;
}

struct scsi_vpd_id_descriptor *
scsi_get_devid(struct scsi_vpd_device_id *id, uint32_t page_len,
    scsi_devid_checkfn_t ck_fn)
{
	struct scsi_vpd_id_descriptor *desc;
	uint8_t *page_end;
	uint8_t *desc_buf_end;

	page_end = (uint8_t *)id + page_len;
	if (page_end < id->desc_list)
		return (NULL);

	desc_buf_end = MIN(id->desc_list + scsi_2btoul(id->length), page_end);

	for (desc = (struct scsi_vpd_id_descriptor *)id->desc_list;
	     desc->identifier <= desc_buf_end
	  && desc->identifier + desc->length <= desc_buf_end;
	     desc = (struct scsi_vpd_id_descriptor *)(desc->identifier
						    + desc->length)) {

		if (ck_fn == NULL || ck_fn((uint8_t *)desc) != 0)
			return (desc);
	}

	return (NULL);
}

void
scsi_test_unit_ready(struct ccb_scsiio *csio, u_int32_t retries,
		     void (*cbfcnp)(struct cam_periph *, union ccb *),
		     u_int8_t tag_action, u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_test_unit_ready *scsi_cmd;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/NULL,
		      /*dxfer_len*/0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);

	scsi_cmd = (struct scsi_test_unit_ready *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = TEST_UNIT_READY;
}

void
scsi_request_sense(struct ccb_scsiio *csio, u_int32_t retries,
		   void (*cbfcnp)(struct cam_periph *, union ccb *),
		   void *data_ptr, u_int8_t dxfer_len, u_int8_t tag_action,
		   u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_request_sense *scsi_cmd;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      CAM_DIR_IN,
		      tag_action,
		      data_ptr,
		      dxfer_len,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);

	scsi_cmd = (struct scsi_request_sense *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = REQUEST_SENSE;
	scsi_cmd->length = dxfer_len;
}

void
scsi_inquiry(struct ccb_scsiio *csio, u_int32_t retries,
	     void (*cbfcnp)(struct cam_periph *, union ccb *),
	     u_int8_t tag_action, u_int8_t *inq_buf, u_int32_t inq_len,
	     int evpd, u_int8_t page_code, u_int8_t sense_len,
	     u_int32_t timeout)
{
	struct scsi_inquiry *scsi_cmd;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_IN,
		      tag_action,
		      /*data_ptr*/inq_buf,
		      /*dxfer_len*/inq_len,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);

	scsi_cmd = (struct scsi_inquiry *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = INQUIRY;
	if (evpd) {
		scsi_cmd->byte2 |= SI_EVPD;
		scsi_cmd->page_code = page_code;		
	}
	scsi_ulto2b(inq_len, scsi_cmd->length);
}

void
scsi_mode_sense(struct ccb_scsiio *csio, u_int32_t retries,
		void (*cbfcnp)(struct cam_periph *, union ccb *),
		u_int8_t tag_action, int dbd, u_int8_t page_code,
		u_int8_t page, u_int8_t *param_buf, u_int32_t param_len,
		u_int8_t sense_len, u_int32_t timeout)
{

	scsi_mode_sense_len(csio, retries, cbfcnp, tag_action, dbd,
			    page_code, page, param_buf, param_len, 0,
			    sense_len, timeout);
}

void
scsi_mode_sense_len(struct ccb_scsiio *csio, u_int32_t retries,
		    void (*cbfcnp)(struct cam_periph *, union ccb *),
		    u_int8_t tag_action, int dbd, u_int8_t page_code,
		    u_int8_t page, u_int8_t *param_buf, u_int32_t param_len,
		    int minimum_cmd_size, u_int8_t sense_len, u_int32_t timeout)
{
	u_int8_t cdb_len;

	/*
	 * Use the smallest possible command to perform the operation.
	 */
	if ((param_len < 256)
	 && (minimum_cmd_size < 10)) {
		/*
		 * We can fit in a 6 byte cdb.
		 */
		struct scsi_mode_sense_6 *scsi_cmd;

		scsi_cmd = (struct scsi_mode_sense_6 *)&csio->cdb_io.cdb_bytes;
		bzero(scsi_cmd, sizeof(*scsi_cmd));
		scsi_cmd->opcode = MODE_SENSE_6;
		if (dbd != 0)
			scsi_cmd->byte2 |= SMS_DBD;
		scsi_cmd->page = page_code | page;
		scsi_cmd->length = param_len;
		cdb_len = sizeof(*scsi_cmd);
	} else {
		/*
		 * Need a 10 byte cdb.
		 */
		struct scsi_mode_sense_10 *scsi_cmd;

		scsi_cmd = (struct scsi_mode_sense_10 *)&csio->cdb_io.cdb_bytes;
		bzero(scsi_cmd, sizeof(*scsi_cmd));
		scsi_cmd->opcode = MODE_SENSE_10;
		if (dbd != 0)
			scsi_cmd->byte2 |= SMS_DBD;
		scsi_cmd->page = page_code | page;
		scsi_ulto2b(param_len, scsi_cmd->length);
		cdb_len = sizeof(*scsi_cmd);
	}
	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      CAM_DIR_IN,
		      tag_action,
		      param_buf,
		      param_len,
		      sense_len,
		      cdb_len,
		      timeout);
}

void
scsi_mode_select(struct ccb_scsiio *csio, u_int32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 u_int8_t tag_action, int scsi_page_fmt, int save_pages,
		 u_int8_t *param_buf, u_int32_t param_len, u_int8_t sense_len,
		 u_int32_t timeout)
{
	scsi_mode_select_len(csio, retries, cbfcnp, tag_action,
			     scsi_page_fmt, save_pages, param_buf,
			     param_len, 0, sense_len, timeout);
}

void
scsi_mode_select_len(struct ccb_scsiio *csio, u_int32_t retries,
		     void (*cbfcnp)(struct cam_periph *, union ccb *),
		     u_int8_t tag_action, int scsi_page_fmt, int save_pages,
		     u_int8_t *param_buf, u_int32_t param_len,
		     int minimum_cmd_size, u_int8_t sense_len,
		     u_int32_t timeout)
{
	u_int8_t cdb_len;

	/*
	 * Use the smallest possible command to perform the operation.
	 */
	if ((param_len < 256)
	 && (minimum_cmd_size < 10)) {
		/*
		 * We can fit in a 6 byte cdb.
		 */
		struct scsi_mode_select_6 *scsi_cmd;

		scsi_cmd = (struct scsi_mode_select_6 *)&csio->cdb_io.cdb_bytes;
		bzero(scsi_cmd, sizeof(*scsi_cmd));
		scsi_cmd->opcode = MODE_SELECT_6;
		if (scsi_page_fmt != 0)
			scsi_cmd->byte2 |= SMS_PF;
		if (save_pages != 0)
			scsi_cmd->byte2 |= SMS_SP;
		scsi_cmd->length = param_len;
		cdb_len = sizeof(*scsi_cmd);
	} else {
		/*
		 * Need a 10 byte cdb.
		 */
		struct scsi_mode_select_10 *scsi_cmd;

		scsi_cmd =
		    (struct scsi_mode_select_10 *)&csio->cdb_io.cdb_bytes;
		bzero(scsi_cmd, sizeof(*scsi_cmd));
		scsi_cmd->opcode = MODE_SELECT_10;
		if (scsi_page_fmt != 0)
			scsi_cmd->byte2 |= SMS_PF;
		if (save_pages != 0)
			scsi_cmd->byte2 |= SMS_SP;
		scsi_ulto2b(param_len, scsi_cmd->length);
		cdb_len = sizeof(*scsi_cmd);
	}
	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      CAM_DIR_OUT,
		      tag_action,
		      param_buf,
		      param_len,
		      sense_len,
		      cdb_len,
		      timeout);
}

void
scsi_log_sense(struct ccb_scsiio *csio, u_int32_t retries,
	       void (*cbfcnp)(struct cam_periph *, union ccb *),
	       u_int8_t tag_action, u_int8_t page_code, u_int8_t page,
	       int save_pages, int ppc, u_int32_t paramptr,
	       u_int8_t *param_buf, u_int32_t param_len, u_int8_t sense_len,
	       u_int32_t timeout)
{
	struct scsi_log_sense *scsi_cmd;
	u_int8_t cdb_len;

	scsi_cmd = (struct scsi_log_sense *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = LOG_SENSE;
	scsi_cmd->page = page_code | page;
	if (save_pages != 0)
		scsi_cmd->byte2 |= SLS_SP;
	if (ppc != 0)
		scsi_cmd->byte2 |= SLS_PPC;
	scsi_ulto2b(paramptr, scsi_cmd->paramptr);
	scsi_ulto2b(param_len, scsi_cmd->length);
	cdb_len = sizeof(*scsi_cmd);

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_IN,
		      tag_action,
		      /*data_ptr*/param_buf,
		      /*dxfer_len*/param_len,
		      sense_len,
		      cdb_len,
		      timeout);
}

void
scsi_log_select(struct ccb_scsiio *csio, u_int32_t retries,
		void (*cbfcnp)(struct cam_periph *, union ccb *),
		u_int8_t tag_action, u_int8_t page_code, int save_pages,
		int pc_reset, u_int8_t *param_buf, u_int32_t param_len,
		u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_log_select *scsi_cmd;
	u_int8_t cdb_len;

	scsi_cmd = (struct scsi_log_select *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = LOG_SELECT;
	scsi_cmd->page = page_code & SLS_PAGE_CODE;
	if (save_pages != 0)
		scsi_cmd->byte2 |= SLS_SP;
	if (pc_reset != 0)
		scsi_cmd->byte2 |= SLS_PCR;
	scsi_ulto2b(param_len, scsi_cmd->length);
	cdb_len = sizeof(*scsi_cmd);

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_OUT,
		      tag_action,
		      /*data_ptr*/param_buf,
		      /*dxfer_len*/param_len,
		      sense_len,
		      cdb_len,
		      timeout);
}

/*
 * Prevent or allow the user to remove the media
 */
void
scsi_prevent(struct ccb_scsiio *csio, u_int32_t retries,
	     void (*cbfcnp)(struct cam_periph *, union ccb *),
	     u_int8_t tag_action, u_int8_t action,
	     u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_prevent *scsi_cmd;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/NULL,
		      /*dxfer_len*/0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);

	scsi_cmd = (struct scsi_prevent *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = PREVENT_ALLOW;
	scsi_cmd->how = action;
}

/* XXX allow specification of address and PMI bit and LBA */
void
scsi_read_capacity(struct ccb_scsiio *csio, u_int32_t retries,
		   void (*cbfcnp)(struct cam_periph *, union ccb *),
		   u_int8_t tag_action,
		   struct scsi_read_capacity_data *rcap_buf,
		   u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_read_capacity *scsi_cmd;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_IN,
		      tag_action,
		      /*data_ptr*/(u_int8_t *)rcap_buf,
		      /*dxfer_len*/sizeof(*rcap_buf),
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);

	scsi_cmd = (struct scsi_read_capacity *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = READ_CAPACITY;
}

void
scsi_read_capacity_16(struct ccb_scsiio *csio, uint32_t retries,
		      void (*cbfcnp)(struct cam_periph *, union ccb *),
		      uint8_t tag_action, uint64_t lba, int reladr, int pmi,
		      uint8_t *rcap_buf, int rcap_buf_len, uint8_t sense_len,
		      uint32_t timeout)
{
	struct scsi_read_capacity_16 *scsi_cmd;

	
	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_IN,
		      tag_action,
		      /*data_ptr*/(u_int8_t *)rcap_buf,
		      /*dxfer_len*/rcap_buf_len,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
	scsi_cmd = (struct scsi_read_capacity_16 *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = SERVICE_ACTION_IN;
	scsi_cmd->service_action = SRC16_SERVICE_ACTION;
	scsi_u64to8b(lba, scsi_cmd->addr);
	scsi_ulto4b(rcap_buf_len, scsi_cmd->alloc_len);
	if (pmi)
		reladr |= SRC16_PMI;
	if (reladr)
		reladr |= SRC16_RELADR;
}

void
scsi_report_luns(struct ccb_scsiio *csio, u_int32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 u_int8_t tag_action, u_int8_t select_report,
		 struct scsi_report_luns_data *rpl_buf, u_int32_t alloc_len,
		 u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_report_luns *scsi_cmd;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_IN,
		      tag_action,
		      /*data_ptr*/(u_int8_t *)rpl_buf,
		      /*dxfer_len*/alloc_len,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
	scsi_cmd = (struct scsi_report_luns *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = REPORT_LUNS;
	scsi_cmd->select_report = select_report;
	scsi_ulto4b(alloc_len, scsi_cmd->length);
}

void
scsi_report_target_group(struct ccb_scsiio *csio, u_int32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 u_int8_t tag_action, u_int8_t pdf,
		 void *buf, u_int32_t alloc_len,
		 u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_target_group *scsi_cmd;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_IN,
		      tag_action,
		      /*data_ptr*/(u_int8_t *)buf,
		      /*dxfer_len*/alloc_len,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
	scsi_cmd = (struct scsi_target_group *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = MAINTENANCE_IN;
	scsi_cmd->service_action = REPORT_TARGET_PORT_GROUPS | pdf;
	scsi_ulto4b(alloc_len, scsi_cmd->length);
}

void
scsi_set_target_group(struct ccb_scsiio *csio, u_int32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 u_int8_t tag_action, void *buf, u_int32_t alloc_len,
		 u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_target_group *scsi_cmd;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_OUT,
		      tag_action,
		      /*data_ptr*/(u_int8_t *)buf,
		      /*dxfer_len*/alloc_len,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
	scsi_cmd = (struct scsi_target_group *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = MAINTENANCE_OUT;
	scsi_cmd->service_action = SET_TARGET_PORT_GROUPS;
	scsi_ulto4b(alloc_len, scsi_cmd->length);
}

/*
 * Syncronize the media to the contents of the cache for
 * the given lba/count pair.  Specifying 0/0 means sync
 * the whole cache.
 */
void
scsi_synchronize_cache(struct ccb_scsiio *csio, u_int32_t retries,
		       void (*cbfcnp)(struct cam_periph *, union ccb *),
		       u_int8_t tag_action, u_int32_t begin_lba,
		       u_int16_t lb_count, u_int8_t sense_len,
		       u_int32_t timeout)
{
	struct scsi_sync_cache *scsi_cmd;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/NULL,
		      /*dxfer_len*/0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);

	scsi_cmd = (struct scsi_sync_cache *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = SYNCHRONIZE_CACHE;
	scsi_ulto4b(begin_lba, scsi_cmd->begin_lba);
	scsi_ulto2b(lb_count, scsi_cmd->lb_count);
}

void
scsi_read_write(struct ccb_scsiio *csio, u_int32_t retries,
		void (*cbfcnp)(struct cam_periph *, union ccb *),
		u_int8_t tag_action, int readop, u_int8_t byte2,
		int minimum_cmd_size, u_int64_t lba, u_int32_t block_count,
		u_int8_t *data_ptr, u_int32_t dxfer_len, u_int8_t sense_len,
		u_int32_t timeout)
{
	int read;
	u_int8_t cdb_len;

	read = (readop & SCSI_RW_DIRMASK) == SCSI_RW_READ;

	/*
	 * Use the smallest possible command to perform the operation
	 * as some legacy hardware does not support the 10 byte commands.
	 * If any of the bits in byte2 is set, we have to go with a larger
	 * command.
	 */
	if ((minimum_cmd_size < 10)
	 && ((lba & 0x1fffff) == lba)
	 && ((block_count & 0xff) == block_count)
	 && (byte2 == 0)) {
		/*
		 * We can fit in a 6 byte cdb.
		 */
		struct scsi_rw_6 *scsi_cmd;

		scsi_cmd = (struct scsi_rw_6 *)&csio->cdb_io.cdb_bytes;
		scsi_cmd->opcode = read ? READ_6 : WRITE_6;
		scsi_ulto3b(lba, scsi_cmd->addr);
		scsi_cmd->length = block_count & 0xff;
		scsi_cmd->control = 0;
		cdb_len = sizeof(*scsi_cmd);

		CAM_DEBUG(csio->ccb_h.path, CAM_DEBUG_SUBTRACE,
			  ("6byte: %x%x%x:%d:%d\n", scsi_cmd->addr[0],
			   scsi_cmd->addr[1], scsi_cmd->addr[2],
			   scsi_cmd->length, dxfer_len));
	} else if ((minimum_cmd_size < 12)
		&& ((block_count & 0xffff) == block_count)
		&& ((lba & 0xffffffff) == lba)) {
		/*
		 * Need a 10 byte cdb.
		 */
		struct scsi_rw_10 *scsi_cmd;

		scsi_cmd = (struct scsi_rw_10 *)&csio->cdb_io.cdb_bytes;
		scsi_cmd->opcode = read ? READ_10 : WRITE_10;
		scsi_cmd->byte2 = byte2;
		scsi_ulto4b(lba, scsi_cmd->addr);
		scsi_cmd->reserved = 0;
		scsi_ulto2b(block_count, scsi_cmd->length);
		scsi_cmd->control = 0;
		cdb_len = sizeof(*scsi_cmd);

		CAM_DEBUG(csio->ccb_h.path, CAM_DEBUG_SUBTRACE,
			  ("10byte: %x%x%x%x:%x%x: %d\n", scsi_cmd->addr[0],
			   scsi_cmd->addr[1], scsi_cmd->addr[2],
			   scsi_cmd->addr[3], scsi_cmd->length[0],
			   scsi_cmd->length[1], dxfer_len));
	} else if ((minimum_cmd_size < 16)
		&& ((block_count & 0xffffffff) == block_count)
		&& ((lba & 0xffffffff) == lba)) {
		/* 
		 * The block count is too big for a 10 byte CDB, use a 12
		 * byte CDB.
		 */
		struct scsi_rw_12 *scsi_cmd;

		scsi_cmd = (struct scsi_rw_12 *)&csio->cdb_io.cdb_bytes;
		scsi_cmd->opcode = read ? READ_12 : WRITE_12;
		scsi_cmd->byte2 = byte2;
		scsi_ulto4b(lba, scsi_cmd->addr);
		scsi_cmd->reserved = 0;
		scsi_ulto4b(block_count, scsi_cmd->length);
		scsi_cmd->control = 0;
		cdb_len = sizeof(*scsi_cmd);

		CAM_DEBUG(csio->ccb_h.path, CAM_DEBUG_SUBTRACE,
			  ("12byte: %x%x%x%x:%x%x%x%x: %d\n", scsi_cmd->addr[0],
			   scsi_cmd->addr[1], scsi_cmd->addr[2],
			   scsi_cmd->addr[3], scsi_cmd->length[0],
			   scsi_cmd->length[1], scsi_cmd->length[2],
			   scsi_cmd->length[3], dxfer_len));
	} else {
		/*
		 * 16 byte CDB.  We'll only get here if the LBA is larger
		 * than 2^32, or if the user asks for a 16 byte command.
		 */
		struct scsi_rw_16 *scsi_cmd;

		scsi_cmd = (struct scsi_rw_16 *)&csio->cdb_io.cdb_bytes;
		scsi_cmd->opcode = read ? READ_16 : WRITE_16;
		scsi_cmd->byte2 = byte2;
		scsi_u64to8b(lba, scsi_cmd->addr);
		scsi_cmd->reserved = 0;
		scsi_ulto4b(block_count, scsi_cmd->length);
		scsi_cmd->control = 0;
		cdb_len = sizeof(*scsi_cmd);
	}
	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      (read ? CAM_DIR_IN : CAM_DIR_OUT) |
		      ((readop & SCSI_RW_BIO) != 0 ? CAM_DATA_BIO : 0),
		      tag_action,
		      data_ptr,
		      dxfer_len,
		      sense_len,
		      cdb_len,
		      timeout);
}

void
scsi_write_same(struct ccb_scsiio *csio, u_int32_t retries,
		void (*cbfcnp)(struct cam_periph *, union ccb *),
		u_int8_t tag_action, u_int8_t byte2,
		int minimum_cmd_size, u_int64_t lba, u_int32_t block_count,
		u_int8_t *data_ptr, u_int32_t dxfer_len, u_int8_t sense_len,
		u_int32_t timeout)
{
	u_int8_t cdb_len;
	if ((minimum_cmd_size < 16) &&
	    ((block_count & 0xffff) == block_count) &&
	    ((lba & 0xffffffff) == lba)) {
		/*
		 * Need a 10 byte cdb.
		 */
		struct scsi_write_same_10 *scsi_cmd;

		scsi_cmd = (struct scsi_write_same_10 *)&csio->cdb_io.cdb_bytes;
		scsi_cmd->opcode = WRITE_SAME_10;
		scsi_cmd->byte2 = byte2;
		scsi_ulto4b(lba, scsi_cmd->addr);
		scsi_cmd->group = 0;
		scsi_ulto2b(block_count, scsi_cmd->length);
		scsi_cmd->control = 0;
		cdb_len = sizeof(*scsi_cmd);

		CAM_DEBUG(csio->ccb_h.path, CAM_DEBUG_SUBTRACE,
			  ("10byte: %x%x%x%x:%x%x: %d\n", scsi_cmd->addr[0],
			   scsi_cmd->addr[1], scsi_cmd->addr[2],
			   scsi_cmd->addr[3], scsi_cmd->length[0],
			   scsi_cmd->length[1], dxfer_len));
	} else {
		/*
		 * 16 byte CDB.  We'll only get here if the LBA is larger
		 * than 2^32, or if the user asks for a 16 byte command.
		 */
		struct scsi_write_same_16 *scsi_cmd;

		scsi_cmd = (struct scsi_write_same_16 *)&csio->cdb_io.cdb_bytes;
		scsi_cmd->opcode = WRITE_SAME_16;
		scsi_cmd->byte2 = byte2;
		scsi_u64to8b(lba, scsi_cmd->addr);
		scsi_ulto4b(block_count, scsi_cmd->length);
		scsi_cmd->group = 0;
		scsi_cmd->control = 0;
		cdb_len = sizeof(*scsi_cmd);

		CAM_DEBUG(csio->ccb_h.path, CAM_DEBUG_SUBTRACE,
			  ("16byte: %x%x%x%x%x%x%x%x:%x%x%x%x: %d\n",
			   scsi_cmd->addr[0], scsi_cmd->addr[1],
			   scsi_cmd->addr[2], scsi_cmd->addr[3],
			   scsi_cmd->addr[4], scsi_cmd->addr[5],
			   scsi_cmd->addr[6], scsi_cmd->addr[7],
			   scsi_cmd->length[0], scsi_cmd->length[1],
			   scsi_cmd->length[2], scsi_cmd->length[3],
			   dxfer_len));
	}
	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_OUT,
		      tag_action,
		      data_ptr,
		      dxfer_len,
		      sense_len,
		      cdb_len,
		      timeout);
}

void
scsi_ata_identify(struct ccb_scsiio *csio, u_int32_t retries,
		  void (*cbfcnp)(struct cam_periph *, union ccb *),
		  u_int8_t tag_action, u_int8_t *data_ptr,
		  u_int16_t dxfer_len, u_int8_t sense_len,
		  u_int32_t timeout)
{
	scsi_ata_pass_16(csio,
			 retries,
			 cbfcnp,
			 /*flags*/CAM_DIR_IN,
			 tag_action,
			 /*protocol*/AP_PROTO_PIO_IN,
			 /*ata_flags*/AP_FLAG_TDIR_FROM_DEV|
				AP_FLAG_BYT_BLOK_BYTES|AP_FLAG_TLEN_SECT_CNT,
			 /*features*/0,
			 /*sector_count*/dxfer_len,
			 /*lba*/0,
			 /*command*/ATA_ATA_IDENTIFY,
			 /*control*/0,
			 data_ptr,
			 dxfer_len,
			 sense_len,
			 timeout);
}

void
scsi_ata_trim(struct ccb_scsiio *csio, u_int32_t retries,
	      void (*cbfcnp)(struct cam_periph *, union ccb *),
	      u_int8_t tag_action, u_int16_t block_count,
	      u_int8_t *data_ptr, u_int16_t dxfer_len, u_int8_t sense_len,
	      u_int32_t timeout)
{
	scsi_ata_pass_16(csio,
			 retries,
			 cbfcnp,
			 /*flags*/CAM_DIR_OUT,
			 tag_action,
			 /*protocol*/AP_EXTEND|AP_PROTO_DMA,
			 /*ata_flags*/AP_FLAG_TLEN_SECT_CNT|AP_FLAG_BYT_BLOK_BLOCKS,
			 /*features*/ATA_DSM_TRIM,
			 /*sector_count*/block_count,
			 /*lba*/0,
			 /*command*/ATA_DATA_SET_MANAGEMENT,
			 /*control*/0,
			 data_ptr,
			 dxfer_len,
			 sense_len,
			 timeout);
}

void
scsi_ata_pass_16(struct ccb_scsiio *csio, u_int32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 u_int32_t flags, u_int8_t tag_action,
		 u_int8_t protocol, u_int8_t ata_flags, u_int16_t features,
		 u_int16_t sector_count, uint64_t lba, u_int8_t command,
		 u_int8_t control, u_int8_t *data_ptr, u_int16_t dxfer_len,
		 u_int8_t sense_len, u_int32_t timeout)
{
	struct ata_pass_16 *ata_cmd;

	ata_cmd = (struct ata_pass_16 *)&csio->cdb_io.cdb_bytes;
	ata_cmd->opcode = ATA_PASS_16;
	ata_cmd->protocol = protocol;
	ata_cmd->flags = ata_flags;
	ata_cmd->features_ext = features >> 8;
	ata_cmd->features = features;
	ata_cmd->sector_count_ext = sector_count >> 8;
	ata_cmd->sector_count = sector_count;
	ata_cmd->lba_low = lba;
	ata_cmd->lba_mid = lba >> 8;
	ata_cmd->lba_high = lba >> 16;
	ata_cmd->device = ATA_DEV_LBA;
	if (protocol & AP_EXTEND) {
		ata_cmd->lba_low_ext = lba >> 24;
		ata_cmd->lba_mid_ext = lba >> 32;
		ata_cmd->lba_high_ext = lba >> 40;
	} else
		ata_cmd->device |= (lba >> 24) & 0x0f;
	ata_cmd->command = command;
	ata_cmd->control = control;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      flags,
		      tag_action,
		      data_ptr,
		      dxfer_len,
		      sense_len,
		      sizeof(*ata_cmd),
		      timeout);
}

void
scsi_unmap(struct ccb_scsiio *csio, u_int32_t retries,
	   void (*cbfcnp)(struct cam_periph *, union ccb *),
	   u_int8_t tag_action, u_int8_t byte2,
	   u_int8_t *data_ptr, u_int16_t dxfer_len, u_int8_t sense_len,
	   u_int32_t timeout)
{
	struct scsi_unmap *scsi_cmd;

	scsi_cmd = (struct scsi_unmap *)&csio->cdb_io.cdb_bytes;
	scsi_cmd->opcode = UNMAP;
	scsi_cmd->byte2 = byte2;
	scsi_ulto4b(0, scsi_cmd->reserved);
	scsi_cmd->group = 0;
	scsi_ulto2b(dxfer_len, scsi_cmd->length);
	scsi_cmd->control = 0;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_OUT,
		      tag_action,
		      data_ptr,
		      dxfer_len,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_receive_diagnostic_results(struct ccb_scsiio *csio, u_int32_t retries,
				void (*cbfcnp)(struct cam_periph *, union ccb*),
				uint8_t tag_action, int pcv, uint8_t page_code,
				uint8_t *data_ptr, uint16_t allocation_length,
				uint8_t sense_len, uint32_t timeout)
{
	struct scsi_receive_diag *scsi_cmd;

	scsi_cmd = (struct scsi_receive_diag *)&csio->cdb_io.cdb_bytes;
	memset(scsi_cmd, 0, sizeof(*scsi_cmd));
	scsi_cmd->opcode = RECEIVE_DIAGNOSTIC;
	if (pcv) {
		scsi_cmd->byte2 |= SRD_PCV;
		scsi_cmd->page_code = page_code;
	}
	scsi_ulto2b(allocation_length, scsi_cmd->length);

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_IN,
		      tag_action,
		      data_ptr,
		      allocation_length,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_send_diagnostic(struct ccb_scsiio *csio, u_int32_t retries,
		     void (*cbfcnp)(struct cam_periph *, union ccb *),
		     uint8_t tag_action, int unit_offline, int device_offline,
		     int self_test, int page_format, int self_test_code,
		     uint8_t *data_ptr, uint16_t param_list_length,
		     uint8_t sense_len, uint32_t timeout)
{
	struct scsi_send_diag *scsi_cmd;

	scsi_cmd = (struct scsi_send_diag *)&csio->cdb_io.cdb_bytes;
	memset(scsi_cmd, 0, sizeof(*scsi_cmd));
	scsi_cmd->opcode = SEND_DIAGNOSTIC;

	/*
	 * The default self-test mode control and specific test
	 * control are mutually exclusive.
	 */
	if (self_test)
		self_test_code = SSD_SELF_TEST_CODE_NONE;

	scsi_cmd->byte2 = ((self_test_code << SSD_SELF_TEST_CODE_SHIFT)
			 & SSD_SELF_TEST_CODE_MASK)
			| (unit_offline   ? SSD_UNITOFFL : 0)
			| (device_offline ? SSD_DEVOFFL  : 0)
			| (self_test      ? SSD_SELFTEST : 0)
			| (page_format    ? SSD_PF       : 0);
	scsi_ulto2b(param_list_length, scsi_cmd->length);

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/param_list_length ? CAM_DIR_OUT : CAM_DIR_NONE,
		      tag_action,
		      data_ptr,
		      param_list_length,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_read_buffer(struct ccb_scsiio *csio, u_int32_t retries,
			void (*cbfcnp)(struct cam_periph *, union ccb*),
			uint8_t tag_action, int mode,
			uint8_t buffer_id, u_int32_t offset,
			uint8_t *data_ptr, uint32_t allocation_length,
			uint8_t sense_len, uint32_t timeout)
{
	struct scsi_read_buffer *scsi_cmd;

	scsi_cmd = (struct scsi_read_buffer *)&csio->cdb_io.cdb_bytes;
	memset(scsi_cmd, 0, sizeof(*scsi_cmd));
	scsi_cmd->opcode = READ_BUFFER;
	scsi_cmd->byte2 = mode;
	scsi_cmd->buffer_id = buffer_id;
	scsi_ulto3b(offset, scsi_cmd->offset);
	scsi_ulto3b(allocation_length, scsi_cmd->length);

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_IN,
		      tag_action,
		      data_ptr,
		      allocation_length,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_write_buffer(struct ccb_scsiio *csio, u_int32_t retries,
			void (*cbfcnp)(struct cam_periph *, union ccb *),
			uint8_t tag_action, int mode,
			uint8_t buffer_id, u_int32_t offset,
			uint8_t *data_ptr, uint32_t param_list_length,
			uint8_t sense_len, uint32_t timeout)
{
	struct scsi_write_buffer *scsi_cmd;

	scsi_cmd = (struct scsi_write_buffer *)&csio->cdb_io.cdb_bytes;
	memset(scsi_cmd, 0, sizeof(*scsi_cmd));
	scsi_cmd->opcode = WRITE_BUFFER;
	scsi_cmd->byte2 = mode;
	scsi_cmd->buffer_id = buffer_id;
	scsi_ulto3b(offset, scsi_cmd->offset);
	scsi_ulto3b(param_list_length, scsi_cmd->length);

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/param_list_length ? CAM_DIR_OUT : CAM_DIR_NONE,
		      tag_action,
		      data_ptr,
		      param_list_length,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void 
scsi_start_stop(struct ccb_scsiio *csio, u_int32_t retries,
		void (*cbfcnp)(struct cam_periph *, union ccb *),
		u_int8_t tag_action, int start, int load_eject,
		int immediate, u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_start_stop_unit *scsi_cmd;
	int extra_flags = 0;

	scsi_cmd = (struct scsi_start_stop_unit *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = START_STOP_UNIT;
	if (start != 0) {
		scsi_cmd->how |= SSS_START;
		/* it takes a lot of power to start a drive */
		extra_flags |= CAM_HIGH_POWER;
	}
	if (load_eject != 0)
		scsi_cmd->how |= SSS_LOEJ;
	if (immediate != 0)
		scsi_cmd->byte2 |= SSS_IMMED;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_NONE | extra_flags,
		      tag_action,
		      /*data_ptr*/NULL,
		      /*dxfer_len*/0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}


/*      
 * Try make as good a match as possible with
 * available sub drivers
 */
int
scsi_inquiry_match(caddr_t inqbuffer, caddr_t table_entry)
{
	struct scsi_inquiry_pattern *entry;
	struct scsi_inquiry_data *inq;
 
	entry = (struct scsi_inquiry_pattern *)table_entry;
	inq = (struct scsi_inquiry_data *)inqbuffer;

	if (((SID_TYPE(inq) == entry->type)
	  || (entry->type == T_ANY))
	 && (SID_IS_REMOVABLE(inq) ? entry->media_type & SIP_MEDIA_REMOVABLE
				   : entry->media_type & SIP_MEDIA_FIXED)
	 && (cam_strmatch(inq->vendor, entry->vendor, sizeof(inq->vendor)) == 0)
	 && (cam_strmatch(inq->product, entry->product,
			  sizeof(inq->product)) == 0)
	 && (cam_strmatch(inq->revision, entry->revision,
			  sizeof(inq->revision)) == 0)) {
		return (0);
	}
        return (-1);
}

/*      
 * Try make as good a match as possible with
 * available sub drivers
 */
int
scsi_static_inquiry_match(caddr_t inqbuffer, caddr_t table_entry)
{
	struct scsi_static_inquiry_pattern *entry;
	struct scsi_inquiry_data *inq;
 
	entry = (struct scsi_static_inquiry_pattern *)table_entry;
	inq = (struct scsi_inquiry_data *)inqbuffer;

	if (((SID_TYPE(inq) == entry->type)
	  || (entry->type == T_ANY))
	 && (SID_IS_REMOVABLE(inq) ? entry->media_type & SIP_MEDIA_REMOVABLE
				   : entry->media_type & SIP_MEDIA_FIXED)
	 && (cam_strmatch(inq->vendor, entry->vendor, sizeof(inq->vendor)) == 0)
	 && (cam_strmatch(inq->product, entry->product,
			  sizeof(inq->product)) == 0)
	 && (cam_strmatch(inq->revision, entry->revision,
			  sizeof(inq->revision)) == 0)) {
		return (0);
	}
        return (-1);
}

/**
 * Compare two buffers of vpd device descriptors for a match.
 *
 * \param lhs      Pointer to first buffer of descriptors to compare.
 * \param lhs_len  The length of the first buffer.
 * \param rhs	   Pointer to second buffer of descriptors to compare.
 * \param rhs_len  The length of the second buffer.
 *
 * \return  0 on a match, -1 otherwise.
 *
 * Treat rhs and lhs as arrays of vpd device id descriptors.  Walk lhs matching
 * agains each element in rhs until all data are exhausted or we have found
 * a match.
 */
int
scsi_devid_match(uint8_t *lhs, size_t lhs_len, uint8_t *rhs, size_t rhs_len)
{
	struct scsi_vpd_id_descriptor *lhs_id;
	struct scsi_vpd_id_descriptor *lhs_last;
	struct scsi_vpd_id_descriptor *rhs_last;
	uint8_t *lhs_end;
	uint8_t *rhs_end;

	lhs_end = lhs + lhs_len;
	rhs_end = rhs + rhs_len;

	/*
	 * rhs_last and lhs_last are the last posible position of a valid
	 * descriptor assuming it had a zero length identifier.  We use
	 * these variables to insure we can safely dereference the length
	 * field in our loop termination tests.
	 */
	lhs_last = (struct scsi_vpd_id_descriptor *)
	    (lhs_end - __offsetof(struct scsi_vpd_id_descriptor, identifier));
	rhs_last = (struct scsi_vpd_id_descriptor *)
	    (rhs_end - __offsetof(struct scsi_vpd_id_descriptor, identifier));

	lhs_id = (struct scsi_vpd_id_descriptor *)lhs;
	while (lhs_id <= lhs_last
	    && (lhs_id->identifier + lhs_id->length) <= lhs_end) {
		struct scsi_vpd_id_descriptor *rhs_id;

		rhs_id = (struct scsi_vpd_id_descriptor *)rhs;
		while (rhs_id <= rhs_last
		    && (rhs_id->identifier + rhs_id->length) <= rhs_end) {

			if ((rhs_id->id_type &
			     (SVPD_ID_ASSOC_MASK | SVPD_ID_TYPE_MASK)) ==
			    (lhs_id->id_type &
			     (SVPD_ID_ASSOC_MASK | SVPD_ID_TYPE_MASK))
			 && rhs_id->length == lhs_id->length
			 && memcmp(rhs_id->identifier, lhs_id->identifier,
				   rhs_id->length) == 0)
				return (0);

			rhs_id = (struct scsi_vpd_id_descriptor *)
			   (rhs_id->identifier + rhs_id->length);
		}
		lhs_id = (struct scsi_vpd_id_descriptor *)
		   (lhs_id->identifier + lhs_id->length);
	}
	return (-1);
}

#ifdef _KERNEL
int
scsi_vpd_supported_page(struct cam_periph *periph, uint8_t page_id)
{
	struct cam_ed *device;
	struct scsi_vpd_supported_pages *vpds;
	int i, num_pages;

	device = periph->path->device;
	vpds = (struct scsi_vpd_supported_pages *)device->supported_vpds;

	if (vpds != NULL) {
		num_pages = device->supported_vpds_len -
		    SVPD_SUPPORTED_PAGES_HDR_LEN;
		for (i = 0; i < num_pages; i++) {
			if (vpds->page_list[i] == page_id)
				return (1);
		}
	}

	return (0);
}

static void
init_scsi_delay(void)
{
	int delay;

	delay = SCSI_DELAY;
	TUNABLE_INT_FETCH("kern.cam.scsi_delay", &delay);

	if (set_scsi_delay(delay) != 0) {
		printf("cam: invalid value for tunable kern.cam.scsi_delay\n");
		set_scsi_delay(SCSI_DELAY);
	}
}
SYSINIT(scsi_delay, SI_SUB_TUNABLES, SI_ORDER_ANY, init_scsi_delay, NULL);

static int
sysctl_scsi_delay(SYSCTL_HANDLER_ARGS)
{
	int error, delay;

	delay = scsi_delay;
	error = sysctl_handle_int(oidp, &delay, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	return (set_scsi_delay(delay));
}
SYSCTL_PROC(_kern_cam, OID_AUTO, scsi_delay, CTLTYPE_INT|CTLFLAG_RW,
    0, 0, sysctl_scsi_delay, "I",
    "Delay to allow devices to settle after a SCSI bus reset (ms)");

static int
set_scsi_delay(int delay)
{
	/*
         * If someone sets this to 0, we assume that they want the
         * minimum allowable bus settle delay.
	 */
	if (delay == 0) {
		printf("cam: using minimum scsi_delay (%dms)\n",
		    SCSI_MIN_DELAY);
		delay = SCSI_MIN_DELAY;
	}
	if (delay < SCSI_MIN_DELAY)
		return (EINVAL);
	scsi_delay = delay;
	return (0);
}
#endif /* _KERNEL */
