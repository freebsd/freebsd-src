/*
 * Copyright (c) 1992, 2000 Hellmuth Michaelis
 *
 * Copyright (c) 1992, 1995 Joerg Wunsch.
 *
 * Copyright (c) 1992, 1993 Brian Dunford-Shore and Holger Veit.
 *
 * Copyright (C) 1992, 1993 Soeren Schmidt.
 *
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
 * $FreeBSD: src/sys/i386/include/pcvt_ioctl.h,v 1.15 2000/03/31 08:29:21 hm Exp $
 * 
 *---------------------------------------------------------------------------*/

#ifndef	_MACHINE_PCVT_IOCTL_H_
#define	_MACHINE_PCVT_IOCTL_H_

/* pcvt version information for VGAPCVTID ioctl */

#define PCVTIDNAME    "pcvt"		/* driver id - string		*/
#define PCVTIDMAJOR   3			/* driver id - major release	*/
#define PCVTIDMINOR   60		/* driver id - minor release	*/

#if !defined(_KERNEL)
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

/*---------------------------------------------------------------------------*
 *		IOCTLs for MF II and AT Keyboards
 *---------------------------------------------------------------------------*/

#define KBDRESET	_IO('K',  1)	  /* reset keyboard / set defaults */
#define KBDGTPMAT	_IOR('K', 2, int) /* get current typematic value   */
#define KBDSTPMAT	_IOW('K', 3, int) /* set current typematic value   */

/*	Typematic Delay Values	*/

#define	KBD_TPD250	0x00			/*  250 ms	*/
#define	KBD_TPD500	0x20			/*  500 ms	*/
#define	KBD_TPD750	0x40			/*  750 ms	*/
#define	KBD_TPD1000	0x60			/* 1000 ms	*/

/*	Typematic Repeat Rate	*/

#define	KBD_TPM300	0x00			/* 30.0 char/second */
#define	KBD_TPM267	0x01			/* 26.7 char/second */
#define	KBD_TPM240	0x02			/* 24.0 char/second */
#define	KBD_TPM218	0x03			/* 21.8 char/second */
#define	KBD_TPM200	0x04			/* 20.0 char/second */
#define	KBD_TPM185	0x05			/* 18.5 char/second */
#define	KBD_TPM171	0x06			/* 17.1 char/second */
#define	KBD_TPM160	0x07			/* 16.0 char/second */
#define	KBD_TPM150	0x08			/* 15.0 char/second */
#define	KBD_TPM133	0x09			/* 13.3 char/second */
#define	KBD_TPM120	0x0A			/* 12.0 char/second */
#define	KBD_TPM109	0x0B			/* 10.9 char/second */
#define	KBD_TPM100	0x0C			/* 10.0 char/second */
#define	KBD_TPM92	0x0D			/*  9.2 char/second */
#define	KBD_TPM86	0x0E			/*  8.6 char/second */
#define	KBD_TPM80	0x0F			/*  8.0 char/second */
#define	KBD_TPM75	0x10			/*  7.5 char/second */
#define	KBD_TPM67	0x11			/*  6.7 char/second */
#define	KBD_TPM60	0x12			/*  6.0 char/second */
#define	KBD_TPM55	0x13			/*  5.5 char/second */
#define	KBD_TPM50	0x14			/*  5.0 char/second */
#define	KBD_TPM46	0x15			/*  4.6 char/second */
#define	KBD_TPM43	0x16			/*  4.3 char/second */
#define	KBD_TPM40	0x17			/*  4.0 char/second */
#define	KBD_TPM37	0x18			/*  3.7 char/second */
#define	KBD_TPM33	0x19			/*  3.3 char/second */
#define	KBD_TPM30	0x1A			/*  3.0 char/second */
#define	KBD_TPM27	0x1B			/*  2.7 char/second */
#define	KBD_TPM25	0x1C			/*  2.5 char/second */
#define	KBD_TPM23	0x1D			/*  2.3 char/second */
#define	KBD_TPM21	0x1E			/*  2.1 char/second */
#define	KBD_TPM20	0x1F			/*  2.0 char/second */

#define KBDGREPSW	_IOR('K', 4, int)	/* get key repetition switch */
#define KBDSREPSW	_IOW('K', 5, int)	/* set key repetition switch */
#define		KBD_REPEATOFF	0
#define		KBD_REPEATON	1

