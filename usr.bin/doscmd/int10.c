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
 *	BSDI int10.c,v 2.3 1996/04/08 19:32:40 bostic Exp
 *
 * $FreeBSD$
 */

#include <unistd.h>

#include "doscmd.h"
#include "mouse.h"
#include "tty.h"
#include "video.h"

static int cursoremu = 1;

void
int10(regcontext_t *REGS)
{
	char *addr;
	int i, j;
	int saved_row, saved_col;

	/*
	 * Any call to the video BIOS is enough to reset the poll
	 * count on the keyboard.
	 */
	reset_poll();

	switch (R_AH) {
	case 0x00:		/* Set display mode */
		if (!xmode)
			goto unsupported;
		init_mode(R_AL);
		break;
	case 0x01:		/* Define cursor */
	{
		int start, end;
		
		start = R_CH;
		end = R_CL;
		if (cursoremu == 0)
			goto out;
		/* Cursor emulation */
		if (start <= 3 && end <= 3)
			goto out;
		if (start + 2 >= end) {
			/* underline cursor */
			start = CharHeight - 3;
			end = CharHeight - 2;
			goto out;
		}
		if (start <= 2 || end < start) {
			/* block cursor */
			start = 0;
			end = CharHeight - 2;
			goto out;
		}
		if (start > CharHeight / 2) {
			/* half block cursor */
			start = CharHeight / 2;
			end = 0;
		}
 out:		CursStart = start;
		CursEnd = end;
		break;
	}
	case 0x02:		/* Position cursor */
		if (!xmode)
			goto unsupported;
		tty_move(R_DH, R_DL);
		break;
	case 0x03:		/* Read cursor position */
		if (!xmode)
			goto unsupported;
		tty_report(&i, &j);
		R_DH = i;
		R_DL = j;
		R_CH = CursStart;
		R_CL = CursEnd;
		break;
	case 0x05:
		debug(D_VIDEO, "Select current display page %d\n", R_AL);
		break;
	case 0x06:		/* initialize window/scroll text upward */
		if (!xmode)
			goto unsupported;
		if (R_AL == 0)		/* clear screen */
			R_AL = DpyRows + 1;
		tty_scroll(R_CH, R_CL,
		    R_DH, R_DL,
		    R_AL, R_BH << 8);
		break;
	case 0x07:		/* initialize window/scroll text downward */
		if (!xmode)
			goto unsupported;
		if (R_AL == 0)		/* clear screen */
			R_AL = DpyRows + 1;
		tty_rscroll(R_CH, R_CL,
		    R_DH, R_DL,
		    R_AL, R_BH << 8);
		break;
	case 0x08:		/* read character/attribute */
		if (!xmode)
			goto unsupported;
		i = tty_char(-1, -1);
		R_AX = i;
		break;
	case 0x09:		/* write character/attribute */
		if (!xmode)
			goto unsupported;
		tty_rwrite(R_CX, R_AL, R_BL << 8);
		break;
	case 0x0a:		/* write character */
		if (!xmode)
			goto unsupported;
		debug(D_HALF, "Int 10:0a: Write char: %02x\n", R_AL);
		tty_rwrite(R_CX, R_AL, -1);
		break;
	case 0x0b:		/* set border color */
		if (!xmode)
			goto unsupported;
		video_setborder(R_BL);
		break;
	case 0x0c:		/* write graphics pixel */
		debug(D_VIDEO, "Write graphics pixel at %d, %d\n", R_CX, R_DX);
		break;
	case 0x0d:		/* read graphics pixel */
		debug(D_VIDEO, "Read graphics pixel at %d, %d\n", R_CX, R_DX);
		break;
	case 0x0e:		/* write character */
		tty_write(R_AL, -1);
		break;
	case 0x0f:		/* get current video mode */
		R_AH = DpyCols;		/* number of columns */
		R_AL = VideoMode;	/* active mode */
		R_BH = 0;/*ActivePage *//* display page */
		break;
	case 0x10:
		if (!xmode)
			goto unsupported;
		switch (R_AL) {
		case 0x00:		/* Set single palette register */
			palette[R_BL] = R_BH;
			update_pixels();
			break;
		case 0x01:		/* Set overscan register */
			VGA_ATC[ATC_OverscanColor] = R_BH;
			break;
		case 0x02:		/* Set all palette registers */
			addr = (char *)MAKEPTR(R_ES, R_DX);
			for (i = 0; i < 16; i++)
				palette[i] = *addr++;
			VGA_ATC[ATC_OverscanColor] = *addr;
			update_pixels();
			break;
		case 0x03:		/* Enable/Disable blinking mode */
			video_blink((R_BL & 1) ? 1 : 0);
			break;
		case 0x07:		/* Get individual palette register */
			R_BH = palette[R_BL];
			break;
		case 0x08:		/* Read overscan register */
			R_BH = VGA_ATC[ATC_OverscanColor];
			break;
		case 0x09:		/* Read all palette registers */
			addr = (char *)MAKEPTR(R_ES, R_DX);
			for (i = 0; i < 16; i++)
				*addr++ = palette[i];
			*addr = VGA_ATC[ATC_OverscanColor];
			break;
		case 0x10:		/* Set individual DAC register */
			dac_rgb[R_BX].red   = R_DH & 0x3f;
			dac_rgb[R_BX].green = R_CH & 0x3f;
			dac_rgb[R_BX].blue  = R_CL & 0x3f;
			update_pixels();
			break;
		case 0x12:		/* Set block of DAC registers */
			addr = (char *)MAKEPTR(R_ES, R_DX);
			for (i = R_BX; i < R_BX + R_CX; i++) {
				dac_rgb[i].red   = *addr++;
				dac_rgb[i].green = *addr++;
				dac_rgb[i].blue  = *addr++;
			}
			update_pixels();
			break;
		case 0x13:		/* Select video DAC color page */
			switch (R_BL) {
			case 0:
				VGA_ATC[ATC_ModeCtrl] |= (R_BH & 0x01) << 7;
				break;
			case 1:
				VGA_ATC[ATC_ColorSelect] = R_BH & 0x0f;
				break;
			default:
				debug(D_VIDEO, "INT 10 10:13 "
				    "Bad value for BL: 0x%02x\n", R_BL);
				break;
			}
		case 0x15:		/* Read individual DAC register */
			R_DH = dac_rgb[R_BX].red;
			R_CH = dac_rgb[R_BX].green;
			R_CL = dac_rgb[R_BX].blue;
			break;
		case 0x17:		/* Read block of DAC registers */
			addr = (char *)MAKEPTR(R_ES, R_DX);
			for (i = R_BX; i < R_BX + R_CX; i++) {
				*addr++ = dac_rgb[i].red;
				*addr++ = dac_rgb[i].green;
				*addr++ = dac_rgb[i].blue;
			}
			break;
		case 0x18:		/* Set PEL mask */
			debug(D_HALF,
			    "INT 10 10:18 Set PEL mask (%02x)\n", R_BL);
			break;
		case 0x19:		/* Read PEL mask */
			debug(D_HALF, "INT 10 10:19 Read PEL mask\n");
			break;
		case 0x1a:		/* Get video dac color-page state */
			R_BH = (VGA_ATC[ATC_ModeCtrl] & 0x80) >> 7;
			R_BL = VGA_ATC[ATC_ColorSelect];
			break;
		case 0x1b:		/* Perform gray-scale summing */
			debug(D_HALF, "Perform gray-scale summing\n");
			break;
		default:
			unknown_int3(0x10, 0x10, R_AL, REGS);
			break;
		}
		break;
	case 0x11:
		switch (R_AL) {
		case 0x00:	/* Text-mode chargen: load user-specified
                                   patterns */
			debug(D_VIDEO, "Tried to load user defined font.\n");
			break;
		case 0x01:	/* Text-mode chargen: load ROM monochrome
                                   patterns */
			debug(D_VIDEO, "Tried to load 8x14 font.\n");
			break;
		case 0x02:	/* Text-mode chargen: load ROM 8x8 double-dot
                                   patterns */
			debug(D_VIDEO, "Tried to load 8x8 font.\n");
			break;
		case 0x03:	/* Text-mode chargen: set block specifier */
			debug(D_VIDEO, "Tried to activate character set\n");
			break;
		case 0x04:	/* Text-mode chargen: load ROM 8x16 character
                                   set */
			debug(D_VIDEO, "Tried to load 8x16 font.\n");
			break;
		case 0x10:	/* Text-mode chargen: load and activate
                                   user-specified patterns */
			debug(D_VIDEO,
			    "Tried to load and activate user defined font\n");
			break;
		case 0x11:	/* Text-mode chargen: load and activate ROM
                                   monochrome patterns */
			debug(D_VIDEO,
			    "Tried to load and activate 8x14 font.\n");
			break;
		case 0x12:	/* Text-mode chargen: load and activate ROM
                                   8x8 double-dot patterns */
			debug(D_VIDEO,
			    "Tried to load and activate 8x8 font.\n");
			break;
		case 0x14:	/* Text-mode chargen: load and activate ROM
                                   8x16 character set */
			debug(D_VIDEO,
			    "Tried to load and activate 8x16 font.\n");
			break;
		case 0x20:	/* Graph-mode chargen: set user 8x8 graphics
                                   characters */
			debug(D_VIDEO, "Load second half of 8x8 char set\n");
			break;
		case 0x21:	/* Graph-mode chargen: set user graphics
                                   characters */
			debug(D_VIDEO, "Install user defined char set\n");
			break;
		case 0x22:	/* Graph-mode chargen: set ROM 8x14 graphics
                                   chars */
			debug(D_VIDEO, "Install 8x14 char set\n");
			break;
		case 0x23:	/* Graph-mode chargen: set ROM 8x8 double-dot
                                   chars */
			debug(D_VIDEO, "Install 8x8 char set\n");
			break;
		case 0x24:	/* Graph-mode chargen: load 8x16 graphics
                                   chars */
			debug(D_VIDEO, "Install 8x16 char set\n");
			break;
		case 0x30:	/* Get font information */
			debug(D_VIDEO,
			    "INT 10 11:30 Request font address %02x\n", R_BH);
			R_CX = CharHeight;
			R_DL = DpyRows;
			switch(R_BH) {
			case 0:
				PUTVEC(R_ES, R_BP, ivec[0x1f]);
				break;
			case 1:
				PUTVEC(R_ES, R_BP, ivec[0x43]);
				break;
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
				R_ES = 0;
				R_BP = 0;
				break;
			default:
				unknown_int4(0x10, 0x11, 0x30, R_BH, REGS);
				break;
			}
			break;
		default:
			unknown_int3(0x10, 0x11, R_AL, REGS);
			break;
		}
		break;
	case 0x12:		/* Alternate function select */
		if (!xmode)
			goto unsupported;
		switch (R_BL) {
		case 0x10:	/* Read EGA/VGA config */
			R_BH = NumColors > 1 ? 0 : 1;	/* Color */
			R_BL = 3; 			/* 256 K */
			break;
		case 0x34:	/* Cursor emulation */
			if (R_AL == 0)
				cursoremu = 1;
			else
				cursoremu = 0;
			R_AL = 0x12;
			break;
		default:
			if (vflag)
				dump_regs(REGS);
			unknown_int3(0x10, 0x12, R_BL, REGS);
			break;
		}
		break;
	case 0x13: /* write character string */
		if (!xmode)
			goto unsupported;
                addr = (char *)MAKEPTR(R_ES, R_BP);
		switch (R_AL & 0x03) {
		case 0:
			tty_report(&saved_row, &saved_col);
			tty_move(R_DH, R_DL);
			for (i = 0; i < R_CX; ++i)
				tty_write(*addr++, R_BL << 8);
			tty_move(saved_row, saved_col);
			break;
		case 1:
			tty_move(R_DH, R_DL);
			for (i = 0; i < R_CX; ++i)
				tty_write(*addr++, R_BL << 8);
			break;
		case 2:
			tty_report(&saved_row, &saved_col);
			tty_move(R_DH, R_DL);
			for (i = 0; i < R_CX; ++i) {
				tty_write(addr[0], addr[1]);
				addr += 2;
			}
			tty_move(saved_row, saved_col);
			break;
		case 3:
			tty_move(R_DH, R_DL);
			for (i = 0; i < R_CX; ++i) {
				tty_write(addr[0], addr[1]);
				addr += 2;
			}
			break;
		}
		break;
	case 0x1a:
		if (!xmode)
			goto unsupported;
		R_AL = 0x1a;		/* I am VGA */
		R_BL = 8;		/* Color VGA */
		R_BH = 0;		/* No other card */
		break;
	case 0x1b:	/* Video Functionality/State information */
		if (R_BX == 0) {
			addr = (char *)MAKEPTR(R_ES, R_DI);
			memcpy(addr, vga_status, 64);
			R_AL = 0x1b;
		}
		break;
	case 0x1c:	/* Save/Restore video state */
		debug(D_VIDEO, "VGA: Save/restore video state\n");
		R_AL = 0;
		break;
	case 0x30:	/* Locate 3270PC configuration table */
		R_CX = 0;
		R_DX = 0;
		break;
	case 0x4f:	/* get VESA information */
		R_AH = 0x01;		/* no VESA support */
		break;
	case 0x6f:
		switch (R_AL) {
		case 0x00:	/* HP-Vectra or Video7 installation check */
			R_BX = 0;		/* nope, none of that */
			break;
		default:
			unknown_int3(0x10, 0x6f, R_AL, REGS);
			break;
		}
		break;
	case 0xef:
	case 0xfe:	/* Get video buffer */
		break;
	case 0xfa:	/* Interrogate mouse driver */
		if (xmode)
			PUTPTR(R_ES, R_BX, (long)mouse_area);
		break;
    	case 0xff:	/* Update real screen from video buffer */
		/* XXX - we should allow secondary buffer here and then
			 update it as the user requests. */
		break;
    	unsupported:
		if (vflag)
			dump_regs(REGS);
		fatal("int10 function 0x%02x:%02x only available in X mode\n",
		    R_AH, R_AL);
	default:
		if (vflag)
			dump_regs(REGS);
		unknown_int3(0x10, R_AH, R_AL, REGS);
		break;
	}
}
