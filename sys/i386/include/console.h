/*
 *  Copyright (C) 1992, 1993, 1994 Søren Schmidt
 *
 *  This program is free software; you may redistribute it and/or 
 *  modify it, provided that it retain the above copyright notice 
 *  and the following disclaimer.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *
 *	Søren Schmidt 		Email:	sos@login.dkuug.dk
 *	Tritonvej 36		UUCP:	...uunet!dkuug!login!sos
 *	DK9210 Aalborg SO	Phone:  +45 9814 8076
 *
 *	from:@(#)console.h	1.1 940105
 *	$Id: console.h,v 1.9 1994/05/20 12:21:49 sos Exp $
 */

#ifndef	_CONSOLE_H_
#define	_CONSOLE_H_

#include <sys/types.h>
#include <sys/ioctl.h>

#define KDGKBMODE 	_IOR('K',  6, int)
#define KDSKBMODE 	_IO('K',  7)
#define KDMKTONE	_IO('K',  8)
#define KDGETMODE	_IOR('K',  9, int)
#define KDSETMODE	_IO('K', 10)
#define KDSBORDER	_IO('K', 13)
#define KDGKBSTATE	_IOR('K', 19, int)
#define KDSKBSTATE	_IO('K', 20)
#define KDENABIO	_IO('K', 60)
#define KDDISABIO	_IO('K', 61)
#define KIOCSOUND	_IO('K', 63)
#define KDGKBTYPE	_IOR('K', 64, int)
#define KDGETLED	_IOR('K', 65, int)
#define KDSETLED	_IO('K', 66) 
#define KDSETRAD	_IO('K', 67)

#define GETFKEY		_IOWR('k', 0, fkeyarg_t)
#define SETFKEY		_IOWR('k', 1, fkeyarg_t)
#define GIO_SCRNMAP	_IOR('k', 2, scrmap_t)
#define PIO_SCRNMAP	_IOW('k', 3, scrmap_t)
#define GIO_KEYMAP 	_IOR('k', 6, keymap_t)
#define PIO_KEYMAP 	_IOW('k', 7, keymap_t)

#define CONS_BLANKTIME	_IOW('c', 4, long)
#define CONS_SSAVER	_IOW('c', 5, ssaver_t)
#define CONS_GSAVER	_IOWR('c', 6, ssaver_t)
#define PIO_FONT8x8	_IOW('c', 64, fnt8_t)
#define GIO_FONT8x8	_IOR('c', 65, fnt8_t)
#define PIO_FONT8x14	_IOW('c', 66, fnt14_t)
#define GIO_FONT8x14	_IOR('c', 67, fnt14_t)
#define PIO_FONT8x16	_IOW('c', 68, fnt16_t)
#define GIO_FONT8x16	_IOR('c', 69, fnt16_t)
#define CONS_GETINFO    _IOWR('c', 73, vid_info_t)
#define CONS_GETVERS	_IOR('c', 74, long)
#define CONS_80x25TEXT	_IO('c', 102)
#define CONS_80x50TEXT	_IO('c', 103)

#define VT_OPENQRY	_IOR('v', 1, int)
#define VT_SETMODE	_IOW('v', 2, vtmode_t)	
#define VT_GETMODE	_IOR('v', 3, vtmode_t)
#define VT_RELDISP	_IO('v', 4)
#define VT_ACTIVATE	_IO('v', 5)
#define VT_WAITACTIVE	_IO('v', 6)
#define VT_GETACTIVE	_IOR('v', 7, int)

#define VT_FALSE	0	
#define VT_TRUE		1
#define VT_ACKACQ	2

#define VT_AUTO		0		/* switching is automatic 	*/
#define VT_PROCESS	1		/* switching controlled by prog */

/* compatibility to old pccons & X386 */
#define CONSOLE_X_MODE_ON	_IO('t', 121)
#define CONSOLE_X_MODE_OFF	_IO('t', 122)
#define CONSOLE_X_BELL		_IOW('t',123,int[2])

struct vt_mode {
	char	mode;
	char	waitv;			/* not implemented yet 	SOS	*/
	short	relsig;
	short	acqsig;
	short	frsig;			/* not implemented yet	SOS	*/
};


#define KD_MONO		1		/* monochrome adapter        	*/
#define KD_HERCULES	2		/* hercules adapter          	*/
#define KD_CGA		3		/* color graphics adapter    	*/
#define KD_EGA		4		/* enhanced graphics adapter 	*/
#define KD_VGA		5		/* video graohics adapter    	*/

#define KD_TEXT		0		/* set text mode restore fonts  */
#define KD_TEXT0	0		/* ditto			*/
#define KD_TEXT1	2		/* set text mode !restore fonts */
#define KD_GRAPHICS	1		/* set graphics mode 		*/

#define K_RAW		0		/* keyboard returns scancodes	*/
#define K_XLATE		1		/* keyboard returns ascii 	*/

