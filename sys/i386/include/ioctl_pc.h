/*      Copyright 1992,1993 by Holger Veit
 *	May be freely used with Bill Jolitz's port of 
 *	386bsd and may be included in a 386bsd collection
 *	as long as binary and source are available and reproduce the above
 *	copyright.
 *
 *	You may freely modify this code and contribute improvements based
 *	on this code as long as you don't claim to be the original author.
 *	Commercial use of this source requires permittance of the copyright 
 *	holder. A general license for 386bsd will override this restriction.
 *
 *	Use at your own risk. The copyright holder or any person who makes
 *	this code available for the public (administrators of public archives
 *	for instance) are not responsible for any harm to hardware or software
 *	that might happen due to wrong application or program faults.
 *
 * Addendum: The XFree86 developers and maintainers are hereby granted the 
 * right to distribute this file together with their source distributions
 * and patchkits of XFree86 without further explicit permission of the
 * above copyright holder. 
 * This and another file is a necessary include file for the unified 
 * pccons/codrv implementation of XFree86. This file is needed if
 * someone wants to compile an Xserver on a system which does not have, 
 * for some reasons, the codrv console driver which comes with this file. The
 * availability of this file avoids a large number of #ifdef's and
 * allows to make the xserver code easier runtime-configurable.
 * To make use of this file, it must be installed in /usr/include/sys.
 * This file is not the complete console device driver, so it is possible
 * that properties described in this file do not work without having the
 * complete driver distribution. This is not a fault of the Xserver that
 * was built with this file.
 *
 *
 *
 *	From: @(#)$RCSfile: ioctl_pc.h,v
 *	Revision: 1.1.1.1  (Contributed to 386bsd)
 *	Date: 1993/06/12 14:58:11
 *
 *	Important notice: #defined values are subject to be changed!!!
 *	Don't use the constant, use the name instead!
 *
 *	codrv1-style uses ioctls	'K': 1-33,255
 *					'V': 100-109
 *
 *	-hv-	Holger Veit, Holger.Veit@gmd.de
 *	-hm	Hellmuth Michaelis, hm@hcshh.hcs.de
 *      -vak-   Sergey Vakulenko, vak@kiae.su
 *
 *	25-07-92	-hv-	First version
 *	16-08-92	-hm	adding vga ioctl for cursor shape
 *      25-10-92	-hv-	X11 + video related ioctls
 *      01/12/92        -vak-   8x16 font loading, beep ioctl,
 *                              LED reassignment ioctl.
 *	22-04-93	-hv-	unified most CODRV1/CODRV2 codes
 *	24-04-93	-hv-	revised parts of keymap structures
 *
 *	$Id: ioctl_pc.h,v 1.1 1993/09/08 19:29:54 rgrimes Exp $
 */

#ifndef	_IOCTL_PC_H_
#define	_IOCTL_PC_H_

#ifdef NOTDEF
#if __GNUC__ >= 2
#pragma pack(1)
#endif
#endif

#ifndef KERNEL
#include <sys/ioctl.h>
#ifndef _TYPES_H_
#include <sys/types.h>
#endif
#else
#include "ioctl.h"
#endif


/***************************************************************************
 *   Basic definitions
 ***************************************************************************/

/* Use this data type when manipulating characters, don't use 'char' or 'u_char'
 * some day this will be changed to 'u_short' or 'u_long' size to allow
 * characters > 255
 */
typedef u_char	XCHAR;

/***************************************************************************
 *   driver identification
 ***************************************************************************/

/*
 * This defines the CONSOLE INFORMATION data structure, used to
 * describe console capabilities, to distinguish between different
 * versions. If this ioctl fail, you probably have an old style "pccons" 
 * driver (or an "improved" console driver, whose writer is not interested
 * in providing compatibility for anything).
 * In this case, a considerable number of features may not work as expected,
 * or do not work at all.
 */

#define MAXINFOSIZE	16
struct consinfo {
	u_long	info1;
	u_long	__reserved1__;
	u_long	__reserved2__;
	u_long	__reserved3__;
	XCHAR	drv_name[MAXINFOSIZE+1];
	XCHAR	emul_name[MAXINFOSIZE+1];
	XCHAR	__reserved1_name__[MAXINFOSIZE+1];
	XCHAR	__reserved2_name__[MAXINFOSIZE+1];
};

