/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI video.h,v 2.2 1996/04/08 19:33:12 bostic Exp
 *
 * $FreeBSD$
 */

/*
 * Motorola 6845 Video Controller registers
 *
 * They are read by
 *	OUT port,code
 *	IN  port+1,res
 *
 * They are written by
 *	OUT port,code
 *	OUT port+1,value
 */
#define	MVC_TotHorzChar		0x00	/* Total Horizontal Character */
#define	MVC_DispHorzChar	0x01	/* Display Horizontal Character */
#define	MVC_HorzSyncChar	0x02	/* Horizontal sync signal after ...char */
#define	MVC_HorzSyncDur		0x03	/* Duration of horizontal sync signal in char */
#define	MVC_TotVertChar		0x04	/* Total Vertical Character */
#define	MVC_AdjVertChar		0x05	/* Adjust Veritcal Character */
#define	MVC_DispVertChar	0x06	/* Display Vertical Charcter */
#define	MVC_VertSyncChar	0x07	/* Vertical sync signal after .. char */
#define	MVC_InterlaceMode	0x08	/* Interlace Mode */
#define	MVC_ScanLines		0x09	/* Number of scan lines per screen line */
#define	MVC_CurStartLine	0x0a	/* Starting line of screen cursor */
#define	MVC_CurEndLine		0x0b	/* Ending line of screen cursor */

#define	MVC_CurHigh		0x0e	/* High byte of cursor position */
#define	MVC_CurLow		0x0f	/* High byte of cursor position */

/*
 * Additional MDA register
 */
#define	MDA_StartDispPageLo	0x0c	/* Starting address of displayed screen page (lo byte) */
#define	MDA_StartDispPageHi	0x0d	/* Starting address of displayed screen page (hi byte) */
#define	MDA_BlinkCurAddrHi	0x0e	/* Character address of blinking screen cursor (hi byte) */
#define	MDA_BlinkCurAddrLo	0x0f	/* Character address of blinking screen cursor (lo byte) */
#define	MDA_LightPenHi		0x10	/* Light Pen Position (hi byte) */
#define	MDA_LightPenLo		0x11	/* Light Pen Position (lo byte) */

#define	MDA_Control		0x03b8	/* MDA Control Register Port */
#define	MVC_Address		0x03b4	/* MVC Address Register */
#define	MVC_Data		0x03b5	/* MVC Data Register */
#define	MDA_VideoSeg		0xb800	/* Segmet address of video ram */

#define	CGA_Control		0x03d8	/* CGA Control Register Port */
#define	CGA_Status		0x03da	/* CGA Control Register Port */
#define	CVC_Address		0x03d4	/* CVC Address Register */
#define	CVC_Data		0x03d5	/* CVC Data Register */

#define	CGA_Black		0x0
#define	CGA_Blue		0x1
#define	CGA_Green		0x2
#define	CGA_Cyan		0x3
#define	CGA_Red			0x4
#define	CGA_Magenta		0x5
#define	CGA_Brown		0x6
#define	CGA_LightGray		0x7
#define	CGA_DarkGray		0x8
#define	CGA_LightBlue		0x9
#define	CGA_LightGreen		0xa
#define	CGA_LightCyan		0xb
#define	CGA_LightRed		0xc
#define	CGA_LightMagenta	0xd
#define	CGA_Yellow		0xe
#define	CGA_White		0xf

#define	VGA_Segment		0xa000	/* Starting Segment of VGA Memory */
#define	V_int		0x10		/* interrupt for dealing with screen */
#define	V_mode		0		/* code for setting new screen mode */
#define	V_curtype	1		/* code for setting new cursor type */
#define	V_setcur	2		/* code for addressing cursor */
#define	V_readcur	3		/* code for reading cursor location */
#define	V_readlp	4		/* code for reading light pen position */
#define	V_setpage	5		/* code to select active page */
#define	V_scrollup	6		/* code to scroll screen up */
#define	V_scrolldn	7		/* code to scroll screen nown */
#define	V_readch	8		/* code to read a character from screen */
#define	V_writeach	9		/* code to write char and attributes */
#define	V_writech	10		/* code to write character only */
#define	V_setpal	11		/* code to set new setpal or border */
#define	V_wdot		12		/* code to write a dot */
#define	V_rdot		13		/* code to read a dot */
#define	V_wtty		14		/* code to write as if teletype */
#define	V_state		15		/* code to find current screen status */

#define	VM_40x25	0x00
#define	VM_80x25	0x02
#define	VM_320x200x4	0x04
#define	VM_640x200x2	0x06
#define	VM_80x25mono	0x07
#define	VM_320x200x16	0x0d
#define	VM_640x200x16	0x0e
#define	VM_640x350mono	0x0f
#define	VM_640x350x16	0x10
#define	VM_640x480x2	0x11
#define	VM_640x480x16	0x12
#define	VM_320x200x256	0x13
#define	VM_80x30	0x50
#define	VM_80x43	0x51
#define	VM_80x60	0x52
#define	VM_132x25	0x53
#define	VM_132x30	0x54
#define	VM_132x43	0x55
#define	VM_132x60	0x56
#define	VM_132x25h	0x57
#define	VM_132x30h	0x58
#define	VM_132x43h	0x59
#define	VM_132x60h	0x5a
#define	VM_800x600x16	0x5b
#define	VM_640x400x256	0x5c
#define	VM_640x480x256	0x5d
#define	VM_800x600x256	0x5e
#define	VM_1024x768x16	0x5f
#define	VM_1024x768x4	0x60
#define	VM_768x1024x16	0x61
#define	VM_1024x768x256	0x62

#define	VM_VGA		VM_640x480x256
#define	VM_EVGA		VM_800x600x256
#define	VM_SVGAportrait	VM_768x1024x16
#define	VM_SVGA16	VM_1024x768x16
#define	VM_SVGA256	VM_1024x768x256