#define KBDGLEDS	_IOR('K', 6, int)	/* get LED state */
#define KBDSLEDS	_IOW('K', 7, int)	/* set LED state, does not influence */
#define 	KBD_SCROLLLOCK	0x0001		/* the driver's idea of lock key state */
#define		KBD_NUMLOCK	0x0002
#define		KBD_CAPSLOCK	0x0004
#define KBDGLOCK	_IOR('K', 8, int)	/* gets state of SCROLL,NUM,CAPS */
#define KBDSLOCK	_IOW('K', 9, int)	/* sets state of SCROLL,NUM,CAPS + LEDs */

#define KBDMAXOVLKEYSIZE	15		/* + zero byte */

struct kbd_ovlkey				/* complete definition of a key */
{
	u_short	keynum;				/* the key itself */
	u_short	type;				/* type of key, see below */
	u_char	subu;				/* subtype, ignored on write */
	char	unshift[KBDMAXOVLKEYSIZE+1];	/* emitted string, unshifted */
	u_char	subs;				/* subtype, ignored on write */
	char	shift[KBDMAXOVLKEYSIZE+1];	/* emitted string, shifted */
	u_char	subc;				/* subtype, ignored on write */
	char	ctrl[KBDMAXOVLKEYSIZE+1];	/* emitted string, control */
	u_char	suba;				/* subtype, ignored on write */
	char	altgr[KBDMAXOVLKEYSIZE+1];	/* emitted string, altgr */
};

/*	Max value for keynum field	*/

#define	KBDMAXKEYS	128		/* Max No. of Keys */

/*	Values for type field	*/

#define	KBD_NONE	0	/* no function, key is disabled */
#define	KBD_SHIFT	1	/* keyboard shift */
#define	KBD_META	2	/* alternate shift, sets bit8 to ASCII code */
#define	KBD_NUM		3	/* numeric shift, keypad num / appl */
#define	KBD_CTL		4	/* control code generation */
#define	KBD_CAPS	5	/* caps shift - swaps case of letter */
#define	KBD_ASCII	6	/* ascii code generating key */
#define	KBD_SCROLL	7	/* stop output */
#define	KBD_FUNC	8	/* function key */
#define	KBD_KP		9	/* keypad keys */
#define	KBD_BREAK	10	/* ignored */
#define	KBD_ALTGR	11	/* AltGr Translation feature */
#define	KBD_SHFTLOCK	12	/* shiftlock */
#define	KBD_CURSOR	13	/* cursor keys */
#define	KBD_RETURN	14	/* RETURN/ENTER keys */

/*	Values  for subtype field	*/

#define	KBD_SUBT_STR	0	/* key is bound to a string */
#define	KBD_SUBT_FNC	1	/* key is bound to a function */


#define	KBD_OVERLOAD	0x8000	/* Key is overloaded, ignored in ioctl */
#define	KBD_MASK	(~KBD_OVERLOAD)	/* mask for type */

#define KBDGCKEY	_IOWR('K',16, struct kbd_ovlkey)	/* get current key values */
#define KBDSCKEY	_IOW('K',17, struct kbd_ovlkey)		/* set new key assignment values*/
#define KBDGOKEY	_IOWR('K',18, struct kbd_ovlkey) 	/* get original key assignment values*/

#define KBDRMKEY	_IOW('K',19, int)	/* remove a key assignment */
#define KBDDEFAULT	_IO('K',20)		/* remove all key assignments */

/*---------------------------------------------------------------------------*
 *		IOCTLs for Video Adapter
 *---------------------------------------------------------------------------*/

/* Definition of PC Video Adaptor Types */

#define UNKNOWN_ADAPTOR	0	/* Unidentified adaptor ... */
#define MDA_ADAPTOR	1	/* Monochrome Display Adaptor/Hercules Graphics Card */
#define CGA_ADAPTOR	2	/* Color Graphics Adaptor */
#define EGA_ADAPTOR	3	/* Enhanced Graphics Adaptor */
#define VGA_ADAPTOR	4	/* Video Graphics Adaptor/Array */

/* Definitions of Monitor types */

#define MONITOR_MONO	0	/* Monochrome Monitor */
#define MONITOR_COLOR	1	/* Color Monitor */

/* Types of VGA chips detectable by current driver version */

#define VGA_F_NONE	0	/* FAMILY NOT KNOWN */
#define VGA_UNKNOWN	0	/* default, no 132 columns	*/

#define VGA_F_TSENG	1	/* FAMILY TSENG */
#define VGA_ET4000	1	/* Tseng Labs ET4000		*/
#define VGA_ET3000	2	/* Tseng Labs ET3000		*/

