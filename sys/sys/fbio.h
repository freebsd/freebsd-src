/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA
 * contract BG 91-66 and contributed to Berkeley.
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
 *	@(#)fbio.h	8.2 (Berkeley) 10/30/93
 *
 * $FreeBSD$
 */

#ifndef _SYS_FBIO_H_
#define _SYS_FBIO_H_

/*
 * Frame buffer ioctls (from Sprite, trimmed to essentials for X11).
 */

/*
 * Frame buffer type codes.
 */
#define	FBTYPE_SUN1BW		0	/* multibus mono */
#define	FBTYPE_SUN1COLOR	1	/* multibus color */
#define	FBTYPE_SUN2BW		2	/* memory mono */
#define	FBTYPE_SUN2COLOR	3	/* color w/rasterop chips */
#define	FBTYPE_SUN2GP		4	/* GP1/GP2 */
#define	FBTYPE_SUN5COLOR	5	/* RoadRunner accelerator */
#define	FBTYPE_SUN3COLOR	6	/* memory color */
#define	FBTYPE_MEMCOLOR		7	/* memory 24-bit */
#define	FBTYPE_SUN4COLOR	8	/* memory color w/overlay */

#define	FBTYPE_NOTSUN1		9	/* reserved for customer */
#define	FBTYPE_NOTSUN2		10	/* reserved for customer */
#define	FBTYPE_NOTSUN3		11	/* reserved for customer */

#define	FBTYPE_SUNFAST_COLOR	12	/* accelerated 8bit */
#define	FBTYPE_SUNROP_COLOR	13	/* MEMCOLOR with rop h/w */
#define	FBTYPE_SUNFB_VIDEO	14	/* Simple video mixing */
#define	FBTYPE_RESERVED5	15	/* reserved, do not use */
#define	FBTYPE_RESERVED4	16	/* reserved, do not use */
#define	FBTYPE_RESERVED3	17	/* reserved, do not use */
#define	FBTYPE_RESERVED2	18	/* reserved, do not use */
#define	FBTYPE_RESERVED1	19	/* reserved, do not use */

#define	FBTYPE_LASTPLUSONE	20	/* max number of fbs (change as add) */

/*
 * Frame buffer descriptor as returned by FBIOGTYPE.
 */
struct fbtype {
	int	fb_type;	/* as defined above */
	int	fb_height;	/* in pixels */
	int	fb_width;	/* in pixels */
	int	fb_depth;	/* bits per pixel */
	int	fb_cmsize;	/* size of color map (entries) */
	int	fb_size;	/* total size in bytes */
};
#define	FBIOGTYPE	_IOR('F', 0, struct fbtype)

#ifdef notdef
/*
 * General purpose structure for passing info in and out of frame buffers
 * (used for gp1) -- unsupported.
 */
struct fbinfo {
	int	fb_physaddr;	/* physical frame buffer address */
	int	fb_hwwidth;	/* fb board width */
	int	fb_hwheight;	/* fb board height */
	int	fb_addrdelta;	/* phys addr diff between boards */
	u_char	*fb_ropaddr;	/* fb virtual addr */
	int	fb_unit;	/* minor devnum of fb */
};
#define	FBIOGINFO	_IOR('F', 2, struct fbinfo)
#endif

/*
 * Color map I/O.
 */
struct fbcmap {
	int	index;		/* first element (0 origin) */
	int	count;		/* number of elements */
	u_char	*red;		/* red color map elements */
	u_char	*green;		/* green color map elements */
	u_char	*blue;		/* blue color map elements */
};
#define	FBIOPUTCMAP	_IOW('F', 3, struct fbcmap)
#define	FBIOGETCMAP	_IOW('F', 4, struct fbcmap)

/*
 * Set/get attributes.
 */
#define	FB_ATTR_NDEVSPECIFIC	8	/* no. of device specific values */
#define	FB_ATTR_NEMUTYPES	4	/* no. of emulation types */

struct fbsattr {
	int	flags;			/* flags; see below */
	int	emu_type;		/* emulation type (-1 if unused) */
	int	dev_specific[FB_ATTR_NDEVSPECIFIC];	/* catchall */
};
#define	FB_ATTR_AUTOINIT	1	/* emulation auto init flag */
#define	FB_ATTR_DEVSPECIFIC	2	/* dev. specific stuff valid flag */

struct fbgattr {
	int	real_type;		/* real device type */
	int	owner;			/* PID of owner, 0 if myself */
	struct	fbtype fbtype;		/* fbtype info for real device */
	struct	fbsattr sattr;		/* see above */
	int	emu_types[FB_ATTR_NEMUTYPES];	/* possible emulations */
						/* (-1 if unused) */
};
/*	FBIOSATTR	_IOW('F', 5, struct fbsattr) -- unsupported */
#define	FBIOGATTR	_IOR('F', 6, struct fbgattr)

/*
 * Video control.
 */
#define	FBVIDEO_OFF		0
#define	FBVIDEO_ON		1

#define	FBIOSVIDEO	_IOW('F', 7, int)
#define	FBIOGVIDEO	_IOR('F', 8, int)

/*
 * Hardware cursor control (for, e.g., CG6).  A rather complex and icky
 * interface that smells like VMS, but there it is....
 */
struct fbcurpos {
	short	x;
	short	y;
};

struct fbcursor {
	short	set;		/* flags; see below */
	short	enable;		/* nonzero => cursor on, 0 => cursor off */
	struct	fbcurpos pos;	/* position on display */
	struct	fbcurpos hot;	/* hot-spot within cursor */
	struct	fbcmap cmap;	/* cursor color map */
	struct	fbcurpos size;	/* number of valid bits in image & mask */
	caddr_t	image;		/* cursor image bits */
	caddr_t	mask;		/* cursor mask bits */
};
#define	FB_CUR_SETCUR	0x01	/* set on/off (i.e., obey fbcursor.enable) */
#define	FB_CUR_SETPOS	0x02	/* set position */
#define	FB_CUR_SETHOT	0x04	/* set hot-spot */
#define	FB_CUR_SETCMAP	0x08	/* set cursor color map */
#define	FB_CUR_SETSHAPE	0x10	/* set size & bits */
#define	FB_CUR_SETALL	(FB_CUR_SETCUR | FB_CUR_SETPOS | FB_CUR_SETHOT | \
			 FB_CUR_SETCMAP | FB_CUR_SETSHAPE)

/* controls for cursor attributes & shape (including position) */
#define	FBIOSCURSOR	_IOW('F', 24, struct fbcursor)
#define	FBIOGCURSOR	_IOWR('F', 25, struct fbcursor)

/* controls for cursor position only */
#define	FBIOSCURPOS	_IOW('F', 26, struct fbcurpos)
#define	FBIOGCURPOS	_IOW('F', 27, struct fbcurpos)

/* get maximum cursor size */
#define	FBIOGCURMAX	_IOR('F', 28, struct fbcurpos)

#endif