struct oldconsinfo {
	u_long	info1;
	u_long	__reserved__;
};

#define CONSGINFO	_IOR('K',255,struct consinfo)	/* Get console capabilities */
#define OLDCONSGINFO	_IOR('K',255,struct oldconsinfo) /* compatibility */
#define		CONS_ISPC	0x00000001	/* is derived from old PCCONS */
#define		CONS_ISCO	0x00000002	/* is derived from CO driver */
#define		CONS_reserved1	0x00000004	/* reserved for other console drivers */
#define		CONS_reserved2	0x00000008	/* reserved for other console drivers */
#define		CONS_HASKBD	0x00000010	/* has /dev/kbd */
#define		CONS_HASSCAN	0x00000020	/* uses Scan codes */
#define		CONS_HASKEYNUM	0x00000040	/* uses KEYNUMS */
#define		CONS_HASVTY	0x00000080	/* has /dev/vty* */
#define		CONS_HASPC3	0x00000100	/* unused, historical */
#define		CONS_HASVTHP	0x00000200	/* unused, historical */
#define		CONS_reserved3	0x00000400	/* reserved */
#define		CONS_reserved4	0x00000800	/* reserved */
#define		CONS_HASPX386	0x00001000	/* has X386 probing support +new CONSOLE_X_MODE */
#define		CONS_HASOX386	0x00002000	/* has old X386 support CONSOLE_X_MODE_ON/OFF */
#define		CONS_reserved5	0x00004000	/* reserved */
#define		CONS_reserved6	0x00008000	/* reserved */
#define		CONS_HASKCAP	0x00010000	/* has ioctl keycap support */
#define		CONS_HASFNT	0x00020000	/* has ioctl font support */
#define		CONS_reserved7	0x00040000	/* reserved */
#define		CONS_reserved8	0x00080000	/* reserved */
#define		CONS_USE7BIT	0x00100000	/* does not support 8bit characters */ 
#define		CONS_USEPC8	0x00200000	/* uses PC8 8-bit mapping */
#define		CONS_USELATIN1	0x00400000	/* uses ISO LATIN1 mapping */
#define		CONS_HAS10646	0x00800000	/* has /dev/unicode */
#define		CONS_PCCONS2	0x01000000	/* modified pccons */
#define		CONS_CODRV1	0x02000000	/* old codrv ioctls */
#define		CONS_CODRV2	0x04000000	/* codrv ioctls 0.1.2 */
#define		CONS_reserved9	0x08000000	/* reserved */
#define		CONS_reserved10	0x10000000	/* reserved */
#define		CONS_reserved11	0x20000000	/* reserved */
#define		CONS_reserved12	0x40000000	/* reserved */
#define		CONS_reserved13	0x80000000	/* reserved */


/***************************************************************************
 *   IOCTLs for AT Keyboard
 ***************************************************************************/

/**** initializing the keyboard ****/

/* reset keyboard, run selftests and set default values:
 * default keymap, no overloaded keys, default typematic rate
 * KBD_TPD500|KBD_TPM100, repetition on
 */
#define KBDCOLDRESET	_IO('K',  1)	/* reset keyboard and set default
					 * values:
					 * default keymap, no overloaded
					 * keys, default typematic rate
					 * KBD_TPD500|KBD_TPM100
					 */	
/* resets the mode in keyboard controller only */
#define	KBDWARMRESET	_IO('K', 23)



/**** key repetition (typematic) feature ****/

/* get (G) / set (S) key repetition rate and delay
 * see below for a definition of rate and delay and the necessary
 * argument
 */
#define KBDGTPMAT	_IOR('K', 2, int)
#define KBDSTPMAT	_IOW('K', 3, int)

