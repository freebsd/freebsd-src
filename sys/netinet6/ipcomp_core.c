/*	$FreeBSD$	*/
/*	$KAME: ipcomp_core.c,v 1.25 2001/07/26 06:53:17 jinmei Exp $	*/

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
 * RFC2393 IP payload compression protocol (IPComp).
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/zlib.h>
#include <machine/cpu.h>

#include <netinet6/ipcomp.h>
#ifdef INET6
#include <netinet6/ipcomp6.h>
#endif
#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif

#include <machine/stdarg.h>

#include <net/net_osdep.h>

static void *deflate_alloc __P((void *, u_int, u_int));
static void deflate_free __P((void *, void *));
static int deflate_common __P((struct mbuf *, struct mbuf *, size_t *, int));
static int deflate_compress __P((struct mbuf *, struct mbuf *, size_t *));
static int deflate_decompress __P((struct mbuf *, struct mbuf *, size_t *));

/*
 * We need to use default window size (2^15 = 32Kbytes as of writing) for
 * inbound case.  Otherwise we get interop problem.
 * Use negative value to avoid Adler32 checksum.  This is an undocumented
 * feature in zlib (see ipsec wg mailing list archive in January 2000).
 */
static int deflate_policy = Z_DEFAULT_COMPRESSION;
static int deflate_window_out = -12;
static const int deflate_window_in = -1 * MAX_WBITS;	/* don't change it */
static int deflate_memlevel = MAX_MEM_LEVEL;

static const struct ipcomp_algorithm ipcomp_algorithms[] = {
	{ deflate_compress, deflate_decompress, 90 },
};

const struct ipcomp_algorithm *
ipcomp_algorithm_lookup(idx)
	int idx;
{

	if (idx == SADB_X_CALG_DEFLATE)
		return &ipcomp_algorithms[0];
	return NULL;
}

static void *
deflate_alloc(aux, items, siz)
	void *aux;
	u_int items;
	u_int siz;
{
	void *ptr;
	ptr = malloc(items * siz, M_TEMP, M_NOWAIT);
	return ptr;
}

static void
deflate_free(aux, ptr)
	void *aux;
	void *ptr;
{
	free(ptr, M_TEMP);
}

