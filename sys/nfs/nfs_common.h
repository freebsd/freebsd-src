/*
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
 *	@(#)nfsm_subs.h	8.2 (Berkeley) 3/30/95
 * $FreeBSD$
 */


#ifndef _NFS_NFS_COMMON_H_
#define _NFS_NFS_COMMON_H_

extern enum vtype nv3tov_type[];
extern nfstype nfsv3_type[];

#define	vtonfsv2_mode(t, m) \
    txdr_unsigned(((t) == VFIFO) ? MAKEIMODE(VCHR, (m)) : MAKEIMODE((t), (m)))

#define	nfsv3tov_type(a)	nv3tov_type[fxdr_unsigned(u_int32_t,(a))&0x7]
#define	vtonfsv3_type(a)	txdr_unsigned(nfsv3_type[((int32_t)(a))])

#define NFSMADV(m, s) \
	do { \
		(m)->m_data += (s); \
	} while (0)

int	nfs_adv(struct mbuf **, caddr_t *, int, int);
void	*nfsm_build_xx(int s, struct mbuf **mb, caddr_t *bpos);
int	nfsm_dissect_xx(void **a, int s, struct mbuf **md, caddr_t *dpos);
int	nfsm_strsiz_xx(int *s, int m, u_int32_t **tl, struct mbuf **mb,
	    caddr_t *bpos);
int	nfsm_adv_xx(int s, u_int32_t **tl, struct mbuf **md, caddr_t *dpos);
u_quad_t nfs_curusec(void);
int	nfsm_disct(struct mbuf **, caddr_t *, int, int, caddr_t *);

#define	nfsm_build(c, s) \
	(c)nfsm_build_xx((s), &mb, &bpos); \

/* XXX 'c' arg (type) is not used */
#define	nfsm_dissect(a, c, s) \
do { \
	int t1; \
	t1 = nfsm_dissect_xx((void **)&(a), (s), &md, &dpos); \
	if (t1) { \
		error = t1; \
		m_freem(mrep); \
		mrep = NULL; \
		goto nfsmout; \
	} \
} while (0)

#define	nfsm_strsiz(s,m) \
do { \
	int t1; \
	t1 = nfsm_strsiz_xx(&(s), (m), &tl, &md, &dpos); \
	if (t1) { \
		error = t1; \
		m_freem(mrep); \
		mrep = NULL; \
		goto nfsmout; \
	} \
} while(0)

#define nfsm_mtouio(p,s) \
do {\
	int32_t t1; \
	if ((s) > 0 && (t1 = nfsm_mbuftouio(&md, (p), (s), &dpos)) != 0) { \
		error = t1; \
		m_freem(mrep); \
		mrep = NULL; \
		goto nfsmout; \
	} \
} while (0)

#define nfsm_rndup(a)	(((a)+3)&(~0x3))

#define	nfsm_adv(s) \
do { \
	int t1; \
	t1 = nfsm_adv_xx((s), &tl, &md, &dpos); \
	if (t1) { \
		error = t1; \
		m_freem(mrep); \
		mrep = NULL; \
		goto nfsmout; \
	} \
} while (0)

#endif
