/*-
 * Copyright (c) 1992-1995 Søren Schmidt
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 * This is a modified version of the keyboard reset code used in syscons.
 * If the keyboard reset fails, we assume that the keyboard has been
 * unplugged and we use a serial port (COM1) as the console instead.
 * Returns 1 on failure (no keyboard), 0 on success (keyboard attached).
 *
 * This grody hack brought to you by Bill Paul (wpaul@ctr.columbia.edu)
 *
 *	$Id: probe_keyboard.c,v 1.2 1995/02/15 04:17:59 rich Exp $
 */

#include <machine/console.h>
#include <machine/cpufunc.h>

#if BOOTWAIT
extern int delay1ms(void);
#else
int delay1ms()
{
        int i = 800;
        while (--i >= 0)
                (void)inb(0x84);
}
#endif

int
probe_keyboard(void)
{
	int i, retries = 5;
	unsigned char val;

	/* flush any noise in the buffer */
	while (inb(KB_STAT) & KB_BUF_FULL) {
		delay1ms();
		(void) inb(KB_DATA);
	}

	/* Try to reset keyboard hardware */
	while (retries--) {
		outb(KB_DATA, KB_RESET);
		for (i=0; i<100000; i++) {
			delay1ms();
			val = inb(KB_DATA);
			if (val == KB_ACK || val == KB_ECHO)
				goto gotres;
			if (val == KB_RESEND)
				break;
		}
	}
gotres:
	if (!retries)
		return(1);
	else {
gotack:
	delay1ms();
	while ((inb(KB_STAT) & KB_BUF_FULL) == 0) delay1ms();
	delay1ms();
	val = inb(KB_DATA);
	if (val == KB_ACK)
		goto gotack;
	if (val != KB_RESET_DONE) 
		return(1);
	}

	return(0);
}