/*   Typematic rates:
 *   Rate = 1 / Period, with 
 *   Period = (8+ (Val&7)) * 2^((Val>>3)&3) * 0.00417 seconds,
 *   and Val the typematic value below
 *
 *   The typematic delay is determined by
 *   Delay = (1+((Val>>5)&3)) * 250 msec +/- 20 %
 *
 *  Source IBM/AT reference manual, 1987
 *
 *  Note that you have to pass one TPD* and one TPM* value to the KBDSTPMAT
 *  ioctl: they are different flags of the same data word. Also note that
 *  0x00 is a valid value: KBD_TPD250|KBD_TPM300 which is really fast, instead
 *  of turning off key repetition entirely. You can turn off key repetition
 *  with the ioctls KBDGREPSW/KBDSREPSW.
*/

#define		KBD_TPD250	0x0000		/* 250 ms */
#define		KBD_TPD500	0x0020		/* 500 ms */
#define		KBD_TPD750	0x0040		/* 750 ms */
#define		KBD_TPD1000	0x0060		/* 1000 ms */

#define		KBD_TPM300	0x0000		/* 30.0 rate */
#define		KBD_TPM267	0x0001		/* 26.7 rate */
#define		KBD_TPM240	0x0002		/* 24.0 rate */
#define		KBD_TPM218	0x0003		/* 21.8 rate */
#define		KBD_TPM200	0x0004		/* 20.0 rate */
#define		KBD_TPM185	0x0005		/* 18.5 rate */
#define		KBD_TPM171	0x0006		/* 17.1 rate */
#define		KBD_TPM160	0x0007		/* 16.0 rate */
#define		KBD_TPM150	0x0008		/* 15.0 rate */
#define		KBD_TPM133	0x0009		/* 13.3 rate */
#define		KBD_TPM120	0x000a		/* 12.0 rate */
#define		KBD_TPM109	0x000b		/* 10.9 rate */
#define		KBD_TPM100	0x000c		/* 10.0 rate */
#define		KBD_TPM92	0x000d		/*  9.2 rate */
#define		KBD_TPM86	0x000e		/*  8.6 rate */
#define		KBD_TPM80	0x000f		/*  8.0 rate */
#define		KBD_TPM75	0x0010		/*  7.5 rate */
#define		KBD_TPM67	0x0011		/*  6.7 rate */
#define		KBD_TPM60	0x0012		/*  6.0 rate */
#define		KBD_TPM55	0x0013		/*  5.5 rate */
#define		KBD_TPM50	0x0014		/*  5.0 rate */
#define		KBD_TPM46	0x0015		/*  4.6 rate */
#define		KBD_TPM43	0x0016		/*  4.3 rate */
#define		KBD_TPM40	0x0017		/*  4.0 rate */
#define		KBD_TPM37	0x0018		/*  3.7 rate */
#define		KBD_TPM33	0x0019		/*  3.3 rate */
#define		KBD_TPM30	0x001a		/*  3.0 rate */
#define		KBD_TPM27	0x001b		/*  2.7 rate */
#define		KBD_TPM25	0x001c		/*  2.5 rate */
#define		KBD_TPM23	0x001d		/*  2.3 rate */
#define		KBD_TPM21	0x001e		/*  2.1 rate */
#define		KBD_TPM20	0x001f		/*  2.0 rate */


/* get (G) / set (S) the key repetition switch */
#define		KBD_REPEATOFF	0
#define		KBD_REPEATON	1
#define KBDGREPSW	_IOR('K', 4, int)
#define KBDSREPSW	_IOW('K', 5, int)



/**** handling keyboard LEDS and Lock keys ****/

/* get (G) / set (S) the keyboard LEDs,
 * does not influence the state of the lock keys.
 * Note: if keyboard serves tty console mode (VTYs have keyboard focus),
 * the lock keys will still modify the state when used
 */
#define KBDGLEDS	_IOR('K', 6, int)
#define KBDSLEDS	_IOW('K', 7, int)

/* get (G) / set (S) the SCROLL, NUM, CAPS ALTGRLOCK keys
 * (note: ALTGRLOCK or SHIFTLOCK are not necessarily accessible 
 * on your keyboard)
 */
#define 	KBD_LOCKSCROLL	0x0001
#define		KBD_LOCKNUM	0x0002
#define		KBD_LOCKCAPS	0x0004
#define		KBD_LOCKALTGR	0x0008
#define		KBD_LOCKSHIFT	0x0010
#define KBDGLOCK	_IOR('K', 8, int)
#define KBDSLOCK	_IOW('K', 9, int)