#define VGA_F_WD	2	/* FAMILY WD */
#define VGA_PVGA	3	/* Western Digital Paradise VGA	*/
#define VGA_WD90C00	4	/* Western Digital WD90C00	*/
#define VGA_WD90C10	5	/* Western Digital WD90C10	*/
#define VGA_WD90C11	6	/* Western Digital WD90C11	*/

#define VGA_F_V7	3	/* FAMILY VIDEO 7 */
#define VGA_V7VEGA	7	/* Video 7 VEGA VGA */
#define VGA_V7FWVR	8	/* Video 7 FASTWRITE/VRAM */
#define VGA_V7V5	9	/* Video 7 Version 5 */
#define VGA_V71024I	10	/* Video 7 1024i */
#define VGA_V7UNKNOWN	11	/* Video 7 unknown board .. */

#define VGA_F_TRI	4	/* FAMILY TRIDENT */
#define VGA_TR8800BR	12	/* Trident TVGA 8800BR */
#define VGA_TR8800CS	13	/* Trident TVGA 8800CS */
#define VGA_TR8900B	14	/* Trident TVGA 8900B  */
#define VGA_TR8900C	15	/* Trident TVGA 8900C  */
#define VGA_TR8900CL	16	/* Trident TVGA 8900CL */
#define VGA_TR9000	17	/* Trident TVGA 9000   */
#define VGA_TR9100	18	/* Trident TVGA 9100   */
#define VGA_TR9200	19	/* Trident TVGA 9200   */
#define VGA_TRUNKNOWN	20	/* Trident unknown     */

#define VGA_F_S3	5	/* FAMILY S3  */
#define VGA_S3_911	21	/* S3 911 */
#define VGA_S3_924	22	/* S3 924 */
#define VGA_S3_80x	23	/* S3 801/805 */
#define VGA_S3_928	24	/* S3 928 */
#define VGA_S3_UNKNOWN	25	/* unknown S3 chipset */

#define VGA_F_CIR	6	/* FAMILY CIRRUS */
#define VGA_CL_GD5402	26	/* Cirrus CL-GD5402	*/
#define VGA_CL_GD5402r1	27	/* Cirrus CL-GD5402r1	*/
#define VGA_CL_GD5420	28	/* Cirrus CL-GD5420	*/
#define VGA_CL_GD5420r1	29	/* Cirrus CL-GD5420r1	*/
#define VGA_CL_GD5422	30	/* Cirrus CL-GD5422	*/
#define VGA_CL_GD5424	31	/* Cirrus CL-GD5424	*/
#define VGA_CL_GD5426	32	/* Cirrus CL-GD5426	*/
#define VGA_CL_GD5428	33	/* Cirrus CL-GD5428	*/

/*****************************************************************************/
/* NOTE: update the 'scon' utility when adding support for more chipsets !!! */
/*****************************************************************************/

/* Definitions of Vertical Screen Sizes for EGA/VGA Adaptors */

#define SIZ_25ROWS	0	/* VGA: 25 lines, 8x16 font */
				/* EGA: 25 lines, 8x14 font */
#define SIZ_28ROWS	1	/* VGA: 28 lines, 8x14 font */
#define SIZ_35ROWS	2	/* EGA: 35 lines, 8x10 font */
#define SIZ_40ROWS	3	/* VGA: 40 lines, 8x10 font */
#define SIZ_43ROWS	4	/* EGA: 43 lines, 8x8  font */
#define SIZ_50ROWS	5	/* VGA: 50 lines, 8x8  font */

/* Definitions of Font Sizes for EGA/VGA Adaptors */

#define FNT_8x16	0	/* 8x16 Pixel Font, only VGA */
#define FNT_8x14	1	/* 8x14 Pixel Font, EGA/VGA  */
#define FNT_8x10	2	/* 8x10 Pixel Font, EGA/VGA  */
#define FNT_8x8		3	/* 8x8  Pixel Font, EGA/VGA  */

/* Definitions of Character Set (Font) Numbers */

