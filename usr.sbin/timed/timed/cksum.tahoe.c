/*
 * Copyright (c) 1986 Regents of the University of California.
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

#ifndef lint
static char sccsid[] = "@(#)cksum.tahoe.c	2.4 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/types.h>

/*
 * Checksum routine for Internet Protocol family headers.
 *
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 * 
 * This implementation is TAHOE version.
 */

#undef	ADDCARRY
#define ADDCARRY(sum) { \
	if (sum & 0xffff0000) {	\
		sum &= 0xffff; \
		sum++; \
	} \
}

in_cksum(addr, len)
	register u_short *addr;
	register int len;
{
	union word {
		char	c[2];
		u_short	s;
	} u;
	register int sum = 0;

	while (len > 0) {
		/*
		 * add by words.
		 */
		while ((len -= 2) >= 0) {
			if ((int)addr & 0x1) {
				/* word is not aligned */
				u.c[0] = *(char *)addr;
				u.c[1] = *((char *)addr+1);
				sum += u.s;
				addr++;
			} else
				sum += *addr++;
			ADDCARRY(sum);
		}
		if (len == -1)
			/*
			 * Odd number of bytes. 
			 */
			u.c[0] = *(u_char *)addr;
	}
	if (len == -1) {
		/* The last mbuf has odd # of bytes. Follow the
		   standard (the odd byte is shifted left by 8 bits) */
		u.c[1] = 0;
		sum += u.s;
		ADDCARRY(sum);
	}
	return (~sum & 0xffff);
}