/**** making noise ****/

/* get (G) / set (S) the beeper frequency and tone duration
 * the nr param determines the VTY which parameters are changed
 * VTY# = 0...n, n < max_vtys
 * nr = -1: actual vty
 * nr = -2: Set the system default beep frequency
 *
 * in some emulations, you can also set pitch and duration by an ESC code
 */
#define		KBD_ACTVTY	-1
#define		KBD_DEFLT	-2
struct kbd_bell {
	int	pitch;
	int	duration;
	int	nr;	
};

#define KBDGETBEEP      _IOWR('K',28, struct kbd_bell)
#define KBDSETBEEP      _IOW('K',29, struct kbd_bell)

/* do a beep of specified frequency and duration
 * the argument nr is unused
 * a NULL arg performs a default system beep
 */
#define KBDBELL		_IOW('K',30, struct kbd_bell)



/**** I/O access ****/

/* This call allows programs to access I/O ports. 
 * The ioctl is intended to perform several tasks for the XFree86 Xserver,
 * but currently has other interesting applications. This is why it is
 * priviledged and can only be executed by root (or with setuid-root).
 * In future the ioctl might be restricted to allow access to video ports
 * only.
 */
#define		X_MODE_ON	1
#define		X_MODE_OFF	0
#define CONSOLE_X_MODE	_IOW('K',22,int)


/**** keyboard overloading ****/

/* Codrv allows loading of strings to keys in six layers.
 * Any string may have a length of up to KBDMAXOVLKEYSIZE XCHARS.
 * !!! Warning: This ioctl uses the type XCHAR. In future, this may
 * !!! no longer be a char type, so str*** functions might not work any more
 * !!! some day.
 * The available layers are:
 *
 * - unshifted
 * - with shift key
 * - with ctrl key
 * - with meta key (usually ALT-left)
 * - with altgr key (usually ALT-right)
 * - with shift+altgr key
 *
 * There are no combinations: shift-ctrl, ctrl-alt, shift-meta.
 * The combination ctrl-altleft-somekey is reserved for system purposes.
 * These keys are usually processed before the above keys. To gain control
 * over these keys, you must run the keyboard in raw mode (/dev/kbd) and
 * do ALL the processing yourself. The Xserver for instance does it this way.
 * The following special keys are currently defined:
 * 
 * CTRL-ALTLEFT-DELETE:	Reboot
 * CTRL-ALTLEFT-ESCAPE: Call the debugger (if compiled into the kernel)
 * CTRL-ALTLEFT-KP+:	Switch to next resolution (Xserver only)
 * CTRL-ALTLEFT-KP-:	Switch to previous resolution (Xserver only)
 */

/* values for type field of various kbd_overload ioctls */
#define		KBD_NONE	0	/* no function, key is disabled */
#define		KBD_SHIFT	1	/* keyboard shift */
#define		KBD_META	2	/* (ALT) alternate shift, sets bit8 to ASCII code */
#define		KBD_NUM		3	/* numeric shift  cursors vs. numeric */
#define		KBD_CTL		4	/* control shift  -- allows ctl function */
#define		KBD_CAPS	5	/* caps shift -- swaps case of letter */
#define		KBD_ASCII	6	/* ascii code for this key */
#define		KBD_SCROLL	7	/* stop output */
#define		KBD_FUNC	8	/* function key */
#define		KBD_KP		9	/* Keypad keys */
#define 	KBD_BREAK	10	/* The damned BREAK key, ignored in ioctl */
#define 	KBD_ALTGR	11	/* AltGr Translation feature */
#define		KBD_SHFTLOCK	12	/* some people are accustomed to this nonsense */
#define         KBD_ALTGRLOCK   13      /* Useful for 8-bit national kbds (cyrillic) */
#define         KBD_DOALTCAPS   0x0400  /* change by altgr + caps shift */
#define         KBD_DOCAPS      0x0800  /* change by caps shift */
#define 	KBD_DIACPFX	0x4000	/* Key carries a diacritical prefix */
#define 	KBD_OVERLOAD	0x8000	/* Key is overloaded, ignored in ioctl */
#define		KBD_MASK	0x001f	/* mask for type */ 

