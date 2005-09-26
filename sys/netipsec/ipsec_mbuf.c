/*-
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
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
 * $FreeBSD$
 */

/*
 * IPsec-specific mbuf routines.
 */

#include "opt_param.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/route.h>
#include <netinet/in.h>

#include <netipsec/ipsec.h>

/*
 * Create a writable copy of the mbuf chain.  While doing this
 * we compact the chain with a goal of producing a chain with
 * at most two mbufs.  The second mbuf in this chain is likely
 * to be a cluster.  The primary purpose of this work is to create
 * a writable packet for encryption, compression, etc.  The
 * secondary goal is to linearize the data so the data can be
 * passed to crypto hardware in the most efficient manner possible.
 */
struct mbuf *
m_clone(struct mbuf *m0)
{
	struct mbuf *m, *mprev;
	struct mbuf *n, *mfirst, *mlast;
	int len, off;

	IPSEC_ASSERT(m0 != NULL, ("null mbuf"));

	mprev = NULL;
	for (m = m0; m != NULL; m = mprev->m_next) {
		/*
		 * Regular mbufs are ignored unless there's a cluster
		 * in front of it that we can use to coalesce.  We do
		 * the latter mainly so later clusters can be coalesced
		 * also w/o having to handle them specially (i.e. convert
		 * mbuf+cluster -> cluster).  This optimization is heavily
		 * influenced by the assumption that we're running over
		 * Ethernet where MCLBYTES is large enough that the max
		 * packet size will permit lots of coalescing into a
		 * single cluster.  This in turn permits efficient
		 * crypto operations, especially when using hardware.
		 */
		if ((m->m_flags & M_EXT) == 0) {
			if (mprev && (mprev->m_flags & M_EXT) &&
			    m->m_len <= M_TRAILINGSPACE(mprev)) {
				/* XXX: this ignores mbuf types */
				memcpy(mtod(mprev, caddr_t) + mprev->m_len,
				       mtod(m, caddr_t), m->m_len);
				mprev->m_len += m->m_len;
				mprev->m_next = m->m_next;	/* unlink from chain */
				m_free(m);			/* reclaim mbuf */
				newipsecstat.ips_mbcoalesced++;
			} else {
				mprev = m;
			}
			continue;
		}
		/*
		 * Writable mbufs are left alone (for now).
		 */
		if (M_WRITABLE(m)) {
			mprev = m;
			continue;
		}

		/*
		 * Not writable, replace with a copy or coalesce with
		 * the previous mbuf if possible (since we have to copy
		 * it anyway, we try to reduce the number of mbufs and
		 * clusters so that future work is easier).
		 */
		IPSEC_ASSERT(m->m_flags & M_EXT, ("m_flags 0x%x", m->m_flags));
		/* NB: we only coalesce into a cluster or larger */
		if (mprev != NULL && (mprev->m_flags & M_EXT) &&
		    m->m_len <= M_TRAILINGSPACE(mprev)) {
			/* XXX: this ignores mbuf types */
			memcpy(mtod(mprev, caddr_t) + mprev->m_len,
			       mtod(m, caddr_t), m->m_len);
			mprev->m_len += m->m_len;
			mprev->m_next = m->m_next;	/* unlink from chain */
			m_free(m);			/* reclaim mbuf */
			newipsecstat.ips_clcoalesced++;
			continue;
		}

		/*
		 * Allocate new space to hold the copy...
		 */
		/* XXX why can M_PKTHDR be set past the first mbuf? */
		if (mprev == NULL && (m->m_flags & M_PKTHDR)) {
			/*
			 * NB: if a packet header is present we must
			 * allocate the mbuf separately from any cluster
			 * because M_MOVE_PKTHDR will smash the data
			 * pointer and drop the M_EXT marker.
			 */
			MGETHDR(n, M_DONTWAIT, m->m_type);
			if (n == NULL) {
				m_freem(m0);
				return (NULL);
			}
			M_MOVE_PKTHDR(n, m);
			MCLGET(n, M_DONTWAIT);
			if ((n->m_flags & M_EXT) == 0) {
				m_free(n);
				m_freem(m0);
				return (NULL);
			}
		} else {
			n = m_getcl(M_DONTWAIT, m->m_type, m->m_flags);
			if (n == NULL) {
				m_freem(m0);
				return (NULL);
			}
		}
		/*
		 * ... and copy the data.  We deal with jumbo mbufs
		 * (i.e. m_len > MCLBYTES) by splitting them into
		 * clusters.  We could just malloc a buffer and make
		 * it external but too many device drivers don't know
		 * how to break up the non-contiguous memory when
		 * doing DMA.
		 */
		len = m->m_len;
		off = 0;
		mfirst = n;
		mlast = NULL;
		for (;;) {
			int cc = min(len, MCLBYTES);
			memcpy(mtod(n, caddr_t), mtod(m, caddr_t) + off, cc);
			n->m_len = cc;
			if (mlast != NULL)
				mlast->m_next = n;
			mlast = n;	
			newipsecstat.ips_clcopied++;

			len -= cc;
			if (len <= 0)
				break;
			off += cc;

			n = m_getcl(M_DONTWAIT, m->m_type, m->m_flags);
			if (n == NULL) {
				m_freem(mfirst);
				m_freem(m0);
				return (NULL);
			}
		}
		n->m_next = m->m_next; 
		if (mprev == NULL)
			m0 = mfirst;		/* new head of chain */
		else
			mprev->m_next = mfirst;	/* replace old mbuf */
		m_free(m);			/* release old mbuf */
		mprev = mfirst;
	}
	return (m0);
}

