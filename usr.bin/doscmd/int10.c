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

#include "doscmd.h"
#include "mouse.h"

/*
 * 0040:0060 contains the start and end of the cursor
 */                 
#define	curs_end BIOSDATA[0x60]
#define	curs_start BIOSDATA[0x61]

void
int10(REGISTERS)
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
	case 0x00: /* Set display mode */
		debug(D_HALF, "Set video mode to %02x\n", R_AL);
		break;
	case 0x01: /* Define cursor */
		curs_start = R_CH;
		curs_end = R_CL;
		break;
	case 0x02: /* Position cursor */
		if (!xmode)
			goto unsupported;
		tty_move(R_DH, R_DL);
		break;
	case 0x03: /* Read cursor position */
		if (!xmode)
			goto unsupported;
		tty_report(&i, &j);
		R_DH = i;
		R_DL = j;
		R_CH = curs_start;
		R_CL = curs_end;
		break;
	case 0x05:
		debug(D_HALF, "Select current display page %d\n", R_AL);
		break;
	case 0x06: /* initialize window/scroll text upward */
		if (!xmode)
			goto unsupported;
		tty_scroll(R_CH, R_CL,
			   R_DH, R_DL,
			   R_AL, R_BH << 8);
		break;
	case 0x07: /* initialize window/scroll text downward */
		if (!xmode)
			goto unsupported;
		tty_rscroll(R_CH, R_CL,
			    R_DH, R_DL,
			    R_AL, R_BH << 8);
		break;
	case 0x08: /* read character/attribute */
		if (!xmode)
			goto unsupported;
		i = tty_char(-1, -1);
		R_AX = i;
		break;
	case 0x09: /* write character/attribute */
		if (!xmode)
			goto unsupported;
		tty_rwrite(R_CX, R_AL, R_BL << 8);
		break;
	case 0x0a: /* write character */
		if (!xmode)
			goto unsupported;
		tty_rwrite(R_CX, R_AL, -1);
		break;
	case 0x0b: /* set border color */
		if (!xmode)
			goto unsupported;
		video_setborder(R_BL);
		break;
	case 0x0e: /* write character */
		tty_write(R_AL, -1);
		break;
	case 0x0f: /* get display mode */
		R_AH = 80; /* number of columns */
		R_AL = 3; /* color */
		R_BH = 0; /* display page */
		break;
	case 0x10:
		switch (R_AL) {
		case 0x01:
			video_setborder(R_BH & 0x0f);
			break;
		case 0x02:		/* Set pallete registers */
			debug(D_HALF, "INT 10 10:02 Set all palette registers\n");
			break;
		case 0x03:		/* Enable/Disable blinking mode */
			video_blink(R_BL ? 1 : 0);
			break;
		case 0x13:
			debug(D_HALF,
			      "INT 10 10:13 Select color or DAC (%02x, %02x)\n",
				      R_BL, R_BH);
			break;
		case 0x1a: /* get video dac color-page state */
			R_BH = 0;		/* Current page */
			R_BL = 0;		/* four pages of 64... */
			break;
		default:
			unknown_int3(0x10, 0x10, R_AL, REGS);
			break;
		}
		break;
#if 1
	case 0x11:
		switch (R_AL) {
		case 0x00: printf("Tried to load user defined font.\n"); break;
		case 0x01: printf("Tried to load 8x14 font.\n"); break;
		case 0x02: printf("Tried to load 8x8 font.\n"); break;
		case 0x03: printf("Tried to activate character set\n"); break;
		case 0x04: printf("Tried to load 8x16 font.\n"); break;
		case 0x10: printf("Tried to load and activate user defined font\n"); break;
		case 0x11: printf("Tried to load and activate 8x14 font.\n"); break;
		case 0x12: printf("Tried to load and activate 8x8 font.\n"); break;
		case 0x14: printf("Tried to load and activate 8x16 font.\n"); break;
		case 0x30:
			R_CX = 14;
			R_DL = 24;
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
				debug(D_HALF,
				      "INT 10 11:30 Request font address %02x",
				      R_BH);
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
#endif
	case 0x12: /* Load multiple DAC color register */
		if (!xmode)
			goto unsupported;
		switch (R_BL) {
		case 0x10:	/* Read EGA/VGA config */
			R_BH = 0;	/* Color */
			R_BL = 0;	/* 64K */
			break;
		default:
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
		R_AL = 0x1a;	/* I am VGA */
		R_BL = 8;		/* Color VGA */
		R_BH = 0;		/* No other card */
		break;

	case 0x4f:	/* get VESA information */
	    R_AH = 0x01;		/* no VESA support */
	    break;

	case 0x1b:	/* Functionality state information */
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
		if (vflag) dump_regs(REGS);
		fatal ("int10 function 0x%02x:%02x only available in X mode\n",
			R_AH, R_AL);
    	unknown:
	default:
		unknown_int3(0x10, R_AH, R_AL, REGS);
		break;
	}
}
