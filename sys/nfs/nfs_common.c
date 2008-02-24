/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)nfs_subs.c  8.8 (Berkeley) 5/22/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/nfs/nfs_common.c,v 1.118 2005/07/14 20:08:26 ps Exp $");

/*
 * These functions support the macros and help fiddle mbuf chains for
 * the nfs op functions. They do things like create the rpc header and
 * copy data between mbuf chains and uio lists.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/sysent.h>
#include <sys/syscall.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsserver/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfs_common.h>

#include <netinet/in.h>

enum vtype nv3tov_type[8]= {
	VNON, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO
};
nfstype nfsv3_type[9] = {
	NFNON, NFREG, NFDIR, NFBLK, NFCHR, NFLNK, NFSOCK, NFFIFO, NFNON
};

static void *nfsm_dissect_xx_sub(int s, struct mbuf **md, caddr_t *dpos, int how);

u_quad_t
nfs_curusec(void) 
{
	struct timeval tv;

	getmicrotime(&tv);
	return ((u_quad_t)tv.tv_sec * 1000000 + (u_quad_t)tv.tv_usec);
}

/*
 * copies mbuf chain to the uio scatter/gather list
 */
int
nfsm_mbuftouio(struct mbuf **mrep, struct uio *uiop, int siz, caddr_t *dpos)
{
	char *mbufcp, *uiocp;
	int xfer, left, len;
	struct mbuf *mp;
	long uiosiz, rem;
	int error = 0;

	mp = *mrep;
	mbufcp = *dpos;
	len = mtod(mp, caddr_t)+mp->m_len-mbufcp;
	rem = nfsm_rndup(siz)-siz;
	while (siz > 0) {
		if (uiop->uio_iovcnt <= 0 || uiop->uio_iov == NULL)
			return (EFBIG);
		left = uiop->uio_iov->iov_len;
		uiocp = uiop->uio_iov->iov_base;
		if (left > siz)
			left = siz;
		uiosiz = left;
		while (left > 0) {
			while (len == 0) {
				mp = mp->m_next;
				if (mp == NULL)
					return (EBADRPC);
				mbufcp = mtod(mp, caddr_t);
				len = mp->m_len;
			}
			xfer = (left > len) ? len : left;
#ifdef notdef
			/* Not Yet.. */
			if (uiop->uio_iov->iov_op != NULL)
				(*(uiop->uio_iov->iov_op))
				(mbufcp, uiocp, xfer);
			else
#endif
			if (uiop->uio_segflg == UIO_SYSSPACE)
				bcopy(mbufcp, uiocp, xfer);
			else
				copyout(mbufcp, uiocp, xfer);
			left -= xfer;
			len -= xfer;
			mbufcp += xfer;
			uiocp += xfer;
			uiop->uio_offset += xfer;
			uiop->uio_resid -= xfer;
		}
		if (uiop->uio_iov->iov_len <= siz) {
			uiop->uio_iovcnt--;
			uiop->uio_iov++;
		} else {
			uiop->uio_iov->iov_base =
			    (char *)uiop->uio_iov->iov_base + uiosiz;
			uiop->uio_iov->iov_len -= uiosiz;
		}
		siz -= uiosiz;
	}
	*dpos = mbufcp;
	*mrep = mp;
	if (rem > 0) {
		if (len < rem)
			error = nfs_adv(mrep, dpos, rem, len);
		else
			*dpos += rem;
	}
	return (error);
}

/*
 * Help break down an mbuf chain by setting the first siz bytes contiguous
 * pointed to by returned val.
 * This is used by the macros nfsm_dissect for tough
 * cases. (The macros use the vars. dpos and dpos2)
 */
