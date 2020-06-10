/*      $OpenBSD: criov.c,v 1.9 2002/01/29 15:48:29 jason Exp $	*/

/*-
 * Copyright (c) 1999 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/uio.h>
#include <sys/limits.h>
#include <sys/lock.h>

#include <opencrypto/cryptodev.h>

/*
 * This macro is only for avoiding code duplication, as we need to skip
 * given number of bytes in the same way in three functions below.
 */
#define	CUIO_SKIP()	do {						\
	KASSERT(off >= 0, ("%s: off %d < 0", __func__, off));		\
	KASSERT(len >= 0, ("%s: len %d < 0", __func__, len));		\
	while (off > 0) {						\
		KASSERT(iol >= 0, ("%s: empty in skip", __func__));	\
		if (off < iov->iov_len)					\
			break;						\
		off -= iov->iov_len;					\
		iol--;							\
		iov++;							\
	}								\
} while (0)

static void
cuio_copydata(struct uio* uio, int off, int len, caddr_t cp)
{
	struct iovec *iov = uio->uio_iov;
	int iol = uio->uio_iovcnt;
	unsigned count;

	CUIO_SKIP();
	while (len > 0) {
		KASSERT(iol >= 0, ("%s: empty", __func__));
		count = min(iov->iov_len - off, len);
		bcopy(((caddr_t)iov->iov_base) + off, cp, count);
		len -= count;
		cp += count;
		off = 0;
		iol--;
		iov++;
	}
}

static void
cuio_copyback(struct uio* uio, int off, int len, c_caddr_t cp)
{
	struct iovec *iov = uio->uio_iov;
	int iol = uio->uio_iovcnt;
	unsigned count;

	CUIO_SKIP();
	while (len > 0) {
		KASSERT(iol >= 0, ("%s: empty", __func__));
		count = min(iov->iov_len - off, len);
		bcopy(cp, ((caddr_t)iov->iov_base) + off, count);
		len -= count;
		cp += count;
		off = 0;
		iol--;
		iov++;
	}
}

/*
 * Return the index and offset of location in iovec list.
 */
static int
cuio_getptr(struct uio *uio, int loc, int *off)
{
	int ind, len;

	ind = 0;
	while (loc >= 0 && ind < uio->uio_iovcnt) {
		len = uio->uio_iov[ind].iov_len;
		if (len > loc) {
	    		*off = loc;
	    		return (ind);
		}
		loc -= len;
		ind++;
	}

	if (ind > 0 && loc == 0) {
		ind--;
		*off = uio->uio_iov[ind].iov_len;
		return (ind);
	}

	return (-1);
}

void
crypto_cursor_init(struct crypto_buffer_cursor *cc,
    const struct crypto_buffer *cb)
{
	memset(cc, 0, sizeof(*cc));
	cc->cc_type = cb->cb_type;
	switch (cc->cc_type) {
	case CRYPTO_BUF_CONTIG:
		cc->cc_buf = cb->cb_buf;
		cc->cc_buf_len = cb->cb_buf_len;
		break;
	case CRYPTO_BUF_MBUF:
		cc->cc_mbuf = cb->cb_mbuf;
		break;
	case CRYPTO_BUF_UIO:
		cc->cc_iov = cb->cb_uio->uio_iov;
		break;
	default:
#ifdef INVARIANTS
		panic("%s: invalid buffer type %d", __func__, cb->cb_type);
#endif
		break;
	}
}