/*
 * Make space for a new header of length hlen at skip bytes
 * into the packet.  When doing this we allocate new mbufs only
 * when absolutely necessary.  The mbuf where the new header
 * is to go is returned together with an offset into the mbuf.
 * If NULL is returned then the mbuf chain may have been modified;
 * the caller is assumed to always free the chain.
 */
struct mbuf *
m_makespace(struct mbuf *m0, int skip, int hlen, int *off)
{
	struct mbuf *m;
	unsigned remain;

	IPSEC_ASSERT(m0 != NULL, ("null mbuf"));
	IPSEC_ASSERT(hlen < MHLEN, ("hlen too big: %u", hlen));

	for (m = m0; m && skip > m->m_len; m = m->m_next)
		skip -= m->m_len;
	if (m == NULL)
		return (NULL);
	/*
	 * At this point skip is the offset into the mbuf m
	 * where the new header should be placed.  Figure out
	 * if there's space to insert the new header.  If so,
	 * and copying the remainder makese sense then do so.
	 * Otherwise insert a new mbuf in the chain, splitting
	 * the contents of m as needed.
	 */
	remain = m->m_len - skip;		/* data to move */
	if (hlen > M_TRAILINGSPACE(m)) {
		struct mbuf *n;

		/* XXX code doesn't handle clusters XXX */
		IPSEC_ASSERT(remain < MLEN, ("remainder too big: %u", remain));
		/*
		 * Not enough space in m, split the contents
		 * of m, inserting new mbufs as required.
		 *
		 * NB: this ignores mbuf types.
		 */
		MGET(n, M_DONTWAIT, MT_DATA);
		if (n == NULL)
			return (NULL);
		n->m_next = m->m_next;		/* splice new mbuf */
		m->m_next = n;
		newipsecstat.ips_mbinserted++;
		if (hlen <= M_TRAILINGSPACE(m) + remain) {
			/*
			 * New header fits in the old mbuf if we copy
			 * the remainder; just do the copy to the new
			 * mbuf and we're good to go.
			 */
			memcpy(mtod(n, caddr_t),
			       mtod(m, caddr_t) + skip, remain);
			n->m_len = remain;
			m->m_len = skip + hlen;
			*off = skip;
		} else {
			/*
			 * No space in the old mbuf for the new header.
			 * Make space in the new mbuf and check the
			 * remainder'd data fits too.  If not then we
			 * must allocate an additional mbuf (yech).
			 */
			n->m_len = 0;
			if (remain + hlen > M_TRAILINGSPACE(n)) {
				struct mbuf *n2;

				MGET(n2, M_DONTWAIT, MT_DATA);
				/* NB: new mbuf is on chain, let caller free */
				if (n2 == NULL)
					return (NULL);
				n2->m_len = 0;
				memcpy(mtod(n2, caddr_t),
				       mtod(m, caddr_t) + skip, remain);
				n2->m_len = remain;
				/* splice in second mbuf */
				n2->m_next = n->m_next;
				n->m_next = n2;
				newipsecstat.ips_mbinserted++;
			} else {
				memcpy(mtod(n, caddr_t) + hlen,
				       mtod(m, caddr_t) + skip, remain);
				n->m_len += remain;
			}
			m->m_len -= remain;
			n->m_len += hlen;
			m = n;			/* header is at front ... */
			*off = 0;		/* ... of new mbuf */
		}
	} else {
		/*
		 * Copy the remainder to the back of the mbuf
		 * so there's space to write the new header.
		 */
		bcopy(mtod(m, caddr_t) + skip,
		      mtod(m, caddr_t) + skip + hlen, remain);
		m->m_len += hlen;
		*off = skip;
	}
	m0->m_pkthdr.len += hlen;		/* adjust packet length */
	return m;
}

/*
 * m_pad(m, n) pads <m> with <n> bytes at the end. The packet header
 * length is updated, and a pointer to the first byte of the padding
 * (which is guaranteed to be all in one mbuf) is returned.
 */