#define KBDMAXOVLKEYSIZE	15	/* excl. zero byte */
struct kbd_ovlkey {
	u_short	keynum;
	u_short	type;
	XCHAR	unshift[KBDMAXOVLKEYSIZE+1];
	XCHAR	shift[KBDMAXOVLKEYSIZE+1];
	XCHAR	ctrl[KBDMAXOVLKEYSIZE+1];
	XCHAR	meta[KBDMAXOVLKEYSIZE+1];
	XCHAR	altgr[KBDMAXOVLKEYSIZE+1];
	XCHAR	shiftaltgr[KBDMAXOVLKEYSIZE+1];
};


/* Get (G) / Set (S) a key assignment. This will influence the current
 * key value only
 */
#define KBDGCKEY	_IOWR('K',16, struct kbd_ovlkey)
#define KBDSCKEY	_IOW('K',17, struct kbd_ovlkey)

/* Get (G) the default (old) key assignment. You cannot overwrite the
 * default setting, so this ioctl is unpaired
 */
#define KBDGOKEY	_IOWR('K',18, struct kbd_ovlkey)



/* Remove a key assignment for a key, i.e. restore default setting for key
 * arg = keynum
 */
#define KBDRMKEY	_IOW('K', 19, int)

/* Restore the default key setting */
#define KBDDEFAULT	_IO('K',20)



/* Set behavior of unassigned key layers
 * Note that there is a hack from further versions which uses
 * the flags KBD_C0 and KBD_A0 for this. This is still supported, but
 * is not recommended way to do. It may disappear in future
 * (what means that it won't :-))
 */
#define		KBD_CLEARCTRL	2
#define		KBD_CLEARMETA	4
#define		KBD_CLEARALT	1
#ifdef notyet
	#define		KBD_CLEARNORM	8
	#define		KBD_CLEARSHIFT	16
	#define		KBD_CLEARSHALT	32
#endif
#define	KBDSCLRLYR	_IOW('K',31,int)

/* get (G) / set (S) CAPSLOCK LED behaviour.
 * Not all of this keys may be accessible at your keyboard
 * Note: For compatibility, the S ioctl returns the old state in arg
 */
#define		KBD_CAPSCAPS	0	/* LED follows CAPSLOCK state */
#define		KBD_CAPSSHIFT	1	/* LED follows SHIFTLOCK state */
#define		KBD_CAPSALTGR	2	/* LED follows ALTGRLOCK state */
#define		KBD_CAPSINIT	0x04	/* bit to set to set a default for all VTYs */
#define KBDGCAPSLED	_IOR('K',27,int)
#define KBDSCAPSLED	_IOWR('K',25,int)

/* extended functions: functions that are triggered by a keypress
 * before key is converted to ASCII
 *
 * use function KBD_HOTKEYDELETE to remove a hotkey from a key
 */
struct kbd_hotkey {
	u_short	key;
	u_short modifier;
	u_short	function;
};
#define KBDGSPECF	_IOWR('K',32,struct kbd_hotkey)
#define KBDSSPECF	_IOW('K',33,struct kbd_hotkey)

/* extended function prefixes (in modifier field)
 * 	bit set triggers a special function on the key layer
 */
#define 	KBD_NOEXT	0x00	/* trigger never */
#define		KBD_EXT_N	0x01	/* on normal key (normal layer) */
#define		KBD_EXT_S	0x02	/* on shift key (shift layer) */
#define		KBD_EXT_C	0x04	/* on ctrl key (ctrl layer) */
#define		KBD_EXT_A	0x08	/* on alt key (alt layer) */
#define		KBD_EXT_SK	0x10	/* on syskey (PRINTSCREEN) (Meta Layer) */
#define		KBD_EXT_CA	0x20	/* on ctrl-alt (shift alt layer) */