void *
nfsm_disct(struct mbuf **mdp, caddr_t *dposp, int siz, int left, int how)
{
	struct mbuf *mp, *mp2;
	int siz2, xfer;
	caddr_t ptr, npos = NULL;
	void *ret;

	mp = *mdp;
	while (left == 0) {
		*mdp = mp = mp->m_next;
		if (mp == NULL)
			return NULL;
		left = mp->m_len;
		*dposp = mtod(mp, caddr_t);
	}
	if (left >= siz) {
		ret = *dposp;
		*dposp += siz;
	} else if (mp->m_next == NULL) {
		return NULL;
	} else if (siz > MHLEN) {
		panic("nfs S too big");
	} else {
		MGET(mp2, how, MT_DATA);
		if (mp2 == NULL)
			return NULL;
		mp2->m_len = siz;
		mp2->m_next = mp->m_next;
		mp->m_next = mp2;
		mp->m_len -= left;
		mp = mp2;
		ptr = mtod(mp, caddr_t);
		ret = ptr;
		bcopy(*dposp, ptr, left);		/* Copy what was left */
		siz2 = siz-left;
		ptr += left;
		mp2 = mp->m_next;
		npos = mtod(mp2, caddr_t);
		/* Loop around copying up the siz2 bytes */
		while (siz2 > 0) {
			if (mp2 == NULL)
				return NULL;
			xfer = (siz2 > mp2->m_len) ? mp2->m_len : siz2;
			if (xfer > 0) {
				bcopy(mtod(mp2, caddr_t), ptr, xfer);
				mp2->m_data += xfer;
				mp2->m_len -= xfer;
				ptr += xfer;
				siz2 -= xfer;
			}
			if (siz2 > 0) {
				mp2 = mp2->m_next;
				if (mp2 != NULL)
					npos = mtod(mp2, caddr_t);
			}
		}
		*mdp = mp2;
		*dposp = mtod(mp2, caddr_t);
		if (!nfsm_aligned(*dposp, u_int32_t)) {
			bcopy(*dposp, npos, mp2->m_len);
			mp2->m_data = npos;
			*dposp = npos;
		}
	}
	return ret;
}

/*
 * Advance the position in the mbuf chain.
 */
int
nfs_adv(struct mbuf **mdp, caddr_t *dposp, int offs, int left)
{
	struct mbuf *m;
	int s;

	m = *mdp;
	s = left;
	while (s < offs) {
		offs -= s;
		m = m->m_next;
		if (m == NULL)
			return (EBADRPC);
		s = m->m_len;
	}
	*mdp = m;
	*dposp = mtod(m, caddr_t)+offs;
	return (0);
}

void *
nfsm_build_xx(int s, struct mbuf **mb, caddr_t *bpos)
{
	struct mbuf *mb2;
	void *ret;

	if (s > M_TRAILINGSPACE(*mb)) {
		MGET(mb2, M_TRYWAIT, MT_DATA);
		if (s > MLEN)
			panic("build > MLEN");
		(*mb)->m_next = mb2;
		*mb = mb2;
		(*mb)->m_len = 0;
		*bpos = mtod(*mb, caddr_t);
	}
	ret = *bpos;
	(*mb)->m_len += s;
	*bpos += s;
	return ret;
}

void *
nfsm_dissect_xx(int s, struct mbuf **md, caddr_t *dpos)
{
	return nfsm_dissect_xx_sub(s, md, dpos, M_TRYWAIT);
}

void *
nfsm_dissect_xx_nonblock(int s, struct mbuf **md, caddr_t *dpos)
{
	return nfsm_dissect_xx_sub(s, md, dpos, M_DONTWAIT);
}

static void *
nfsm_dissect_xx_sub(int s, struct mbuf **md, caddr_t *dpos, int how)
{
	int t1;
	char *cp2;
	void *ret;

	t1 = mtod(*md, caddr_t) + (*md)->m_len - *dpos;
	if (t1 >= s) {
		ret = *dpos;
		*dpos += s;
		return ret;
	}
	cp2 = nfsm_disct(md, dpos, s, t1, how); 
	return cp2;
}

int
nfsm_strsiz_xx(int *s, int m, struct mbuf **mb, caddr_t *bpos)
{
	u_int32_t *tl;

	tl = nfsm_dissect_xx(NFSX_UNSIGNED, mb, bpos);
	if (tl == NULL)
		return EBADRPC;
	*s = fxdr_unsigned(int32_t, *tl);
	if (*s > m)
		return EBADRPC;
	return 0;
}

int
nfsm_adv_xx(int s, struct mbuf **md, caddr_t *dpos)
{
	int t1;

	t1 = mtod(*md, caddr_t) + (*md)->m_len - *dpos;
	if (t1 >= s) {
		*dpos += s;
		return 0;
	}
	t1 = nfs_adv(md, dpos, s, t1);
	if (t1)
		return t1;
	return 0;
}
