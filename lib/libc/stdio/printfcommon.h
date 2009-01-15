/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $FreeBSD$
 */

/*
 * This file defines common routines used by both printf and wprintf.
 * You must define CHAR to either char or wchar_t prior to including this.
 */

#define NIOV 8
struct io_state {
	FILE *fp;
	struct __suio uio;	/* output information: summary */
	struct __siov iov[NIOV];/* ... and individual io vectors */
	struct __siov *iovp;	/* pointer to next free slot in iov */
};

static inline void
io_init(struct io_state *iop, FILE *fp)
{

	iop->uio.uio_iov = iop->iovp = iop->iov;
	iop->uio.uio_resid = 0;
	iop->uio.uio_iovcnt = 0;
	iop->fp = fp;
}

/*
 * WARNING: The buffer passed to io_print() is not copied immediately; it must
 * remain valid until io_flush() is called.
 */
static inline int
io_print(struct io_state *iop, const CHAR * __restrict ptr, int len)
{

	iop->iovp->iov_base = (char *)ptr;
	iop->iovp->iov_len = len;
	iop->uio.uio_resid += len;
	iop->iovp++;
	if (++iop->uio.uio_iovcnt >= NIOV) {
		iop->iovp = iop->iov;
		return (__sprint(iop->fp, &iop->uio));
	}
	return (0);
}

/*
 * Choose PADSIZE to trade efficiency vs. size.  If larger printf
 * fields occur frequently, increase PADSIZE and make the initialisers
 * below longer.
 */
#define	PADSIZE	16		/* pad chunk size */
static const CHAR blanks[PADSIZE] =
{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
static const CHAR zeroes[PADSIZE] =
{'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'};

/*
 * Pad with blanks or zeroes. 'with' should point to either the blanks array
 * or the zeroes array.
 */
static inline int
io_pad(struct io_state *iop, int howmany, const CHAR * __restrict with)
{

	while (howmany > PADSIZE) {
		if (io_print(iop, with, PADSIZE))
			return (-1);
		howmany -= PADSIZE;
	}
	if (howmany > 0 && io_print(iop, with, howmany))
		return (-1);
	return (0);
}

/*
 * Print exactly len characters of the string spanning p to ep, truncating
 * or padding with 'with' as necessary.
 */
static inline int
io_printandpad(struct io_state *iop, const CHAR *p, const CHAR *ep,
	       int len, const CHAR * __restrict with)
{
	int p_len;

	p_len = ep - p;
	if (p_len > len)
		p_len = len;
	if (p_len > 0 && io_print(iop, p, p_len))
		return (-1);
	return (io_pad(iop, len - (p_len > 0 ? p_len : 0), with));
}

static inline int
io_flush(struct io_state *iop)
{

	iop->iovp = iop->iov;
	return (__sprint(iop->fp, &iop->uio));
}
