/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mtio.h	8.1 (Berkeley) 6/2/93
 * $Id: mtio.h,v 1.6 1996/01/08 12:26:15 joerg Exp $
 */

#ifndef _SYS_MTIO_H_
#define _SYS_MTIO_H_ 1

/*
 * Structures and definitions for mag tape io control commands
 */

/* structure for MTIOCTOP - mag tape op command */
struct mtop {
	short	mt_op;		/* operations defined below */
	daddr_t	mt_count;	/* how many of them */
};

/* operations */
#define MTWEOF		0	/* write an end-of-file record */
#define MTFSF		1	/* forward space file */
#define MTBSF		2	/* backward space file */
#define MTFSR		3	/* forward space record */
#define MTBSR		4	/* backward space record */
#define MTREW		5	/* rewind */
#define MTOFFL		6	/* rewind and put the drive offline */
#define MTNOP		7	/* no operation, sets status only */
#define MTCACHE		8	/* enable controller cache */
#define MTNOCACHE	9	/* disable controller cache */

#if defined(__FreeBSD__)
/* Set block size for device. If device is a variable size dev		*/
/* a non zero parameter will change the device to a fixed block size	*/
/* device with block size set to that of the parameter passed in.	*/
/* Resetting the block size to 0 will restore the device to a variable	*/
/* block size device. */

#define MTSETBSIZ	10

/* Set density values for device. They are defined in the SCSI II spec	*/
/* and range from 0 to 0x17. Sets the value for the openned mode only	*/

#define MTSETDNSTY	11

#define MTERASE		12	/* erase to EOM */
#define MTEOD		13	/* Space to EOM */
#define MTCOMP		14	/* select compression mode 0=off, 1=def */
#define MTRETENS	15	/* re-tension tape */

#endif

/* structure for MTIOCGET - mag tape get status command */

struct mtget {
	short	mt_type;	/* type of magtape device */
/* the following two registers are grossly device dependent */
	short	mt_dsreg;	/* ``drive status'' register */
	short	mt_erreg;	/* ``error'' register */
/* end device-dependent registers */
	short	mt_resid;	/* residual count */
#if defined (__FreeBSD__)
	daddr_t mt_blksiz;	/* presently operating blocksize */
	daddr_t mt_density;	/* presently operating density */
	daddr_t mt_comp;	/* presently operating compresion */
	daddr_t mt_blksiz0;	/* blocksize for mode 0 */
	daddr_t mt_blksiz1;	/* blocksize for mode 1 */
	daddr_t mt_blksiz2;	/* blocksize for mode 2 */
	daddr_t mt_blksiz3;	/* blocksize for mode 3 */
	daddr_t mt_density0;	/* density for mode 0 */
	daddr_t mt_density1;	/* density for mode 1 */
	daddr_t mt_density2;	/* density for mode 2 */
	daddr_t mt_density3;	/* density for mode 3 */
/* the following are not yet implemented */
	u_char	mt_comp0;	/* compression type for mode 0 */
	u_char	mt_comp1;	/* compression type for mode 1 */
	u_char	mt_comp2;	/* compression type for mode 2 */
	u_char	mt_comp3;	/* compression type for mode 3 */
#endif
	daddr_t	mt_fileno;	/* file number of current position */
	daddr_t	mt_blkno;	/* block number of current position */
/* end not yet implemented */
};

/*
 * Constants for mt_type byte.  These are the same
 * for controllers compatible with the types listed.
 */
#define	MT_ISTS		0x01		/* TS-11 */
#define	MT_ISHT		0x02		/* TM03 Massbus: TE16, TU45, TU77 */
#define	MT_ISTM		0x03		/* TM11/TE10 Unibus */
#define	MT_ISMT		0x04		/* TM78/TU78 Massbus */
#define	MT_ISUT		0x05		/* SI TU-45 emulation on Unibus */
#define	MT_ISCPC	0x06		/* SUN */
#define	MT_ISAR		0x07		/* SUN */
#define	MT_ISTMSCP	0x08		/* DEC TMSCP protocol (TU81, TK50) */
#define MT_ISCY		0x09		/* CCI Cipher */
#define MT_ISCT		0x0a		/* HP 1/4 tape */
#define MT_ISFHP	0x0b		/* HP 7980 1/2 tape */
#define MT_ISEXABYTE	0x0c		/* Exabyte */
#define MT_ISEXA8200	0x0c		/* Exabyte EXB-8200 */
#define MT_ISEXA8500	0x0d		/* Exabyte EXB-8500 */
#define MT_ISVIPER1	0x0e		/* Archive Viper-150 */
#define MT_ISPYTHON	0x0f		/* Archive Python (DAT) */
#define MT_ISHPDAT	0x10		/* HP 35450A DAT drive */
#define MT_ISMFOUR	0x11		/* M4 Data 1/2 9track drive */
#define MT_ISTK50	0x12		/* DEC SCSI TK50 */
#define MT_ISMT02	0x13		/* Emulex MT02 SCSI tape controller */

/* mag tape io control commands */
#define	MTIOCTOP	_IOW('m', 1, struct mtop)	/* do a mag tape op */
#define	MTIOCGET	_IOR('m', 2, struct mtget)	/* get tape status */
#define MTIOCIEOT	_IO('m', 3)			/* ignore EOT error */
#define MTIOCEEOT	_IO('m', 4)			/* enable EOT error */

#ifndef KERNEL
#define	DEFTAPE	"/dev/nrst0"
#endif

#ifdef	KERNEL
/*
 * minor device number
 */

#define	T_UNIT		003		/* unit selection */
#define	T_NOREWIND	004		/* no rewind on close */
#define	T_DENSEL	030		/* density select */
#define	T_800BPI	000		/* select  800 bpi */
#define	T_1600BPI	010		/* select 1600 bpi */
#define	T_6250BPI	020		/* select 6250 bpi */
#define	T_BADBPI	030		/* undefined selection */
#endif
#endif /* _SYS_MTIO_H_ */
