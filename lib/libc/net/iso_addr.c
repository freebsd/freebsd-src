/*
 * Copyright (c) 1989 Regents of the University of California.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)iso_addr.c	5.4 (Berkeley) 2/24/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <netiso/iso.h>
#include <string.h>

/* States*/
#define VIRGIN	0
#define GOTONE	1
#define GOTTWO	2
/* Inputs */
#define	DIGIT	(4*0)
#define	END	(4*1)
#define DELIM	(4*2)

struct iso_addr *
iso_addr(addr)
	register const char *addr;
{
	static struct iso_addr out_addr;
	register char *cp = out_addr.isoa_genaddr;
	char *cplim = cp + sizeof(out_addr.isoa_genaddr);
	register int byte = 0, state = VIRGIN, new;

	bzero((char *)&out_addr, sizeof(out_addr));
	do {
		if ((*addr >= '0') && (*addr <= '9')) {
			new = *addr - '0';
		} else if ((*addr >= 'a') && (*addr <= 'f')) {
			new = *addr - 'a' + 10;
		} else if ((*addr >= 'A') && (*addr <= 'F')) {
			new = *addr - 'A' + 10;
		} else if (*addr == 0) 
			state |= END;
		else
			state |= DELIM;
		addr++;
		switch (state /* | INPUT */) {
		case GOTTWO | DIGIT:
			*cp++ = byte; /*FALLTHROUGH*/
		case VIRGIN | DIGIT:
			state = GOTONE; byte = new; continue;
		case GOTONE | DIGIT:
			state = GOTTWO; byte = new + (byte << 4); continue;
		default: /* | DELIM */
			state = VIRGIN; *cp++ = byte; byte = 0; continue;
		case GOTONE | END:
		case GOTTWO | END:
			*cp++ = byte; /* FALLTHROUGH */
		case VIRGIN | END:
			break;
		}
		break;
	} while (cp < cplim); 
	out_addr.isoa_len = cp - out_addr.isoa_genaddr;
	return (&out_addr);
}
static char hexlist[] = "0123456789abcdef";

char *
iso_ntoa(isoa)
	const struct iso_addr *isoa;
{
	static char obuf[64];
	register char *out = obuf; 
	register int i;
	register u_char *in = (u_char *)isoa->isoa_genaddr;
	u_char *inlim = in + isoa->isoa_len;

	out[1] = 0;
	while (in < inlim) {
		i = *in++;
		*out++ = '.';
		if (i > 0xf) {
			out[1] = hexlist[i & 0xf];
			i >>= 4;
			out[0] = hexlist[i];
			out += 2;
		} else
			*out++ = hexlist[i];
	}
	*out = 0;
	return(obuf + 1);
}
