/*-
 * Copyright 1997 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: util.c,v 1.6 1998/02/20 05:08:53 jb Exp $
 */

#include <stdio.h>

/*
 * Not much left of the original MIT code, but it's still derived from it
 * so I'll keep their copyright. This is taken from util.c in MIT fetch.
 *
 *					-- DES 1998/05/22
 */

/*
 * Implement the `base64' encoding as described in RFC 1521.
 */
static const char base64[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int
fprint64(FILE *f, const unsigned char *buf)
{
    int len = 0, l = 0;
    unsigned int tmp;

    while (buf[len])
	len++;
	   
    while (len >= 3) {
	tmp = buf[0] << 16 | buf[1] << 8 | buf[2];
	fprintf(f, "%c%c%c%c",
		base64[(tmp >> 18) & 077],
		base64[(tmp >> 12) & 077],
		base64[(tmp >> 6) & 077],
		base64[tmp & 077]);
	len -= 3;
	buf += 3;
	l += 4;
    }

    /* RFC 1521 enumerates these three possibilities... */
    switch(len) {
    case 2:
	tmp = buf[0] << 16 | buf[1] << 8;
	fprintf(f, "%c%c%c=",
		base64[(tmp >> 18) & 077],
		base64[(tmp >> 12) & 077],
		base64[(tmp >> 6) & 077]);
	l += 4;
	break;
    case 1:
	tmp = buf[0] << 16;
	fprintf(f, "%c%c==",
		base64[(tmp >> 18) & 077],
		base64[(tmp >> 12) & 077]);
	l += 4;
	break;
    case 0:
	break;
    }

    return l;
}