#define KB_84		1		/* 'old' 84 key AT-keyboard	*/
#define KB_101		2		/* MF-101 or MF-102 keyboard	*/
#define KB_OTHER	3		/* keyboard not known 		*/

#define CLKED		1		/* Caps locked			*/
#define NLKED		2		/* Num locked			*/
#define SLKED		4		/* Scroll locked		*/
#define ALKED		8		/* AltGr locked			*/
#define LED_CAP		1		/* Caps lock LED 		*/ 
#define LED_NUM		2		/* Num lock LED 		*/
#define LED_SCR		4		/* Scroll lock LED 		*/

/* possible flag values */
#define	FLAG_LOCK_O	0
#define	FLAG_LOCK_C	1
#define FLAG_LOCK_N	2

#define NUM_KEYS	256		/* number of keys in table	*/
#define NUM_STATES	8		/* states per key		*/
#define ALTGR_OFFSET	128		/* offset for altlock keys	*/

struct key_t {
	u_char map[NUM_STATES];
	u_char spcl;
	u_char flgs;
};

struct keymap {
	u_short	n_keys;
	struct key_t key[NUM_KEYS];
};

#define MAXFK		16
#define NUM_FKEYS	60

struct fkeytab {
	u_char	str[MAXFK];
	u_char	len;
};

struct fkeyarg {
	u_short	keynum;
	char	keydef[MAXFK];
	char	flen;
};

struct colors	{
	char	fore;
	char	back;
};

struct vid_info {
	short	size;
	short	m_num;
	u_short	mv_row, mv_col;
	u_short	mv_rsz, mv_csz;
	struct colors	mv_norm,
			mv_rev,
			mv_grfc;
	u_char	mv_ovscan;
	u_char	mk_keylock;
};

#define MAXSSAVER	16

struct ssaver	{
	char	name[MAXSSAVER];
	int	num;
	long	time;
};

typedef struct keymap keymap_t;
typedef struct fkeytab fkeytab_t;
typedef struct fkeyarg fkeyarg_t;
typedef struct vid_info vid_info_t;
typedef struct vt_mode vtmode_t;
typedef struct {char scrmap[256];} scrmap_t;
typedef struct {char fnt8x8[8*256];} fnt8_t;
typedef struct {char fnt8x14[14*256];} fnt14_t;
typedef struct {char fnt8x16[16*256];} fnt16_t;
typedef struct ssaver ssaver_t;

/* defines for "special" keys (spcl bit set in keymap) */
#define NOP		0x00		/* nothing (dead key)		*/
#define LSH		0x02		/* left shift key		*/
#define RSH		0x03		/* right shift key		*/
#define CLK		0x04		/* caps lock key		*/
#define NLK		0x05		/* num lock key			*/
#define SLK		0x06		/* scroll lock key		*/
#define LALT		0x07		/* left alt key			*/
#define BTAB		0x08		/* backwards tab		*/
#define LCTR		0x09		/* left control key		*/
#define NEXT		0x0a		/* switch to next screen 	*/
#define F_SCR		0x0b		/* switch to first screen 	*/
#define L_SCR		0x1a		/* switch to last screen 	*/
#define F_FN		0x1b		/* first function key 		*/
#define L_FN		0x7a		/* last function key 		*/
#define RCTR		0x7b		/* right control key		*/
#define RALT		0x7c		/* right alt (altgr) key	*/
#define ALK		0x7d		/* alt lock key			*/
#define ASH		0x7e		/* alt shift key		*/
#define META		0x7f		/* meta key			*/
#define RBT		0x80		/* boot machine			*/
#define DBG		0x81		/* call debugger		*/

#define F(x)		((x)+F_FN-1)
#define	S(x)		((x)+F_SCR-1)
#define NOKEY		0x100		/* no key pressed marker 	*/
#define FKEY		0x200		/* funtion key marker 		*/
#define MKEY		0x400		/* meta key marker (prepend ESC)*/

#define	KB_DATA		0x60		/* kbd data port 		*/
#define	KB_STAT		0x64		/* kbd status port 		*/
#define	KB_BUF_FULL	0x01		/* kbd has char pending 	*/
#define	KB_READY	0x02		/* kbd ready for command 	*/
#define KB_MODE		0x4D		/* kbd mode (trans, ints enable)*/
#define KB_WRITE	0x60		/* kbd write command 		*/
#define KB_RESET_DONE	0xAA		/* kbd reset command completed  */
#define KB_SETLEDS	0xED		/* kbd set leds 		*/
#define KB_ECHO		0xEE		/* kbd set leds 		*/
#define KB_SETRAD	0xF3		/* kbd set repeat&delay command */
#define KB_ACK		0xFA		/* kbd acknowledge answer 	*/
#define KB_RESEND	0xFE		/* kbd resend cmd answer      	*/
#define KB_RESET	0xFF		/* kbd reset 			*/

#endif
