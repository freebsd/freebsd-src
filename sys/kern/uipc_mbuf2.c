/*	$FreeBSD$	*/
/*	$KAME: uipc_mbuf2.c,v 1.15 2000/02/22 14:01:37 itojun Exp $	*/
/*	$NetBSD: uipc_mbuf.c,v 1.40 1999/04/01 00:23:25 thorpej Exp $	*/

/*
 * Copyright (C) 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)uipc_mbuf.c	8.4 (Berkeley) 2/14/95
 */

/*#define PULLDOWN_DEBUG*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>

/*
 * ensure that [off, off + len) is contiguous on the mbuf chain "m".
 * packet chain before "off" is kept untouched.
 * if offp == NULL, the target will start at <retval, 0> on resulting chain.
 * if offp != NULL, the target will start at <retval, *offp> on resulting chain.
 *
 * on error return (NULL return value), original "m" will be freed.
 *
 * XXX: M_TRAILINGSPACE/M_LEADINGSPACE only permitted on writable ext_buf.
 */
struct mbuf *
m_pulldown(m, off, len, offp)
	struct mbuf *m;
	int off, len;
	int *offp;
{
	struct mbuf *n, *o;
	int hlen, tlen, olen;
	int writable;

	/* check invalid arguments. */
	if (m == NULL)
		panic("m == NULL in m_pulldown()");
	if (len > MCLBYTES) {
		m_freem(m);
		return NULL;	/* impossible */
	}

#ifdef PULLDOWN_DEBUG
    {
	struct mbuf *t;
	printf("before:");
	for (t = m; t; t = t->m_next)
		printf(" %d", t->m_len);
	printf("\n");
    }
#endif
	n = m;
	while (n != NULL && off > 0) {
		if (n->m_len > off)
			break;
		off -= n->m_len;
		n = n->m_next;
	}
	/* be sure to point non-empty mbuf */
	while (n != NULL && n->m_len == 0)
		n = n->m_next;
	if (!n) {
		m_freem(m);
		return NULL;	/* mbuf chain too short */
	}

	/*
	 * the target data is on <n, off>.
	 * if we got enough data on the mbuf "n", we're done.
	 */
	if ((off == 0 || offp) && len <= n->m_len - off)
		goto ok;

	/*
	 * when len < n->m_len - off and off != 0, it is a special case.
	 * len bytes from <n, off> sits in single mbuf, but the caller does
	 * not like the starting position (off).
	 * chop the current mbuf into two pieces, set off to 0.
	 */
	if (len < n->m_len - off) {
		o = m_copym(n, off, n->m_len - off, M_DONTWAIT);
		if (o == NULL) {
			m_freem(m);
			return NULL;	/* ENOBUFS */
		}
		n->m_len = off;
		o->m_next = n->m_next;
		n->m_next = o;
		n = n->m_next;
		off = 0;
		goto ok;
	}

	/*
	 * we need to take hlen from <n, off> and tlen from <n->m_next, 0>,
	 * and construct contiguous mbuf with m_len == len.
	 * note that hlen + tlen == len, and tlen > 0.
	 */
	hlen = n->m_len - off;
	tlen = len - hlen;

	/*
	 * ensure that we have enough trailing data on mbuf chain.
	 * if not, we can do nothing about the chain.
	 */
	olen = 0;
	for (o = n->m_next; o != NULL; o = o->m_next)
		olen += o->m_len;
	if (hlen + olen < len) {
		m_freem(m);
		return NULL;	/* mbuf chain too short */
	}

	/*
	 * easy cases first.
	 * we need to use m_copydata() to get data from <n->m_next, 0>.
	 */
	/*
	 * XXX: This code is flawed because it considers a "writable" mbuf
	 *      data region to require all of the following:
	 *	  (i) mbuf _has_ to have M_EXT set; if it is just a regular
	 *	      mbuf, it is still not considered "writable."
	 *	  (ii) since mbuf has M_EXT, the ext_type _has_ to be
	 *	       EXT_CLUSTER. Anything else makes it non-writable.
	 *	  (iii) M_WRITABLE() must evaluate true.
	 *      Ideally, the requirement should only be (iii).
	 *
	 * If we're writable, we're sure we're writable, because the ref. count
	 * cannot increase from 1, as that would require posession of mbuf
	 * n by someone else (which is impossible). However, if we're _not_
	 * writable, we may eventually become writable )if the ref. count drops
	 * to 1), but we'll fail to notice it unless we re-evaluate
	 * M_WRITABLE(). For now, we only evaluate once at the beginning and
	 * live with this.
	 */
	/*
	 * XXX: This is dumb. If we're just a regular mbuf with no M_EXT,
	 *      then we're not "writable," according to this code.
	 */
	writable = 0;
	if ((n->m_flags & M_EXT) && (n->m_ext.ext_type == EXT_CLUSTER) &&
	    M_WRITABLE(n))
		writable = 1;

	if ((off == 0 || offp) && M_TRAILINGSPACE(n) >= tlen
	 && writable) {
		m_copydata(n->m_next, 0, tlen, mtod(n, caddr_t) + n->m_len);
		n->m_len += tlen;
		m_adj(n->m_next, tlen);
		goto ok;
	}
	if ((off == 0 || offp) && M_LEADINGSPACE(n->m_next) >= hlen
	 && writable) {
		n->m_next->m_data -= hlen;
		n->m_next->m_len += hlen;
		bcopy(mtod(n, caddr_t) + off, mtod(n->m_next, caddr_t), hlen);
		n->m_len -= hlen;
		n = n->m_next;
		off = 0;
		goto ok;
	}

	/*
	 * now, we need to do the hard way.  don't m_copy as there's no room
	 * on both end.
	 */
	MGET(o, M_DONTWAIT, m->m_type);
	if (o == NULL) {
		m_freem(m);
		return NULL;	/* ENOBUFS */
	}
	if (len > MHLEN) {	/* use MHLEN just for safety */
		MCLGET(o, M_DONTWAIT);
		if ((o->m_flags & M_EXT) == 0) {
			m_freem(m);
			m_free(o);
			return NULL;	/* ENOBUFS */
		}
	}
	/* get hlen from <n, off> into <o, 0> */
	o->m_len = hlen;
	bcopy(mtod(n, caddr_t) + off, mtod(o, caddr_t), hlen);
	n->m_len -= hlen;
	/* get tlen from <n->m_next, 0> into <o, hlen> */
	m_copydata(n->m_next, 0, tlen, mtod(o, caddr_t) + o->m_len);
	o->m_len += tlen;
	m_adj(n->m_next, tlen);
	o->m_next = n->m_next;
	n->m_next = o;
	n = o;
	off = 0;

ok:
#ifdef PULLDOWN_DEBUG
    {
	struct mbuf *t;
	printf("after:");
	for (t = m; t; t = t->m_next)
		printf("%c%d", t == n ? '*' : ' ', t->m_len);
	printf(" (off=%d)\n", off);
    }
#endif
	if (offp)
		*offp = off;
	return n;
}

