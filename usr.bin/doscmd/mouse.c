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
 *	BSDI int33.c,v 2.2 1996/04/08 19:32:54 bostic Exp
 *
 * $Id: mouse.c,v 1.3 1996/09/22 15:42:58 miff Exp $
 */

#include "doscmd.h"
#include "mouse.h"

mouse_t		mouse_status;
u_char		*mouse_area = 0;
int 		nmice = 0;

static void 
mouse_probe(void) 
{ 
}

void
int33(regcontext_t *REGS)
{
    u_long vec;
    u_short mask;
    void *addr;
    int i;

    if (!nmice) {
	R_FLAGS |= PSL_C;	/* We don't support a mouse */
	return;
    }

    printf("Mouse: %02x\n", R_AX);
    switch (R_AX) {
    case 0x00:				/* Reset Mouse */
	printf("Installing mouse driver\n");
	R_AX = 0xffff;			/* Mouse installed */
	R_BX = 2;			/* Number of mouse buttons */
	memset(&mouse_status, 0, sizeof(mouse_status));
	mouse_status.installed = 1;
	mouse_status.hardcursor = 1;
	mouse_status.end = 16;
	mouse_status.hmickey = 8;
	mouse_status.vmickey = 16;
	mouse_status.doubling = 100;
	mouse_status.init = -1;
	mouse_status.range.w = 8 * 80;
	mouse_status.range.h = 16 * 25;
	break;

    case 0x01:	/* Display Mouse Cursor */
	if ((mouse_status.init += 1) == 0) {
	    mouse_status.show = 1;
	}
	break;

    case 0x02:	/* Hide Mouse Cursor */
	if (mouse_status.init == 0)
	    mouse_status.show = 0;
	mouse_status.init -= 1;
	break;

    case 0x03:	/* Get cursor position/button status */
	mouse_probe();
	R_CX = mouse_status.x;
	R_DX = mouse_status.y;
	R_BX = mouse_status.buttons;
	break;

    case 0x04:	/* Move mouse cursor */
	/* mouse_move(GET16(sc->sc_ecx), GET16(sc->sc_edx)); */
	break;

    case 0x05:	/* Determine number of times mouse button was active */
	i = R_BX & 3;
	if (i == 3)
	    i = 1;
	
	R_BX = mouse_status.downs[i];
	mouse_status.downs[i] = 0;
	R_AX = mouse_status.buttons;
	R_CX = mouse_status.x;		/* Not quite right */
	R_DX = mouse_status.y;		/* Not quite right */
	break;

    case 0x06:	/* Determine number of times mouse button was relsd */
	i = R_DX & 3;
	if (i == 3)
	    i = 1;
	
	R_BX = mouse_status.ups[i];
	mouse_status.ups[i] = 0;
	R_AX = mouse_status.buttons;
	R_CX = mouse_status.x;		/* Not quite right */
	R_DX = mouse_status.y;		/* Not quite right */
	break;
	
    case 0x07:	/* Set min/max horizontal cursor position */
	mouse_status.range.x = R_CX;
	mouse_status.range.w = R_DX - R_CX;
	break;
	
    case 0x08:	/* Set min/max vertical cursor position */
	mouse_status.range.y = R_CX;
	mouse_status.range.h = R_DX - R_CX;
	
    case 0x09:	/* Set graphics cursor block */
	/* BX,CX is hot spot, ES:DX is data. */
	break;
	
    case 0x0a:	/* Set Text Cursor */
	mouse_status.hardcursor = R_BX ? 1 : 0;
	mouse_status.start = R_CX;
	mouse_status.end = R_CX;	/* XXX is this right ? */
	break;

    case 0x0b:	/* Read Mouse Motion Counters */
	mouse_probe();
	R_CX = mouse_status.x - mouse_status.lastx;
	R_DX = mouse_status.y - mouse_status.lasty;
	mouse_status.lastx - mouse_status.x;
	mouse_status.lasty - mouse_status.y;
	break;

    case 0x0c:	/* Set event handler */
	mouse_status.mask = R_CX;
	mouse_status.handler = N_GETVEC(R_ES, R_DX);
	break;
    
    case 0x0d:	/* Enable light pen */
    case 0x0e:	/* Disable light pen */
	break;

    case 0x0f:	/* Set cursor speed */
	mouse_status.hmickey = R_CX;
	mouse_status.vmickey = R_DX;
	break;
    
    case 0x10:	/* Exclusive area */
	mouse_status.exclude.x = R_CX;
	mouse_status.exclude.y = R_DX;
	mouse_status.exclude.w = R_SI - R_CX;
	mouse_status.exclude.h = R_DI - R_DX;
	break;

    case 0x13:	/* Set maximum for mouse speed doubling */
	break;
    case 0x14:	/* Exchange event handlers */
	mask = mouse_status.mask;
	vec = mouse_status.handler;

	mouse_status.mask = R_CX;
	mouse_status.handler = GETVEC(R_ES, R_DX);
	R_CX = mask;
	N_PUTVEC(R_ES, R_DX, vec);
	break;

    case 0x15:	/* Determine mouse status buffer size */
	R_BX = sizeof(mouse_status);
	break;

    case 0x16:	/* Store mouse buffer */
	memcpy((char *)N_GETPTR(R_ES, R_DX), &mouse_status,
	       sizeof(mouse_status));
	break;

    case 0x17:	/* Restore mouse buffer */
	memcpy(&mouse_status, (char *)N_GETPTR(R_ES, R_DX),
	       sizeof(mouse_status));
	break;

    case 0x18:	/* Install alternate handler */
	mask = R_CX & 0xff;
	if ((R_CX & 0xe0) == 0x00 ||
	    mask == mouse_status.altmask[0] ||
	    mask == mouse_status.altmask[1] ||
	    mask == mouse_status.altmask[2] ||
	    (mouse_status.altmask[i = 0] &&
	     mouse_status.altmask[i = 1] &&
	     mouse_status.altmask[i = 2])) {
	    R_AX = 0xffff;
	    break;
	}
	mouse_status.altmask[i] = R_CX;
	mouse_status.althandler[i] = N_GETVEC(R_ES, R_DX);
	break;

    case 0x19:	/* Determine address of alternate event handler */
	mask = R_CX & 0xff;
	if (mask == mouse_status.altmask[0])
	    vec = mouse_status.althandler[0];
	else if (mask == mouse_status.altmask[1])
	    vec = mouse_status.althandler[1];
	else if (mask == mouse_status.altmask[2])
	    vec = mouse_status.althandler[2];
	else
	    R_CX = 0;
	N_PUTVEC(R_ES, R_DX, vec);
	break;

    case 0x1a:	/* set mouse sensitivity */
	mouse_status.hmickey = R_BX;
	mouse_status.vmickey = R_CX;
	mouse_status.doubling = R_DX;
	break;

    case 0x1b:	/* set mouse sensitivity */
	R_BX = mouse_status.hmickey;
	R_CX = mouse_status.vmickey;
	R_DX = mouse_status.doubling;
	break;

    case 0x1c:	/* set mouse hardware rate */
    case 0x1d:	/* set display page */
	break;

    case 0x1e:	/* get display page */
	R_BX = 0;	/* Always on display page 0 */
	break;

    case 0x1f:	/* Disable mouse driver */
	if (mouse_status.installed) {
	    N_PUTVEC(R_ES, R_DX, mouse_status.handler);
	    mouse_status.installed = 0;
	} else {
	    R_AX = 0xffff;
	}
	break;

    case 0x20:	/* Enable mouse driver */
	mouse_status.installed = 1;
	break;

    case 0x21:	/* Reset mouse driver */
	if (mouse_status.installed) {
	    mouse_status.show = 0;
	    mouse_status.handler = 0;
	    mouse_status.mask = 0;
	    mouse_status.cursor = 0;
	} else {
	    R_AX = 0xffff;
	}
	break;

    case 0x22:	/* Specified language for mouse messages */
	break;

    case 0x23:	/* Get language number */
	R_BX = 0;	/* Always return english */
	break;

    case 0x24:	/* Get mouse type */
	R_CX = 0x0400;		/* PS/2 style mouse */
	R_BX = 0x0600 + 24;	/* Version 6.24 */
	break;

    default:
	R_FLAGS |= PSL_C;
	break;
    }
}

void
mouse_init(void)
{
    u_long vec;

    vec = insert_softint_trampoline();
    ivec[0x33] = vec;
    register_callback(vec, int33, "int 33");
    
    mouse_area[1] = 24;
}