#define CH_SET0		0	/* Character Set (Font) 0, EGA/VGA */
#define CH_SET1		1	/* Character Set (Font) 1, EGA/VGA */
#define CH_SET2		2	/* Character Set (Font) 2, EGA/VGA */
#define CH_SET3		3	/* Character Set (Font) 3, EGA/VGA */
#define CH_SETMAX_EGA	3	/* EGA has 4 Character Sets / Fonts */
#define CH_SET4		4	/* Character Set (Font) 4, VGA */
#define CH_SET5		5	/* Character Set (Font) 5, VGA */
#define CH_SET6		6	/* Character Set (Font) 6, VGA */
#define CH_SET7		7	/* Character Set (Font) 7, VGA */
#define CH_SETMAX_VGA	7	/* VGA has 8 Character Sets / Fonts */

/* Definitions of Terminal Emulation Modes */

#define M_HPVT		0	/* VTxxx and HP Mode, Labels & Status Line on */
#define M_PUREVT	1	/* only VTxxx Sequences recognized, no Labels */

/*---*/

#define VGACURSOR	_IOW('V',100, struct cursorshape) /* set cursor shape */

struct cursorshape {
	int screen_no;	   /* screen number for which to set,		    */
			   /*  or -1 to set on current active screen        */
	int start;	   /* top scanline, range 0... Character Height - 1 */
	int end;	   /* end scanline, range 0... Character Height - 1 */
};

#define VGALOADCHAR	_IOW('V',101, struct vgaloadchar) /* load vga char */

struct vgaloadchar {
	int character_set;	    /* VGA character set to load into */
	int character;		    /* Character to load */
	int character_scanlines;    /* Scanlines per character */
	u_char char_table[32];	    /* VGA character shape table */
};

#define VGASETFONTATTR	_IOW('V',102, struct vgafontattr) /* set font attr */
#define VGAGETFONTATTR	_IOWR('V',103, struct vgafontattr) /* get font attr */

struct vgafontattr {
	int character_set;	    /* VGA character set */
	int font_loaded;	    /* Mark font loaded or unloaded */
	int screen_size;	    /* Character rows per screen */
	int character_scanlines;    /* Scanlines per character - 1 */
	int screen_scanlines;       /* Scanlines per screen - 1 byte */
};

#define VGASETSCREEN	_IOW('V',104, struct screeninfo) /* set screen info */
#define VGAGETSCREEN	_IOWR('V',105, struct screeninfo) /* get screen info */

struct screeninfo {
	int adaptor_type;	/* type of video adaptor installed	*/
				/* read only, ignored on write		*/
	int monitor_type;	/* type of monitor (mono/color)installed*/
				/* read only, ignored on write		*/
	int totalfonts;		/* no of downloadable fonts		*/
				/* read only, ignored on write		*/
	int totalscreens;	/* no of virtual screens		*/
				/* read only, ignored on write		*/
	int screen_no;		/* screen number, this was got from	*/
				/* on write, if -1, apply pure_vt_mode	*/
				/* and/or screen_size to current screen */
				/* else to screen_no supplied		*/
	int current_screen;	/* screen number, which is displayed.	*/
				/* on write, if -1, make this screen	*/
				/* the current screen, else set current	*/
				/* displayed screen to parameter	*/
	int pure_vt_mode;	/* flag, pure VT mode or HP/VT mode	*/
				/* on write, if -1, no change		*/
	int screen_size;	/* screen size 				*/
				/* on write, if -1, no change		*/
	int force_24lines;	/* force 24 lines if 25 lines VT mode	*/
				/* or 28 lines HP mode to get pure	*/
				/* VT220 screen size			*/
				/* on write, if -1, no change		*/
	int vga_family;		/* if adaptor_type = VGA, this reflects */
				/* the chipset family after a read	*/
				/* nothing happens on write ...        */
	int vga_type;		/* if adaptor_type = VGA, this reflects */
				/* the chipset after a read		*/
				/* nothing happens on write ...        */
	int vga_132;		/* set to 1 if driver has support for	*/
				/* 132 column operation for chipset	*/
				/* currently ignored on write		*/
};

#define VGAREADPEL	_IOWR('V', 110, struct vgapel) /*r VGA palette entry */
#define VGAWRITEPEL	_IOW('V', 111, struct vgapel)  /*w VGA palette entry */

struct vgapel {
	unsigned idx;		/* index into palette, 0 .. 255 valid	*/
	unsigned r, g, b;	/* RGB values, masked by VGA_PMASK (63) */
};

/* NOTE: The next ioctl is only valid if option PCVT_SCREENSAVER is configured*/
/* this is *not* restricted to VGA's, but won't introduce new garbage...      */

#define VGASCREENSAVER	_IOW('V', 112, int)	/* set timeout for screen     */
						/* saver in seconds; 0 turns  */
						/* it off                     */

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