/* extended functions (in function field) */
#define		KBD_VTY0	0	/* select vty 0 */
#define		KBD_VTY1	1	/* select vty 1 */
#define		KBD_VTY2	2	/* select vty 2 */
#define		KBD_VTY3	3	/* select vty 3 */
#define		KBD_VTY4	4	/* select vty 4 */
#define		KBD_VTY5	5	/* select vty 5 */
#define		KBD_VTY6	6	/* select vty 6 */
#define		KBD_VTY7	7	/* select vty 7 */
#define		KBD_VTY8	8	/* select vty 8 */
#define		KBD_VTY9	9	/* select vty 9 */
#define		KBD_VTY10	10	/* select vty 10 */
#define		KBD_VTY11	11	/* select vty 11 */
#define		KBD_VTYUP	0x80	/* select next vty */
#define		KBD_VTYDOWN	0x81	/* select previous vty */
#define		KBD_RESETKEY	0x82	/* the CTRL-ALT-DEL key (movable) */
#define		KBD_DEBUGKEY	0x83	/* the CTRL-ALT-ESC key (debugger) */

#define		KBD_HOTKEYDELETE 0xff	/* use to delete a hotkey KBDSSPECF */



/* These are names used in older versions of keycap/codrv */
/* do not use the following functions any longer in future */
#ifdef COMPAT_CO011
#define KBDRESET	KBDCOLDRESET
#define KBDRESET8042	KBDWARMRESET
#define KBDFORCEASCII	_IOW('K', 24, int)	/* no op in codrv-0.1.2 */
#define 	KBD_SCROLLLOCK	KBD_LOCKSCROLL
#define		KBD_NUMLOCK	KBD_LOCKNUM
#define		KBD_CAPSLOCK	KBD_LOCKCAPS
#define KBDASGNLEDS     KBDSCAPSLED
#ifndef KERNEL
struct kbd_sound {
	int pitch;	/* Frequency in Hz */
	int duration;	/* Time in msec */
};
#endif
#define KBDSETBELL	_IOW('K',21, struct kbd_sound)	/* do some music */
#define OLDKBDSETBEEP      _IOW('K',26, struct kbd_sound) /* change beep settings */

struct oldkbd_ovlkey {
	u_short	keynum;
	u_short	type;
	char	unshift[KBDMAXOVLKEYSIZE+1];
	char	shift[KBDMAXOVLKEYSIZE+1];
	char	ctrl[KBDMAXOVLKEYSIZE+1];
	char	altgr[KBDMAXOVLKEYSIZE+1];
};
#define OLDKBDGCKEY	_IOWR('K',16, struct oldkbd_ovlkey)	/* get current key values */



#endif /*COMPAT_CO011*/

/***************************************************************************
 *   IOCTLs for Video Adapter
 ***************************************************************************/

/* to define the cursor shape for ioctl */
struct cursorshape {
	int start;	/* topmost scanline, range 0...31 */
	int end;	/* bottom scanline, range 0...31 */
};

#define VGAGCURSOR	_IOR('V',100, struct cursorshape) /* get cursor shape */
#define VGASCURSOR	_IOW('V',101, struct cursorshape) /* set cursor shape */



/**** information ****/

/* the video information structure for ioctl */
struct videoinfo {
	char	name[20];	/* ASCIZ name of detected card */
	short	type;		/* Adapter type, see below */
	short	subtype;	/* Adapter specific subtype */
	short	ram;		/* in KBytes */
	short	iobase;		/* Address of 6845: 0x3b0 / 0x3d0 */
};

/* Get information about the videoboard */
#define VGAGINFO	_IOR('V',102, struct videoinfo)

/* recognized Adapter types */
#define		VG_UNKNOWN	0
#define		VG_MONO		1
#define		VG_CGA		2
#define		VG_EGA		3
#define		VG_VGA		4
#define		VG_CHIPS	5
/*	CHIPS & TECHNOLOGIES has subtypes:
 *		0x10	82c451
 *		0x11	82c452
 *		0x20	82c455
 *		0x30	82c453
 *		0x50	82c455
 */
#define		VG_GENOA	6
/*	GENOA has subtypes:
 *		0x33/0x55	5100-5400, ET3000 based
 *		0x22		6100
 *		0x00		6200,6300
 *		0x11		6400,6600
 */
