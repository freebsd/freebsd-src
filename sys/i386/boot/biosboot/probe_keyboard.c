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
 *	$Id: probe_keyboard.c,v 1.9 1996/11/24 08:06:01 peter Exp $
 */

#ifdef PROBE_KEYBOARD

#include <machine/console.h>
#include <machine/cpufunc.h>
#include <i386/isa/kbdio.h>
#include <i386/isa/isa.h>
#include "boot.h"

int
probe_keyboard(void)
{
	int i, retries = 5;
	unsigned char val;

	/* flush any noise in the buffer */
	while (inb(IO_KBD + KBD_STATUS_PORT) & KBDS_BUFFER_FULL) {
		delay1ms();
		(void) inb(IO_KBD + KBD_DATA_PORT);
	}

	/* Try to reset keyboard hardware */
  again:
	while (--retries) {
#ifdef DEBUG
		printf("%d ", retries);
#endif
		while ((inb(IO_KBD + KBD_STATUS_PORT) & KBDS_INPUT_BUFFER_FULL)
			== KBDS_INPUT_BUFFER_FULL)
			delay1ms();
		outb(IO_KBD + KBD_DATA_PORT, KBDC_RESET_KBD);
		for (i=0; i<1000; i++) {
			delay1ms();
			val = inb(IO_KBD + KBD_DATA_PORT);
			if (val == KBD_ACK || val == KBD_ECHO)
				goto gotack;
			if (val == KBD_RESEND)
				break;
		}
	}
gotres:
#ifdef DEBUG
	printf("gotres\n");
#endif
	if (!retries) {
#ifdef DEBUG
		printf("gave up\n");
#endif
		return(1);
	}
gotack:
	delay1ms();
	while ((inb(IO_KBD + KBD_STATUS_PORT) & KBDS_KBD_BUFFER_FULL) == 0)
		delay1ms();
	delay1ms();
#ifdef DEBUG
	printf("ACK ");
#endif
	val = inb(IO_KBD + KBD_DATA_PORT);
	if (val == KBD_ACK)
		goto gotack;
	if (val == KBD_RESEND)
		goto again;
	if (val != KBD_RESET_DONE) {
#ifdef DEBUG
		printf("stray val %d\n", val);
#endif
		return(0);
	}
#ifdef DEBUG
	printf("ok\n");
#endif
	return(0);
}

#endif /* PROBE_KEYBOARD */