#define VGASETCOLMS	_IOW('V', 115, int) /* set number of columns (80/132)*/

/*
 * start of USL VT compatibility stuff (in case XSERVER defined)
 * these definitions must match those ones used by syscons
 *
 * Note that some of the ioctl command definitions below break the Berkeley
 * style. They accept a parameter of type "int" (instead of Berkeley style
 * "int *") in order to pass a single integer to the ioctl. These macros
 * below are marked with a dummy "int" comment. Dont blame anyone else
 * than USL for that braindeadness. It is done here to be a bit source-
 * level compatible to SysV. (N.B., within the ioctl, the argument is
 * dereferenced by "int *" anyway. The translation is done by the upper-
 * level ioctl stuff.)
 */

/*
 * NB: Some of the definitions below apparently override the definitions
 * in the KBD section above. But, due to BSDs encoding of the IO direction
 * and transfer size, the resulting ioctl cmds differ, so we can take them
 * here. The only real conflict would appear if we implemented KDGKBMODE,
 * which would be identical to KBDGLEDS above. Since this command is not
 * necessary for XFree86 2.0, we omit it.
 */

/* #define KDGKBMODE 	_IOR('K', 6, int) */ /* not yet implemented */

#define KDSKBMODE 	_IO('K', 7 /*, int */)
#define K_RAW		0		/* keyboard returns scancodes	*/
#define K_XLATE		1		/* keyboard returns ascii 	*/

#define KDMKTONE	_IO('K', 8 /*, int */)

/* #define KDGETMODE	_IOR('K', 9, int) */ /* not yet implemented */

#define KDSETMODE	_IO('K', 10 /*, int */)
#define KD_TEXT		0		/* set text mode restore fonts  */
#define KD_GRAPHICS	1		/* set graphics mode 		*/

/* we cannot see any sense to support KD_TEXT0 or KD_TEXT1 */

#define KDENABIO	_IO('K', 60) /* only allowed if euid == 0 */
#define KDDISABIO	_IO('K', 61)

#define KDGETLED	_IOR('K', 65, int)
#define KDSETLED	_IO('K', 66 /*, int */)
#define LED_CAP		1
#define LED_NUM		2
#define LED_SCR		4

#define KDSETRAD	_IO('K', 67 /*, int */)

/*
 * Note that since our idea of key mapping is much different from the
 * SysV style, we _only_ support mapping layers base (0), shifted (1),
 * alt (4), and alt-shift (5), and only for the basic keys (without
 * any function keys). This is what XFree86 2.0+ needs to establish
 * the default X keysym mapping.
 */
#define GIO_KEYMAP 	_IOR('k', 6, keymap_t)

#define VT_OPENQRY	_IOR('v', 1, int)
#define VT_SETMODE	_IOW('v', 2, vtmode_t)
#define VT_GETMODE	_IOR('v', 3, vtmode_t)

#define VT_RELDISP	_IO('v', 4 /*, int */)

/* acceptable values for the VT_RELDISP command */

#define VT_FALSE	0		/* release of VT refused */
#define VT_TRUE		1		/* VT released */
#define VT_ACKACQ	2		/* acknowledging VT acquisition */


#define VT_ACTIVATE	_IO('v', 5 /*, int */)
#define VT_WAITACTIVE	_IO('v', 6 /*, int */)
#define VT_GETACTIVE	_IOR('v', 7, int)

#ifndef _VT_MODE_DECLARED
#define	_VT_MODE_DECLARED
struct vt_mode {
	char	mode;

#define VT_AUTO		0		/* switching controlled by drvr	*/
#define VT_PROCESS	1		/* switching controlled by prog */

	char	waitv;			/* not implemented yet 	SOS	*/
	short	relsig;
	short	acqsig;
	short	frsig;			/* not implemented yet	SOS	*/
};

typedef struct vt_mode vtmode_t;
#endif /* !_VT_MODE_DECLARED */

#define NUM_KEYS	256
#define NUM_STATES	8

#ifndef _KEYMAP_DECLARED
#define	_KEYMAP_DECLARED
struct keyent_t {
	u_char map[NUM_STATES];
	u_char spcl;
	u_char flgs;
};

struct keymap {
	u_short	n_keys;
	struct keyent_t key[NUM_KEYS];
};

typedef struct keymap keymap_t;
#endif /* !_KEYMAP_DECLARED */

/* end of USL VT compatibility stuff */

#endif /* !_MACHINE_PCVT_IOCTL_H_ */