void
crypto_cursor_advance(struct crypto_buffer_cursor *cc, size_t amount)
{
	size_t remain;

	switch (cc->cc_type) {
	case CRYPTO_BUF_CONTIG:
		MPASS(cc->cc_buf_len >= amount);
		cc->cc_buf += amount;
		cc->cc_buf_len -= amount;
		break;
	case CRYPTO_BUF_MBUF:
		for (;;) {
			remain = cc->cc_mbuf->m_len - cc->cc_offset;
			if (amount < remain) {
				cc->cc_offset += amount;
				break;
			}
			amount -= remain;
			cc->cc_mbuf = cc->cc_mbuf->m_next;
			cc->cc_offset = 0;
			if (amount == 0)
				break;
		}
		break;
	case CRYPTO_BUF_UIO:
		for (;;) {
			remain = cc->cc_iov->iov_len - cc->cc_offset;
			if (amount < remain) {
				cc->cc_offset += amount;
				break;
			}
			amount -= remain;
			cc->cc_iov++;
			cc->cc_offset = 0;
			if (amount == 0)
				break;
		}
		break;
	default:
#ifdef INVARIANTS
		panic("%s: invalid buffer type %d", __func__, cc->cc_type);
#endif
		break;
	}
}

void *
crypto_cursor_segbase(struct crypto_buffer_cursor *cc)
{
	switch (cc->cc_type) {
	case CRYPTO_BUF_CONTIG:
		return (cc->cc_buf);
	case CRYPTO_BUF_MBUF:
		if (cc->cc_mbuf == NULL)
			return (NULL);
		KASSERT((cc->cc_mbuf->m_flags & M_EXTPG) == 0,
		    ("%s: not supported for unmapped mbufs", __func__));
		return (mtod(cc->cc_mbuf, char *) + cc->cc_offset);
	case CRYPTO_BUF_UIO:
		return ((char *)cc->cc_iov->iov_base + cc->cc_offset);
	default:
#ifdef INVARIANTS
		panic("%s: invalid buffer type %d", __func__, cc->cc_type);
#endif
		return (NULL);
	}
}

size_t
crypto_cursor_seglen(struct crypto_buffer_cursor *cc)
{
	switch (cc->cc_type) {
	case CRYPTO_BUF_CONTIG:
		return (cc->cc_buf_len);
	case CRYPTO_BUF_MBUF:
		if (cc->cc_mbuf == NULL)
			return (0);
		return (cc->cc_mbuf->m_len - cc->cc_offset);
	case CRYPTO_BUF_UIO:
		return (cc->cc_iov->iov_len - cc->cc_offset);
	default:
#ifdef INVARIANTS
		panic("%s: invalid buffer type %d", __func__, cc->cc_type);
#endif
		return (0);
	}
}

void
crypto_cursor_copyback(struct crypto_buffer_cursor *cc, int size,
    const void *vsrc)
{
	size_t remain, todo;
	const char *src;
	char *dst;

	src = vsrc;
	switch (cc->cc_type) {
	case CRYPTO_BUF_CONTIG:
		MPASS(cc->cc_buf_len >= size);
		memcpy(cc->cc_buf, src, size);
		cc->cc_buf += size;
		cc->cc_buf_len -= size;
		break;
	case CRYPTO_BUF_MBUF:
		for (;;) {
			KASSERT((cc->cc_mbuf->m_flags & M_EXTPG) == 0,
			    ("%s: not supported for unmapped mbufs", __func__));
			dst = mtod(cc->cc_mbuf, char *) + cc->cc_offset;
			remain = cc->cc_mbuf->m_len - cc->cc_offset;
			todo = MIN(remain, size);
			memcpy(dst, src, todo);
			src += todo;
			if (todo < remain) {
				cc->cc_offset += todo;
				break;
			}
			size -= todo;	
			cc->cc_mbuf = cc->cc_mbuf->m_next;
			cc->cc_offset = 0;
			if (size == 0)
				break;
		}
		break;
	case CRYPTO_BUF_UIO:
		for (;;) {
			dst = (char *)cc->cc_iov->iov_base + cc->cc_offset;
			remain = cc->cc_iov->iov_len - cc->cc_offset;
			todo = MIN(remain, size);
			memcpy(dst, src, todo);
			src += todo;
			if (todo < remain) {
				cc->cc_offset += todo;
				break;
			}
			size -= todo;	
			cc->cc_iov++;
			cc->cc_offset = 0;
			if (size == 0)
				break;
		}
		break;
	default:
#ifdef INVARIANTS
		panic("%s: invalid buffer type %d", __func__, cc->cc_type);
#endif
		break;
	}
}

