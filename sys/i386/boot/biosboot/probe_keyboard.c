/*-
 * Copyright (c) 1997 Kazutaka YOKOTA (yokota@zodiac.mech.utsunomiya-u.ac.jp)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id:$
 */

#include <sys/types.h>

#include <machine/cpufunc.h>

#include <i386/isa/isa.h>
#include <i386/isa/kbdio.h>

#include "boot.h"

#define PROBE_MAXRETRY	5
#define PROBE_MAXWAIT	400

#define IO_DUMMY	0x84

/* 7 microsec delay necessary for some keyboard controllers */
static void
delay7(void)
{
    /* 
     * I know this is broken, but no timer is avaiable yet at this stage...
     * See also comments in `delay1ms()' in `io.c'.
     */
    inb(IO_DUMMY); inb(IO_DUMMY);
    inb(IO_DUMMY); inb(IO_DUMMY);
    inb(IO_DUMMY); inb(IO_DUMMY);
}

/* 
 * Perform a simple test on the keyboard; issue the ECHO command and see
 * if the right answer is returned. We don't do anything as drastic as
 * full keyboard reset; it will be too troublesome and take too much time.
 */
int
probe_keyboard(void)
{
    int retry = PROBE_MAXRETRY;
    int wait;
    int i;

    while (--retry >= 0) {
	/* flush any noise */
	while (inb(IO_KBD + KBD_STATUS_PORT) & KBDS_ANY_BUFFER_FULL) {
	    delay7();
	    inb(IO_KBD + KBD_DATA_PORT);
	    delay1ms();
	}

	/* wait until the controller can accept a command */
	for (wait = PROBE_MAXWAIT; wait > 0; --wait) {
	    if (((i = inb(IO_KBD + KBD_STATUS_PORT)) 
                & (KBDS_INPUT_BUFFER_FULL | KBDS_ANY_BUFFER_FULL)) == 0)
		break;
	    if (i & KBDS_ANY_BUFFER_FULL) {
		delay7();
	        inb(IO_KBD + KBD_DATA_PORT);
	    }
	    delay1ms();
	}
	if (wait <= 0)
	    continue;

	/* send the ECHO command */
	outb(IO_KBD + KBD_DATA_PORT, KBDC_ECHO);

	/* wait for a response */
	for (wait = PROBE_MAXWAIT; wait > 0; --wait) {
	     if (inb(IO_KBD + KBD_STATUS_PORT) & KBDS_ANY_BUFFER_FULL)
		 break;
	     delay1ms();
	}
	if (wait <= 0)
	    continue;

	delay7();
	i = inb(IO_KBD + KBD_DATA_PORT);
#ifdef PROBE_KBD_BEBUG
        printf("probe_keyboard: got 0x%x.\n", i);
#endif
	if (i == KBD_ECHO) {
	    /* got the right answer */
	    return (0);
	}
    }

    return (1);
}
