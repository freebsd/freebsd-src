/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	From:	@(#)nfscompress.h	7.2 (Berkeley) 10/2/90
 *	$Id: nfscompress.h,v 1.2 1993/09/09 22:06:16 rgrimes Exp $
 */

#ifndef __h_nfscompress
#define __h_nfscompress 1

/*
 * Definitions for the compression algorithm
 */
#define NFSC_MAX	17
#define	NFSCRL		0xe0
#define	NFSCRLE(a)	(NFSCRL | ((a) - 2))

#define	nfscput(c) \
		if (oleft == 0) { \
			MGET(om, M_WAIT, MT_DATA); \
			if (clget) \
				MCLGET(om, M_WAIT); \
			om->m_len = 0; \
			oleft = M_TRAILINGSPACE(om) - 1; \
			*mp = om; \
			mp = &om->m_next; \
			op = mtod(om, u_char *); \
		} else \
			oleft--; \
		*op++ = (c); \
		om->m_len++; \
		olen++

#define nfscget(c) \
		if (ileft == 0) { \
			do { \
				m = m->m_next; \
			} while (m && m->m_len == 0); \
			if (m) { \
				ileft = m->m_len - 1; \
				ip = mtod(m, u_char *); \
				(c) = *ip++; \
			} else { \
				(c) = '\0'; \
				noteof = 0; \
			} \
		} else { \
			(c) = *ip++; \
			ileft--; \
		}

#endif /* __h_nfscompress */