/*
 * pkthdr.aux chain manipulation.
 * we don't allow clusters at this moment. 
 */
struct mbuf *
m_aux_add(m, af, type)
	struct mbuf *m;
	int af, type;
{
	struct mbuf *n;
	struct mauxtag *t;

	if ((m->m_flags & M_PKTHDR) == 0)
		return NULL;

	n = m_aux_find(m, af, type);
	if (n)
		return n;

	MGET(n, M_DONTWAIT, m->m_type);
	if (n == NULL)
		return NULL;

	t = mtod(n, struct mauxtag *);
	t->af = af;
	t->type = type;
	n->m_data += sizeof(struct mauxtag);
	n->m_len = 0;
	n->m_next = m->m_pkthdr.aux;
	m->m_pkthdr.aux = n;
	return n;
}

struct mbuf *
m_aux_find(m, af, type)
	struct mbuf *m;
	int af, type;
{
	struct mbuf *n;
	struct mauxtag *t;

	if ((m->m_flags & M_PKTHDR) == 0)
		return NULL;

	for (n = m->m_pkthdr.aux; n; n = n->m_next) {
		t = (struct mauxtag *)n->m_dat;
		if (t->af == af && t->type == type)
			return n;
	}
	return NULL;
}

void
m_aux_delete(m, victim)
	struct mbuf *m;
	struct mbuf *victim;
{
	struct mbuf *n, *prev, *next;
	struct mauxtag *t;

	if ((m->m_flags & M_PKTHDR) == 0)
		return;

	prev = NULL;
	n = m->m_pkthdr.aux;
	while (n) {
		t = (struct mauxtag *)n->m_dat;
		next = n->m_next;
		if (n == victim) {
			if (prev)
				prev->m_next = n->m_next;
			else
				m->m_pkthdr.aux = n->m_next;
			n->m_next = NULL;
			m_free(n);
		} else
			prev = n;
		n = next;
	}
}