caddr_t
m_pad(struct mbuf *m, int n)
{
	register struct mbuf *m0, *m1;
	register int len, pad;
	caddr_t retval;

	if (n <= 0) {  /* No stupid arguments. */
		DPRINTF(("%s: pad length invalid (%d)\n", __func__, n));
		m_freem(m);
		return NULL;
	}

	len = m->m_pkthdr.len;
	pad = n;
	m0 = m;

	while (m0->m_len < len) {
		len -= m0->m_len;
		m0 = m0->m_next;
	}

	if (m0->m_len != len) {
		DPRINTF(("%s: length mismatch (should be %d instead of %d)\n",
			__func__, m->m_pkthdr.len,
			m->m_pkthdr.len + m0->m_len - len));

		m_freem(m);
		return NULL;
	}

	/* Check for zero-length trailing mbufs, and find the last one. */
	for (m1 = m0; m1->m_next; m1 = m1->m_next) {
		if (m1->m_next->m_len != 0) {
			DPRINTF(("%s: length mismatch (should be %d instead "
				"of %d)\n", __func__,
				m->m_pkthdr.len,
				m->m_pkthdr.len + m1->m_next->m_len));

			m_freem(m);
			return NULL;
		}

		m0 = m1->m_next;
	}

	if (pad > M_TRAILINGSPACE(m0)) {
		/* Add an mbuf to the chain. */
		MGET(m1, M_DONTWAIT, MT_DATA);
		if (m1 == 0) {
			m_freem(m0);
			DPRINTF(("%s: unable to get extra mbuf\n", __func__));
			return NULL;
		}

		m0->m_next = m1;
		m0 = m1;
		m0->m_len = 0;
	}

	retval = m0->m_data + m0->m_len;
	m0->m_len += pad;
	m->m_pkthdr.len += pad;

	return retval;
}

/*
 * Remove hlen data at offset skip in the packet.  This is used by
 * the protocols strip protocol headers and associated data (e.g. IV,
 * authenticator) on input.
 */
int
m_striphdr(struct mbuf *m, int skip, int hlen)
{
	struct mbuf *m1;
	int roff;

	/* Find beginning of header */
	m1 = m_getptr(m, skip, &roff);
	if (m1 == NULL)
		return (EINVAL);

	/* Remove the header and associated data from the mbuf. */
	if (roff == 0) {
		/* The header was at the beginning of the mbuf */
		newipsecstat.ips_input_front++;
		m_adj(m1, hlen);
		if ((m1->m_flags & M_PKTHDR) == 0)
			m->m_pkthdr.len -= hlen;
	} else if (roff + hlen >= m1->m_len) {
		struct mbuf *mo;

		/*
		 * Part or all of the header is at the end of this mbuf,
		 * so first let's remove the remainder of the header from
		 * the beginning of the remainder of the mbuf chain, if any.
		 */
		newipsecstat.ips_input_end++;
		if (roff + hlen > m1->m_len) {
			/* Adjust the next mbuf by the remainder */
			m_adj(m1->m_next, roff + hlen - m1->m_len);

			/* The second mbuf is guaranteed not to have a pkthdr... */
			m->m_pkthdr.len -= (roff + hlen - m1->m_len);
		}

		/* Now, let's unlink the mbuf chain for a second...*/
		mo = m1->m_next;
		m1->m_next = NULL;

		/* ...and trim the end of the first part of the chain...sick */
		m_adj(m1, -(m1->m_len - roff));
		if ((m1->m_flags & M_PKTHDR) == 0)
			m->m_pkthdr.len -= (m1->m_len - roff);

		/* Finally, let's relink */
		m1->m_next = mo;
	} else {
		/*
		 * The header lies in the "middle" of the mbuf; copy
		 * the remainder of the mbuf down over the header.
		 */
		newipsecstat.ips_input_middle++;
		bcopy(mtod(m1, u_char *) + roff + hlen,
		      mtod(m1, u_char *) + roff,
		      m1->m_len - (roff + hlen));
		m1->m_len -= hlen;
		m->m_pkthdr.len -= hlen;
	}
	return (0);
}

/*
 * Diagnostic routine to check mbuf alignment as required by the
 * crypto device drivers (that use DMA).
 */
void
m_checkalignment(const char* where, struct mbuf *m0, int off, int len)
{
	int roff;
	struct mbuf *m = m_getptr(m0, off, &roff);
	caddr_t addr;

	if (m == NULL)
		return;
	printf("%s (off %u len %u): ", where, off, len);
	addr = mtod(m, caddr_t) + roff;
	do {
		int mlen;

		if (((uintptr_t) addr) & 3) {
			printf("addr misaligned %p,", addr);
			break;
		}
		mlen = m->m_len;
		if (mlen > len)
			mlen = len;
		len -= mlen;
		if (len && (mlen & 3)) {
			printf("len mismatch %u,", mlen);
			break;
		}
		m = m->m_next;
		addr = m ? mtod(m, caddr_t) : NULL;
	} while (m && len > 0);
	for (m = m0; m; m = m->m_next)
		printf(" [%p:%u]", mtod(m, caddr_t), m->m_len);
	printf("\n");
}
