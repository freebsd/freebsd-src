/*
 * Copyright (c) 1992, 2000 Hellmuth Michaelis
 * Copyright (c) 1992, 1995 Joerg Wunsch.
 * Copyright (c) 1992, 1993 Brian Dunford-Shore and Holger Veit.
 * Copyright (C) 1992, 1993 Soeren Schmidt.
 * All rights reserved.
 *
 * For the sake of compatibility, portions of this code regarding the
 * X server interface are taken from Soeren Schmidt's syscons driver.
 *
 * This code is derived from software contributed to 386BSD by
 * Holger Veit.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	This product includes software developed by
 * 	Hellmuth Michaelis, Brian Dunford-Shore, Joerg Wunsch, Holger Veit
 *	and Soeren Schmidt.
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
/*---------------------------------------------------------------------------
 *
 *	pcvt_ioctl.h	ioctl's for the VT220 video driver 'pcvt'
 *	---------------------------------------------------------
 *
 *	Last Edit-Date: [Fri Mar 31 10:22:29 2000]
 *
 * $FreeBSD$
 * 
 *---------------------------------------------------------------------------*/

#ifndef	_MACHINE_PCVT_IOCTL_H_
#define	_MACHINE_PCVT_IOCTL_H_

/* pcvt version information for VGAPCVTID ioctl */

#define PCVTIDNAME    "pcvt"		/* driver id - string		*/
#define PCVTIDMAJOR   3			/* driver id - major release	*/
#define PCVTIDMINOR   60		/* driver id - minor release	*/

#include <sys/ioccom.h>

#define VGAPCVTID	_IOWR('V',113, struct pcvtid)	/* get driver id */

struct pcvtid {				/* THIS STRUCTURE IS NOW FROZEN !!! */
#define PCVTIDNAMELN  16		/* driver id - string length	*/
	char name[PCVTIDNAMELN];	/* driver name, == PCVTIDSTR	*/
	int rmajor;			/* revision number, major	*/
	int rminor;			/* revision number, minor	*/
};					/* END OF COLD PART ...		*/

#define VGAPCVTINFO	_IOWR('V',114, struct pcvtinfo)	/* get driver info */

struct pcvtinfo {			/* compile time option values */
	u_int nscreens;			/* PCVT_NSCREENS */
	u_int scanset;			/* PCVT_SCANSET */
	u_int updatefast;		/* PCVT_UPDATEFAST */
	u_int updateslow;		/* PCVT_UPDATESLOW */
	u_int sysbeepf;			/* PCVT_SYSBEEPF */
	u_int pcburst;			/* PCVT_PCBURST */
	u_int kbd_fifo_sz;		/* PCVT_KBD_FIFO_SZ */

/* config booleans */

	u_long compile_opts;		/* PCVT_xxxxxxxxxxxxxxx */

#define CONF_VT220KEYB		0x00000001
#define CONF_SCREENSAVER	0x00000002
#define CONF_PRETTYSCRNS	0x00000004
#define CONF_CTRL_ALT_DEL	0x00000008
#define CONF_USEKBDSEC		0x00000010
#define CONF_24LINESDEF		0x00000020
#define CONF_SHOWKEYS		0x00000040
#define CONF_NULLCHARS		0x00000080
#define CONF_SETCOLOR		0x00000100
#define CONF_132GENERIC		0x00000200
#define CONF_XSERVER		0x00000400
#define CONF_INHIBIT_NUMLOCK	0x00000800
#define CONF_META_ESC		0x00001000
#define CONF_SLOW_INTERRUPT	0x00002000
#define CONF_NO_LED_UPDATE	0x00004000
#define CONF_GREENSAVER		0x00008000
};

#endif /* !_MACHINE_PCVT_IOCTL_H_ */