void
crypto_cursor_copydata(struct crypto_buffer_cursor *cc, int size, void *vdst)
{
	size_t remain, todo;
	const char *src;
	char *dst;

	dst = vdst;
	switch (cc->cc_type) {
	case CRYPTO_BUF_CONTIG:
		MPASS(cc->cc_buf_len >= size);
		memcpy(dst, cc->cc_buf, size);
		cc->cc_buf += size;
		cc->cc_buf_len -= size;
		break;
	case CRYPTO_BUF_MBUF:
		for (;;) {
			KASSERT((cc->cc_mbuf->m_flags & M_EXTPG) == 0,
			    ("%s: not supported for unmapped mbufs", __func__));
			src = mtod(cc->cc_mbuf, const char *) + cc->cc_offset;
			remain = cc->cc_mbuf->m_len - cc->cc_offset;
			todo = MIN(remain, size);
			memcpy(dst, src, todo);
			dst += todo;
			if (todo < remain) {
				cc->cc_offset += todo;
				break;
			}
			size -= todo;
			cc->cc_mbuf = cc->cc_mbuf->m_next;
			cc->cc_offset = 0;
			if (size == 0)
				break;
		}
		break;
	case CRYPTO_BUF_UIO:
		for (;;) {
			src = (const char *)cc->cc_iov->iov_base +
			    cc->cc_offset;
			remain = cc->cc_iov->iov_len - cc->cc_offset;
			todo = MIN(remain, size);
			memcpy(dst, src, todo);
			dst += todo;
			if (todo < remain) {
				cc->cc_offset += todo;
				break;
			}
			size -= todo;
			cc->cc_iov++;
			cc->cc_offset = 0;
			if (size == 0)
				break;
		}
		break;
	default:
#ifdef INVARIANTS
		panic("%s: invalid buffer type %d", __func__, cc->cc_type);
#endif
		break;
	}
}

/*
 * To avoid advancing 'cursor', make a local copy that gets advanced
 * instead.
 */
void
crypto_cursor_copydata_noadv(struct crypto_buffer_cursor *cc, int size,
    void *vdst)
{
	struct crypto_buffer_cursor copy;

	copy = *cc;
	crypto_cursor_copydata(&copy, size, vdst);
}

/*
 * Apply function f to the data in an iovec list starting "off" bytes from
 * the beginning, continuing for "len" bytes.
 */
static int
cuio_apply(struct uio *uio, int off, int len,
    int (*f)(void *, const void *, u_int), void *arg)
{
	struct iovec *iov = uio->uio_iov;
	int iol = uio->uio_iovcnt;
	unsigned count;
	int rval;

	CUIO_SKIP();
	while (len > 0) {
		KASSERT(iol >= 0, ("%s: empty", __func__));
		count = min(iov->iov_len - off, len);
		rval = (*f)(arg, ((caddr_t)iov->iov_base) + off, count);
		if (rval)
			return (rval);
		len -= count;
		off = 0;
		iol--;
		iov++;
	}
	return (0);
}

void
crypto_copyback(struct cryptop *crp, int off, int size, const void *src)
{
	struct crypto_buffer *cb;

	if (crp->crp_obuf.cb_type != CRYPTO_BUF_NONE)
		cb = &crp->crp_obuf;
	else
		cb = &crp->crp_buf;
	switch (cb->cb_type) {
	case CRYPTO_BUF_MBUF:
		m_copyback(cb->cb_mbuf, off, size, src);
		break;
	case CRYPTO_BUF_UIO:
		cuio_copyback(cb->cb_uio, off, size, src);
		break;
	case CRYPTO_BUF_CONTIG:
		MPASS(off + size <= cb->cb_buf_len);
		bcopy(src, cb->cb_buf + off, size);
		break;
	default:
#ifdef INVARIANTS
		panic("invalid crp buf type %d", cb->cb_type);
#endif
		break;
	}
}

