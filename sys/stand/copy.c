/*-
 * Copyright (c) 1982, 1986, 1988 The Regents of the University of California.
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
 *
 *	from: @(#)copy.c	7.7 (Berkeley) 5/21/91
 *	$Id: copy.c,v 1.2 1993/10/16 19:31:29 rgrimes Exp $
 */

#define	BSIZE	10240

/*
 * Copy from from to to.  Intended for use in system installation.
 */
main()
{
	extern int errno;
	register int from, to, record, rcc, wcc, bsize = BSIZE;
	char buf[BSIZE];

	from = getfile("From", 0);
	to = getfile("To", 1);
	for (record = 0;; ++record) {
		if (!(rcc = read(from, buf, bsize)))
			break;
		if (rcc < 0) {
			printf("Record %d: read error, errno=%d\n",
			    record, errno);
			break;
		}
		if (rcc != bsize) {
			if (record == 0) {
				bsize = rcc;
				printf("Block size set from input; %d bytes\n",
				    bsize);
			} else
				printf("Record %d: read short; expected %d, got %d\n",
				    record, bsize, rcc);
		}
#ifdef vax
		/* For bug in ht driver. */
		if (rcc > bsize)
			rcc = bsize;
#endif
		if ((wcc = write(to, buf, rcc)) < 0) {
			printf("Record %d: write error: errno=%d\n",
			    record, errno);
			break;
		}
		if (wcc < rcc) {
			printf("Record %d: write short; expected %d, got %d\n",
			    record, rcc, wcc);
			break;
		}
	}
	printf("copy completed: %d records copied\n", record);
}
