/*
 * Copyright (c) 2002 Somebody. All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_param.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>

/*
 * Miscellaneous mbuf routines that belong in uipc_mbuf.c but
 * are kept private to the crypto support for now.
 */
extern	int m_apply(struct mbuf *m, int off, int len,
		    int (*f)(caddr_t, caddr_t, unsigned int), caddr_t fstate);
extern	struct mbuf * m_getptr(struct mbuf *m, int loc, int *off);

/*
 * Apply function f to the data in an mbuf chain starting "off" bytes from the
 * beginning, continuing for "len" bytes.
 */
int
m_apply(struct mbuf *m, int off, int len, int (*f)(caddr_t, caddr_t, unsigned int), caddr_t fstate)
{
	int rval;
	unsigned int count;

	if (len < 0)
		panic("m_apply: len %d < 0", len);
	if (off < 0)
		panic("m_apply: off %d < 0", off);
	while (off > 0) {
		if (m == NULL)
			panic("m_apply: null mbuf in skip");
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		if (m == NULL)
			panic("m_apply: null mbuf");
		count = min(m->m_len - off, len);

		rval = f(fstate, mtod(m, caddr_t) + off, count);
		if (rval)
			return (rval);

		len -= count;
		off = 0;
		m = m->m_next;
	}

	return (0);
}

/*
 * Return a pointer to mbuf/offset of location in mbuf chain.
 */
struct mbuf *
m_getptr(struct mbuf *m, int loc, int *off)
{
	while (loc >= 0) {
		/* Normal end of search */
		if (m->m_len > loc) {
	    		*off = loc;
	    		return (m);
		}
		else {
	    		loc -= m->m_len;

	    		if (m->m_next == NULL) {
				if (loc == 0) {
 					/* Point at the end of valid data */
		    			*off = m->m_len;
		    			return (m);
				}
				else
		  			return (NULL);
	    		}
	    		else
	      			m = m->m_next;
		}
    	}

	return (NULL);
}
