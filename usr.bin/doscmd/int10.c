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
 * $Id: int10.c,v 1.2 1996/09/22 05:53:00 miff Exp $
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

	switch (GET8H(sc->sc_eax)) {
	case 0x00: /* Set display mode */
		debug(D_HALF, "Set video mode to %02x\n", GET8L(sc->sc_eax));
		break;
	case 0x01: /* Define cursor */
		curs_start = GET8H(sc->sc_ecx);
		curs_end = GET8L(sc->sc_ecx);
		break;
	case 0x02: /* Position cursor */
		if (!xmode)
			goto unsupported;
		tty_move(GET8H(sc->sc_edx), GET8L(sc->sc_edx));
		break;
	case 0x03: /* Read cursor position */
		if (!xmode)
			goto unsupported;
		tty_report(&i, &j);
		SET8H(sc->sc_edx, i);
		SET8L(sc->sc_edx, j);
		SET8H(sc->sc_ecx, curs_start);
		SET8L(sc->sc_ecx, curs_end);
		break;
	case 0x05:
		debug(D_HALF, "Select current display page %d\n", GET8L(sc->sc_eax));
		break;
	case 0x06: /* initialize window/scroll text upward */
		if (!xmode)
			goto unsupported;
		tty_scroll(GET8H(sc->sc_ecx), GET8L(sc->sc_ecx),
			   GET8H(sc->sc_edx), GET8L(sc->sc_edx),
			   GET8L(sc->sc_eax), GET8H(sc->sc_ebx) << 8);
		break;
	case 0x07: /* initialize window/scroll text downward */
		if (!xmode)
			goto unsupported;
		tty_rscroll(GET8H(sc->sc_ecx), GET8L(sc->sc_ecx),
			    GET8H(sc->sc_edx), GET8L(sc->sc_edx),
			    GET8L(sc->sc_eax), GET8H(sc->sc_ebx) << 8);
		break;
	case 0x08: /* read character/attribute */
		if (!xmode)
			goto unsupported;
		i = tty_char(-1, -1);
		SET16(sc->sc_eax, i);
		break;
	case 0x09: /* write character/attribute */
		if (!xmode)
			goto unsupported;
		tty_rwrite(GET16(sc->sc_ecx), GET8L(sc->sc_eax), GET8L(sc->sc_ebx) << 8);
		break;
	case 0x0a: /* write character */
		if (!xmode)
			goto unsupported;
		tty_rwrite(GET16(sc->sc_ecx), GET8L(sc->sc_eax), -1);
		break;
	case 0x0b: /* set border color */
		if (!xmode)
			goto unsupported;
		video_setborder(GET8L(sc->sc_ebx));
		break;
	case 0x0e: /* write character */
		tty_write(GET8L(sc->sc_eax), -1);
		break;
	case 0x0f: /* get display mode */
		SET8H(sc->sc_eax, 80); /* number of columns */
		SET8L(sc->sc_eax, 3); /* color */
		SET8H(sc->sc_ebx, 0); /* display page */
		break;
	case 0x10:
		switch (GET8L(sc->sc_eax)) {
		case 0x01:
			video_setborder(GET8H(sc->sc_ebx) & 0x0f);
			break;
		case 0x02:		/* Set pallete registers */
			debug(D_HALF, "INT 10 10:02 Set all palette registers\n");
			break;
		case 0x03:		/* Enable/Disable blinking mode */
			video_blink(GET8L(sc->sc_ebx) ? 1 : 0);
			break;
		case 0x13:
			debug(D_HALF,
			      "INT 10 10:13 Select color or DAC (%02x, %02x)\n",
				      GET8L(sc->sc_ebx), GET8H(sc->sc_ebx));
			break;
		case 0x1a: /* get video dac color-page state */
			SET8H(sc->sc_ebx, 0);		/* Current page */
			SET8L(sc->sc_ebx, 0);		/* four pages of 64... */
			break;
		default:
			unknown_int3(0x10, 0x10, GET8L(sc->sc_eax), sc);
			break;
		}
		break;
#if 1
	case 0x11:
		switch (GET8L(sc->sc_eax)) {
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
			SET16(sc->sc_ecx, 14);
			SET8L(sc->sc_edx, 24);
			switch(GET8H(sc->sc_ebx)) {
			case 0:
				PUTVEC(sc->sc_es, sc->sc_ebp, ivec[0x1f]);
				break;
			case 1:
				PUTVEC(sc->sc_es, sc->sc_ebp, ivec[0x43]);
				break;
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
				SET16(sc->sc_es, 0);
				SET16(sc->sc_ebp, 0);
				debug(D_HALF,
				      "INT 10 11:30 Request font address %02x",
				      GET8H(sc->sc_ebx));
				break;
			default:
				unknown_int4(0x10, 0x11, 0x30, GET8H(sc->sc_ebx), sc);
				break;
			}
			break;
		default:
			unknown_int3(0x10, 0x11, GET8L(sc->sc_eax), sc);
			break;
		}
		break;
#endif
	case 0x12: /* Load multiple DAC color register */
		if (!xmode)
			goto unsupported;
		switch (GET8L(sc->sc_ebx)) {
		case 0x10:	/* Read EGA/VGA config */
			SET8H(sc->sc_ebx, 0);	/* Color */
			SET8L(sc->sc_ebx, 0);	/* 64K */
			break;
		default:
			unknown_int3(0x10, 0x12, GET8L(sc->sc_ebx), sc);
			break;
		}
		break;
	case 0x13: /* write character string */
		if (!xmode)
			goto unsupported;
                addr = (char *)GETPTR(sc->sc_es, sc->sc_ebp);
		switch (GET8L(sc->sc_eax) & 0x03) {
		case 0:
			tty_report(&saved_row, &saved_col);
			tty_move(GET8H(sc->sc_edx), GET8L(sc->sc_edx));
			for (i = 0; i < GET16(sc->sc_ecx); ++i)
				tty_write(*addr++, GET8L(sc->sc_ebx) << 8);
			tty_move(saved_row, saved_col);
			break;
		case 1:
			tty_move(GET8H(sc->sc_edx), GET8L(sc->sc_edx));
			for (i = 0; i < GET16(sc->sc_ecx); ++i)
				tty_write(*addr++, GET8L(sc->sc_ebx) << 8);
			break;
		case 2:
			tty_report(&saved_row, &saved_col);
			tty_move(GET8H(sc->sc_edx), GET8L(sc->sc_edx));
			for (i = 0; i < GET16(sc->sc_ecx); ++i) {
				tty_write(addr[0], addr[1]);
				addr += 2;
			}
			tty_move(saved_row, saved_col);
			break;
		case 3:
			tty_move(GET8H(sc->sc_edx), GET8L(sc->sc_edx));
			for (i = 0; i < GET16(sc->sc_ecx); ++i) {
				tty_write(addr[0], addr[1]);
				addr += 2;
			}
			break;
		}
		break;
	case 0x1a:
		if (!xmode)
			goto unsupported;
		SET8L(sc->sc_eax, 0x1a);	/* I am VGA */
		SET8L(sc->sc_ebx, 8);		/* Color VGA */
		SET8H(sc->sc_ebx, 0);		/* No other card */
		break;

	case 0x4f:	/* get VESA information */
	    SET8H(sc->sc_eax, 0x01);		/* no VESA support */
	    break;

	case 0x1b:	/* Functionality state information */
	case 0xef:
	case 0xfe:	/* Get video buffer */
		break;
	case 0xfa:	/* Interrogate mouse driver */
		if (xmode)
			PUTPTR(sc->sc_es, sc->sc_ebx, (long)mouse_area);
		break;
    	case 0xff:	/* Update real screen from video buffer */
		/* XXX - we should allow secondary buffer here and then
			 update it as the user requests. */
		break;

    	unsupported:
		if (vflag) dump_regs(sc);
		fatal ("int10 function 0x%02x:%02x only available in X mode\n",
			GET8H(sc->sc_eax), GET8L(sc->sc_eax));
    	unknown:
	default:
		unknown_int2(0x10, GET8H(sc->sc_eax), sc);
		break;
	}
}
