/*-
 * Copyright (c) 1987, 1993
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
 *	@(#)vsio.h	8.1 (Berkeley) 6/2/93
 * $FreeBSD$
 */

#ifndef _SYS_VSIO_H_
#define _SYS_VSIO_H_

 /****************************************************************************
 *									    *
 *  Copyright (c) 1983, 1984 by						    *
 *  DIGITAL EQUIPMENT CORPORATION, Maynard, Massachusetts.		    *
 *  All rights reserved.						    *
 * 									    *
 *  This software is furnished on an as-is basis and may be used and copied *
 *  only with inclusion of the above copyright notice. This software or any *
 *  other copies thereof may be provided or otherwise made available to     *
 *  others only for non-commercial purposes.  No title to or ownership of   *
 *  the software is hereby transferred.					    *
 * 									    *
 *  The information in this software is  subject to change without notice   *
 *  and  should  not  be  construed as  a commitment by DIGITAL EQUIPMENT   *
 *  CORPORATION.							    *
 * 									    *
 *  DIGITAL assumes no responsibility for the use  or  reliability of its   *
 *  software on equipment which is not supplied by DIGITAL.		    *
 * 									    *
 *									    *
 ****************************************************************************/
/*
 * vsio.h - VS100 I/O command definitions
 *
 * Author:	Christopher A. Kent
 *		Digital Equipment Corporation
 *		Western Research Lab
 * Date:	Tue Jun 21 1983
 */

/*
 * Possible ioctl calls
 */

#define	VSIOINIT	_IO('V', 0)		/* init the device */
#define	VSIOSTART	_IOW('V', 1, int)	/* start microcode */
#define	VSIOABORT	_IO('V', 2)		/* abort a command chain */
#define	VSIOPWRUP	_IO('V', 3)		/* power-up reset */
#define	VSIOGETVER	_IOR('V', 4, int)	/* get rom version */
#define	VSIOSYNC	_IO('V', 6)		/* synch with device */
#define	VSIOBBACTL	_IOW('V', 8, int)	/* control the BBA */
#define	VSIOFIBCTL	_IOW('V', 9, int)	/* lamp on/off */
#define	VSIOFIBRETRY	_IOW('V',10, int)	/* fiber retries */
#define	VSIOGETSTATS	_IOR('V',11, vsStats)	/* get statistics */
#define	VSIOGETIOA	_IOR('V',13, vsIoAddrAddr)/* get ioreg address */
#define	VSIOUSERWAIT	_IO('V', 15)	/* wait for user I/O completion */
#define VSIOWAITGO	_IOW('V', 16, caddr_t)	/* wait then go */


#define	VSIO_OFF	0		/* option off */
#define	VSIO_ON		1		/* option on */

#define	VS_FIB_FINITE	1		/* finite retries */
#define	VS_FIB_INFINITE	2		/* infinite retries */

/*
 * Event queue entries
 */

typedef struct	_vs_event{
	u_short	vse_x;		/* x position */
	u_short	vse_y;		/* y position */
	u_short	vse_time;	/* 10 millisecond units (button only) */
	char	vse_type;	/* button or motion? */
	u_char	vse_key;	/* the key (button only) */
	char	vse_direction;	/* which direction (button only) */
	char	vse_device;	/* which device (button only) */
}vsEvent;

#define	VSE_BUTTON	0		/* button moved */
#define	VSE_MMOTION	1		/* mouse moved */
#define	VSE_TMOTION	2		/* tablet moved */

#define	VSE_KBTUP	0		/* up */
#define	VSE_KBTDOWN	1		/* down */

#define	VSE_MOUSE	1		/* mouse */
#define	VSE_DKB		2		/* main keyboard */
#define	VSE_TABLET	3		/* graphics tablet */
#define	VSE_AUX		4		/* auxiliary */
#define	VSE_CONSOLE	5		/* console */

typedef struct _vsStats{
	int	errors;			/* count errors */
	int	unsolIntr;		/* count unsolicited interrupts */
	int	overruns;		/* event queue overruns */
	int	flashes;		/* flashes on fiber link */
	int	ignites;		/* times turned on */
	int	douses;			/* times turned off */
	int	linkErrors;		/* link errors */
}vsStats;

typedef struct _vs_cursor{
	short x;
	short y;
}vsCursor;

typedef struct _vs_box {
	short bottom;
	short right;
	short left;
	short top;
}vsBox;

typedef struct _vsIoAddr {
	short	 *ioreg;
	short	 status;
	caddr_t  obuff;
	int	 obufflen;
	int	 reloc;
	vsEvent  *ibuff;
	int	 iqsize;		/* may assume power of 2 */
	int	 ihead;			/* atomic write */
	int	 itail;			/* atomic read */
	vsCursor mouse;			/* atomic read/write */
	vsBox	 mbox;			/* atomic read/write */
} vsIoAddr;
typedef vsIoAddr *vsIoAddrAddr;

#endif