#define		VG_PARADISE	7
/*	PARADISE has subtypes:
 *		01	PVGA1A,WD90C90
 *		02	WD90C00
 *		03	WD90C10
 *		04	WD90C11
 */
#define		VG_TVGA		8
/*	TVGA	has subtypes:
 *		00-02	8800
 *		03	8900B
 *		04	8900C
 *		13	8900C
 *		23	9000
 */
#define		VG_ET3000	9
#define		VG_ET4000	10
#define		VG_VIDEO7	11
/*	VIDEO7  has subtypes:
 *		0x80-0xfe	VEGA VGA
 *		0x70-0x7e	V7VGA FASTWRITE/VRAM
 *		0x50-0x59	V7VGA version 5
 *		0x41-0x49	1024i
 */
#define		VG_ATI		12
/*	ATI	has subtypes:
 *		0x01nn	18800
 *		0x02nn	18800-1
 *		0x03nn	28800-2
 *		0x04nn-05nn
 *	with nn:
 *		0x01	VGA WONDER
 *		0x02	EGA WONDER800+
 *		0x03	VGA BASIC 16+
 */	



/**** Screen blanking ****/

/* Get (G) / Set (S) screen blanker timeout (seconds),
 * time=0 disables blanking
 *
 * The blanking state is coded in bits 31 and 30 of word returned by get
 */
#define		VGA_BLANKOFF	0x00000000	/* display is on, no blanking */
#define		VGA_BLANKON	0x40000000	/* display is on, wait for blank */
#define		VGA_BLANKED	0x80000000	/* display is dark */
#define	VGAGBLANK	_IOR('V',2,int)
#define VGASBLANK	_IOW('V',3,int)



/**** Text/Attribute direct access, block move ****/

struct vga_block {
	short	mode;
	short	pagenum;
	short	x0,y0;		/* upper left coordinates 0..x-1, 0..y-1 */
	short	x1,y1;		/* lower right coordinates >= x0,y0 */
	u_char	*map;		/* must be allocated by user process ! */
};

/* mode word */
#define		VGA_SCREEN	0x01	/* entire screen, ignore x,y */
#define		VGA_WINDOW	0x02	/* use x,y for a rectangular window */
#define		VGA_TEXT	0x10	/* copy text information only */
#define		VGA_ATTR	0x20	/* copy attribute information only */
#define		VGA_BOTH	0x30	/* copy text and attribute */
#define		VGA_ALL		0x31	/* copy complete screen */

/* Get (G) / Set (S) a rectangular block of screen
 * The virtual screen need not be visible.
 * The buffer must be provided by the user process and must be large enough
 * use VGAGVRES to find out how many bytes
 * pagenum: 0..n, n < max_vty, VTY number
 *	    -1, actual VTY
 */
#define VGAGBLOCK	_IOWR('V',4,struct vga_block)
#define VGASBLOCK	_IOW('V',5,struct vga_block)



#define VGA_TXTPAGE0	0
#define VGA_TXTPAGE1	1
#ifdef notyet
#define VGA_GFXPAGE	2
#endif
#define	VGA_PC8CODING	0x80	/* obsolete ! */

/* maximum dimension of pixels
 * Note: this is the space reserved in the fontchar map, but
 * does not mean, that this resolution is accepted in the current release
 * codrv-0.1.2 accepts 8x16 / "9x16" fonts only
 */
#define	VGA_MAXX	16
#define VGA_MAXY	16

struct fchar {
	XCHAR	encoding;	/* encoding of character */
	char	_f1_,_f2_,_f3_;	/* filler */
	u_char	map[VGA_MAXX/8*VGA_MAXY];
};

struct fmap {
	short	page;	/* page to load */
	short	nr;	/* nr of characters to load */
	char	x,y;	/* x,y pixel width */
	XCHAR	start;	/* first character in sequence (get only) */
	struct fchar *fntmap;	/* allocated by user process */
};

/* get (G) / set (S) font map. Must provide page,nr,start for get */
#define	VGAGFONTMAP	_IOWR('V',6,struct fmap)
#define	VGASFONTMAP	_IOW('V',7,struct fmap)