void
crypto_copydata(struct cryptop *crp, int off, int size, void *dst)
{

	switch (crp->crp_buf.cb_type) {
	case CRYPTO_BUF_MBUF:
		m_copydata(crp->crp_buf.cb_mbuf, off, size, dst);
		break;
	case CRYPTO_BUF_UIO:
		cuio_copydata(crp->crp_buf.cb_uio, off, size, dst);
		break;
	case CRYPTO_BUF_CONTIG:
		MPASS(off + size <= crp->crp_buf.cb_buf_len);
		bcopy(crp->crp_buf.cb_buf + off, dst, size);
		break;
	default:
#ifdef INVARIANTS
		panic("invalid crp buf type %d", crp->crp_buf.cb_type);
#endif
		break;
	}
}

int
crypto_apply_buf(struct crypto_buffer *cb, int off, int len,
    int (*f)(void *, const void *, u_int), void *arg)
{
	int error;

	switch (cb->cb_type) {
	case CRYPTO_BUF_MBUF:
		error = m_apply(cb->cb_mbuf, off, len,
		    (int (*)(void *, void *, u_int))f, arg);
		break;
	case CRYPTO_BUF_UIO:
		error = cuio_apply(cb->cb_uio, off, len, f, arg);
		break;
	case CRYPTO_BUF_CONTIG:
		MPASS(off + len <= cb->cb_buf_len);
		error = (*f)(arg, cb->cb_buf + off, len);
		break;
	default:
#ifdef INVARIANTS
		panic("invalid crypto buf type %d", cb->cb_type);
#endif
		error = 0;
		break;
	}
	return (error);
}

int
crypto_apply(struct cryptop *crp, int off, int len,
    int (*f)(void *, const void *, u_int), void *arg)
{
	return (crypto_apply_buf(&crp->crp_buf, off, len, f, arg));
}

static inline void *
m_contiguous_subsegment(struct mbuf *m, size_t skip, size_t len)
{
	int rel_off;

	MPASS(skip <= INT_MAX);

	m = m_getptr(m, (int)skip, &rel_off);
	if (m == NULL)
		return (NULL);

	MPASS(rel_off >= 0);
	skip = rel_off;
	if (skip + len > m->m_len)
		return (NULL);

	return (mtod(m, char*) + skip);
}

static inline void *
cuio_contiguous_segment(struct uio *uio, size_t skip, size_t len)
{
	int rel_off, idx;

	MPASS(skip <= INT_MAX);
	idx = cuio_getptr(uio, (int)skip, &rel_off);
	if (idx < 0)
		return (NULL);

	MPASS(rel_off >= 0);
	skip = rel_off;
	if (skip + len > uio->uio_iov[idx].iov_len)
		return (NULL);
	return ((char *)uio->uio_iov[idx].iov_base + skip);
}

void *
crypto_buffer_contiguous_subsegment(struct crypto_buffer *cb, size_t skip,
    size_t len)
{

	switch (cb->cb_type) {
	case CRYPTO_BUF_MBUF:
		return (m_contiguous_subsegment(cb->cb_mbuf, skip, len));
	case CRYPTO_BUF_UIO:
		return (cuio_contiguous_segment(cb->cb_uio, skip, len));
	case CRYPTO_BUF_CONTIG:
		MPASS(skip + len <= cb->cb_buf_len);
		return (cb->cb_buf + skip);
	default:
#ifdef INVARIANTS
		panic("invalid crp buf type %d", cb->cb_type);
#endif
		return (NULL);
	}
}

void *
crypto_contiguous_subsegment(struct cryptop *crp, size_t skip, size_t len)
{
	return (crypto_buffer_contiguous_subsegment(&crp->crp_buf, skip, len));
}
