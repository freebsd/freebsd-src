/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: elink.c,v 1.1 1994/05/25 20:06:40 ats Exp $
 */

/*
 * Common code for dealing with 3COM ethernet cards.
 */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <machine/pio.h>
#include <i386/isa/elink.h>

/*
 * Issue a `global reset' to all cards.  We have to be careful to do this only
 * once during autoconfig, to prevent resetting boards that have already been
 * configured.
 */
void
elink_reset()
{  
	static int x = 0;

	if (x == 0) {
		x = 1;
		outb(ELINK_ID_PORT, ELINK_RESET);
	}
}

/*
 * The `ID sequence' is really just snapshots of an 8-bit CRC register as 0
 * bits are shifted in.  Different board types use different polynomials.
 */
void
elink_idseq(p)
	register u_char p;
{
	register int i;
	register u_char c;

	c = 0xff;
	for (i = 255; i; i--) {
		outb(ELINK_ID_PORT, c);
		if (c & 0x80) {
			c <<= 1;
			c ^= p;
		} else
			c <<= 1;
	}
}