/* do not use the following functions any longer in future */
#ifdef COMPAT_CO011
/* miscellaneous functions: */
#define VGA_DIS1	1	/* disable font 1 */
#define VGA_GTENC	2	/* get current encoding */
#define VGA_SBLANK	3	/* set screen blanking timeout (use VGASBLANK!) */
#define VGA_GBLANK	4	/* get screen blanking timeout (use VGAGBLANK!) */

struct miscfcns {
	u_char	cmd;
	union {
		short	enc[2];	
		int	timeout;
	} u;
};	
#define	VGAMISCFCNS	_IOWR('V',107,struct miscfcns)	/* misc functions */


/* Font mapping this needs at least an EGA card (else EINVAL) */
#define 	VGAFNTLATIN1	0x00
#define		VGAFNTEXTEND1	0x01
#define		VGAFNTEXTEND2	0x02
#define		VGAFNTGREEK	0x03
#define		VGAFNTCYRILLIC	0x04
#define		VGAFNTHEBREW	0x05
#define		VGAFNTARABIAN	0x06

#define VGA_FNTNCHARS	256
#define VGA_FNTCSIZE    15

struct fontchar {
	u_char	page;		/* which font page */
	u_char	idx;		/* which char in font page */
	u_char	cmap[VGA_FNTCSIZE];	/* character bitmap */
};

#define OLDVGAGCHAR	_IOWR('V',105,struct fontchar) /* get character of font */
#define OLDVGASCHAR	_IOW('V',106,struct fontchar)	/* set character in font */

struct fontmap {
	u_char	page;		/* page to load */
	u_short	encoding;	/* font encoding */
	u_char	map[VGA_FNTNCHARS*VGA_FNTCSIZE];
};

#define	OLDVGAGFNTMAP	_IOWR('V',103,struct fontmap)	/* get font */
#define VGAGFNTMAP	OLDVGAGFNTMAP
#define	OLDVGASFNTMAP	_IOW('V',104,struct fontmap)	/* set font */
#define VGASFNTMAP	OLDVGASFNTMAP

#endif




struct textpage {
	u_char	pagenum;	/* note: only page 0 used by vtys */
#define	VGA_TEXTATTR	0
#define	VGA_TEXTDATA	1
	u_char	ad;
#define	VGA_LINES	50	/* only 25 used for now */
#define	VGA_COLUMNS	80
	u_char	map[VGA_LINES*VGA_COLUMNS];
};

#define VGAGPAGE	_IOWR('V',108,struct textpage)	/* get a data page */
#define VGASPAGE	_IOW('V',109,struct textpage)	/* set a data page */

/**** Signalling access ****/

/* Use "take control" in an application program to signal the kernel
 * that the program wants to use video memory (such as Xserver)
 * before the program switches modes
 *
 * Use "give control" to return the control to the kernel. The application
 * should have restored the original state before giving back control.
 * Close /dev/vga also returns control.
 *
 * However, the kernel remains the master in the house, and reserves the right
 * to grab control back at any time. (It usually doesn't).
 *
 */
#define	VGATAKECTRL	_IO('V',8)
#define VGAGIVECTRL	_IO('V',9)

/***************************************************************************
 *   Pandora's box, don't even think of using the following ioctl's
 *   (if you happen to find some; codrv_experimental might not be
 *   available at your system)
 ***************************************************************************/

#ifdef PANDORA
#include "codrv_experimental.h"
#endif



/***************************************************************************
 *   XFree86 pccons support
 ***************************************************************************/

#ifdef COMPAT_PCCONS
/* The following calls are special to the old pccons driver and are
 * not understood or supported by codrv. 
 * This file serves as a central definition base for these calls
 * in order to avoid defining them in applications that want to 
 * use them.
 *
 * One word of warning: There are different purpose tty ioctls
 * with the same encoding, see <sys/ioctl.h>
 * TIOCSDTR = _IO('t', 121)
 * TIOCCBRK = _IO('t', 122)
 *
 */
#define CONSOLE_X_MODE_ON _IO('t',121)
#define CONSOLE_X_MODE_OFF _IO('t',122)
#define CONSOLE_X_BELL _IOW('t',123,int[2])
#endif /* COMPAT_PCCONS */

#endif	/* _IOCTL_PC_H_ */