static int
deflate_common(m, md, lenp, mode)
	struct mbuf *m;
	struct mbuf *md;
	size_t *lenp;
	int mode;	/* 0: compress 1: decompress */
{
	struct mbuf *mprev;
	struct mbuf *p;
	struct mbuf *n = NULL, *n0 = NULL, **np;
	z_stream zs;
	int error = 0;
	int zerror;
	size_t offset;

#define MOREBLOCK() \
do { \
	/* keep the reply buffer into our chain */		\
	if (n) {						\
		n->m_len = zs.total_out - offset;		\
		offset = zs.total_out;				\
		*np = n;					\
		np = &n->m_next;				\
		n = NULL;					\
	}							\
								\
	/* get a fresh reply buffer */				\
	MGET(n, M_DONTWAIT, MT_DATA);				\
	if (n) {						\
		MCLGET(n, M_DONTWAIT);				\
	}							\
	if (!n) {						\
		error = ENOBUFS;				\
		goto fail;					\
	}							\
	n->m_len = 0;						\
	n->m_len = M_TRAILINGSPACE(n);				\
	n->m_next = NULL;					\
	/* 							\
	 * if this is the first reply buffer, reserve		\
	 * region for ipcomp header.				\
	 */							\
	if (*np == NULL) {					\
		n->m_len -= sizeof(struct ipcomp);		\
		n->m_data += sizeof(struct ipcomp);		\
	}							\
								\
	zs.next_out = mtod(n, u_int8_t *);			\
	zs.avail_out = n->m_len;				\
} while (0)

	for (mprev = m; mprev && mprev->m_next != md; mprev = mprev->m_next)
		;
	if (!mprev)
		panic("md is not in m in deflate_common");

	bzero(&zs, sizeof(zs));
	zs.zalloc = deflate_alloc;
	zs.zfree = deflate_free;

	zerror = mode ? inflateInit2(&zs, deflate_window_in)
		      : deflateInit2(&zs, deflate_policy, Z_DEFLATED,
				deflate_window_out, deflate_memlevel,
				Z_DEFAULT_STRATEGY);
	if (zerror != Z_OK) {
		error = ENOBUFS;
		goto fail;
	}

	n0 = n = NULL;
	np = &n0;
	offset = 0;
	zerror = 0;
	p = md;
	while (p && p->m_len == 0) {
		p = p->m_next;
	}

	/* input stream and output stream are available */
	while (p && zs.avail_in == 0) {
		/* get input buffer */
		if (p && zs.avail_in == 0) {
			zs.next_in = mtod(p, u_int8_t *);
			zs.avail_in = p->m_len;
			p = p->m_next;
			while (p && p->m_len == 0) {
				p = p->m_next;
			}
		}

		/* get output buffer */
		if (zs.next_out == NULL || zs.avail_out == 0) {
			MOREBLOCK();
		}

		zerror = mode ? inflate(&zs, Z_NO_FLUSH)
			      : deflate(&zs, Z_NO_FLUSH);

		if (zerror == Z_STREAM_END)
			; /* once more. */
		else if (zerror == Z_OK) {
			/* inflate: Z_OK can indicate the end of decode */
			if (mode && !p && zs.avail_out != 0)
				goto terminate;
			else
				; /* once more. */
		} else {
			if (zs.msg) {
				ipseclog((LOG_ERR, "ipcomp_%scompress: "
				    "%sflate(Z_NO_FLUSH): %s\n",
				    mode ? "de" : "", mode ? "in" : "de",
				    zs.msg));
			} else {
				ipseclog((LOG_ERR, "ipcomp_%scompress: "
				    "%sflate(Z_NO_FLUSH): unknown error (%d)\n",
				    mode ? "de" : "", mode ? "in" : "de",
				    zerror));
			}
			mode ? inflateEnd(&zs) : deflateEnd(&zs);
			error = EINVAL;
			goto fail;
		}
	}

	if (zerror == Z_STREAM_END)
		goto terminate;

	/* termination */
	while (1) {
		/* get output buffer */
		if (zs.next_out == NULL || zs.avail_out == 0) {
			MOREBLOCK();
		}

		zerror = mode ? inflate(&zs, Z_SYNC_FLUSH)
			      : deflate(&zs, Z_FINISH);

		if (zerror == Z_STREAM_END)
			break;
		else if (zerror == Z_OK) {
			if (mode && zs.avail_out != 0)
				goto terminate;
			else
				; /* once more. */
		} else {
			if (zs.msg) {
				ipseclog((LOG_ERR, "ipcomp_%scompress: "
				    "%sflate(Z_FINISH): %s\n",
				    mode ? "de" : "", mode ? "in" : "de",
				    zs.msg));
			} else {
				ipseclog((LOG_ERR, "ipcomp_%scompress: "
				    "%sflate(Z_FINISH): unknown error (%d)\n",
				    mode ? "de" : "", mode ? "in" : "de",
				    zerror));
			}
			mode ? inflateEnd(&zs) : deflateEnd(&zs);
			error = EINVAL;
			goto fail;
		}
	}

terminate:
	zerror = mode ? inflateEnd(&zs) : deflateEnd(&zs);
	if (zerror != Z_OK) {
		if (zs.msg) {
			ipseclog((LOG_ERR, "ipcomp_%scompress: "
			    "%sflateEnd: %s\n",
			    mode ? "de" : "", mode ? "in" : "de",
			    zs.msg));
		} else {
			ipseclog((LOG_ERR, "ipcomp_%scompress: "
			    "%sflateEnd: unknown error (%d)\n",
			    mode ? "de" : "", mode ? "in" : "de",
			    zerror));
		}
		error = EINVAL;
		goto fail;
	}
	/* keep the final reply buffer into our chain */
	if (n) {
		n->m_len = zs.total_out - offset;
		offset = zs.total_out;
		*np = n;
		np = &n->m_next;
		n = NULL;
	}

	/* switch the mbuf to the new one */
	mprev->m_next = n0;
	m_freem(md);
	*lenp = zs.total_out;

	return 0;

fail:
	if (m)
		m_freem(m);
	if (n)
		m_freem(n);
	if (n0)
		m_freem(n0);
	return error;
#undef MOREBLOCK
}

static int
deflate_compress(m, md, lenp)
	struct mbuf *m;
	struct mbuf *md;
	size_t *lenp;
{
	if (!m)
		panic("m == NULL in deflate_compress");
	if (!md)
		panic("md == NULL in deflate_compress");
	if (!lenp)
		panic("lenp == NULL in deflate_compress");

	return deflate_common(m, md, lenp, 0);
}

static int
deflate_decompress(m, md, lenp)
	struct mbuf *m;
	struct mbuf *md;
	size_t *lenp;
{
	if (!m)
		panic("m == NULL in deflate_decompress");
	if (!md)
		panic("md == NULL in deflate_decompress");
	if (!lenp)
		panic("lenp == NULL in deflate_decompress");

	return deflate_common(m, md, lenp, 1);
}
