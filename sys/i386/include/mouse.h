/*-
 * Copyright (c) 1992, 1993 Erik Forsberg.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL I BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: mouse.h,v 1.3 1996/11/15 06:22:48 nate Exp $
 */

#ifndef _MACHINE_MOUSE_H_
#define _MACHINE_MOUSE_H_

#include <sys/types.h>
#include <sys/ioccom.h>

/*
 * NOTE: MOUSEIOC and MOUSEIOCREAD are now obsolete, but will stay 
 * for compatibility reasons. But, remember, the MOUSEIOCREAD ioctl
 * command doesn't work and never worked before.  Some day we shall 
 * get rid of these...
 */

#define MOUSEIOC		('M'<<8)
#define MOUSEIOCREAD		(MOUSEIOC|60)

#define MOUSE_GETSTATE		_IOR('M',0,mouseinfo_t)
#define MOUSE_GETINFO		_IOR('M',1,mousehw_t)
#define MOUSE_GETMODE		_IOR('M',2,mousemode_t)
#define MOUSE_SETMODE		_IOW('M',3,mousemode_t)

typedef struct mouseinfo {
	unsigned char status;
	char xmotion;
	char ymotion;
} mouseinfo_t;

/* status */
#define BUTSTATMASK	0x07	/* Any mouse button down if any bit set */
#define BUTCHNGMASK	0x38	/* Any mouse button changed if any bit set */
#define BUT3STAT	0x01	/* Button 3 down if set */
#define BUT2STAT	0x02	/* Button 2 down if set */
#define BUT1STAT	0x04	/* Button 1 down if set */
#define BUT3CHNG	0x08	/* Button 3 changed if set */
#define BUT2CHNG	0x10	/* Button 2 changed if set */
#define BUT1CHNG	0x20	/* Button 1 changed if set */
#define MOVEMENT	0x40	/* Mouse movement detected */

typedef struct mousehw {
	int buttons;
	int iftype;		/* MOUSE_IF_XXX */
	int type;		/* mouse/track ball/pad... */
	int hwid;		/* I/F dependent hardware ID
				   for the PS/2 mouse, it will be PSM_XXX_ID */
} mousehw_t;

/* iftype */
#define MOUSE_IF_SERIAL		0
#define MOUSE_IF_BUS		1
#define MOUSE_IF_INPORT		2
#define MOUSE_IF_PS2		3

/* type */
#define MOUSE_UNKNOWN		(-1)	/* should be treated as a mouse */
#define MOUSE_MOUSE		0
#define MOUSE_TRACKBALL		1
#define MOUSE_STICK		2
#define MOUSE_PAD		3

typedef struct mousemode {
	int protocol;		/* MOUSE_PROTO_XXX */
	int rate;		/* report rate (per sec), -1 if unknown */
	int resolution;		/* ppi, -1 if unknown */
	int accelfactor;	/* accelation factor (must be 1 or greater) */
} mousemode_t;

/* protocol */
#define MOUSE_PROTO_MS		0	/* Microsoft Serial, 3 bytes */
#define MOUSE_PROTO_MSC		1	/* Mouse Systems, 5 bytes */
#define MOUSE_PROTO_LOGI	2	/* Logitech, 3 bytes */
#define MOUSE_PROTO_MM		3	/* MM series, 3 bytes */
#define MOUSE_PROTO_LOGIMOUSEMAN 4	/* Logitech MouseMan 3/4 bytes */
#define MOUSE_PROTO_BUS		5	/* MS/Logitech bus mouse */
#define MOUSE_PROTO_INPORT	6	/* MS/ATI inport mouse */
#define MOUSE_PROTO_PS2		7	/* PS/2 mouse, 3 bytes */

/* Microsoft Serial mouse data packet */
#define MOUSE_MSS_PACKETSIZE	3
#define MOUSE_MSS_SYNCMASK	0x40
#define MOUSE_MSS_SYNC		0x40
#define MOUSE_MSS_BUTTONS	0x30
#define MOUSE_MSS_BUTTON1DOWN	0x20	/* left */
#define MOUSE_MSS_BUTTON2DOWN	0x00	/* no middle button */
#define MOUSE_MSS_BUTTON3DOWN	0x10	/* right */

/* Mouse Systems Corp. mouse data packet */
#define MOUSE_MSC_PACKETSIZE	5
#define MOUSE_MSC_SYNCMASK	0xf8
#define MOUSE_MSC_SYNC		0x80
#define MOUSE_MSC_BUTTONS	0x07
#define MOUSE_MSC_BUTTON1UP	0x04	/* left */
#define MOUSE_MSC_BUTTON2UP	0x02	/* middle */
#define MOUSE_MSC_BUTTON3UP	0x01	/* right */

/* PS/2 mouse data packet */
#define MOUSE_PS2_PACKETSIZE	3
#define MOUSE_PS2_SYNCMASK	0x08	/* 0x0c for 2 button mouse */
#define MOUSE_PS2_SYNC		0x08	/* 0x0c for 2 button mouse */
#define MOUSE_PS2_BUTTONS	0x07	/* 0x03 for 2 button mouse */
#define MOUSE_PS2_BUTTON1DOWN	0x01	/* left */
#define MOUSE_PS2_BUTTON2DOWN	0x04	/* middle */
#define MOUSE_PS2_BUTTON3DOWN	0x02	/* right */
#define MOUSE_PS2_XNEG		0x10
#define MOUSE_PS2_YNEG		0x20
#define MOUSE_PS2_XOVERFLOW	0x40
#define MOUSE_PS2_YOVERFLOW	0x80

#endif /* _MACHINE_MOUSE_H_ */
