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


#ifndef _NFS_NFSM_SUBS_H_
#define _NFS_NFSM_SUBS_H_

struct ucred;
struct vnode;

/*
 * These macros do strange and peculiar things to mbuf chains for
 * the assistance of the nfs code. To attempt to use them for any
 * other purpose will be dangerous. (they make weird assumptions)
 */

/*
 * First define what the actual subs. return
 */
struct mbuf *nfsm_reqh __P((struct vnode *vp, u_long procid, int hsiz,
			    caddr_t *bposp));
struct mbuf *nfsm_rpchead __P((struct ucred *cr, int nmflag, int procid,
			       int auth_type, int auth_len, char *auth_str,
			       int verf_len, char *verf_str,
			       struct mbuf *mrest, int mrest_len,
			       struct mbuf **mbp, u_int32_t *xidp));

#define	M_HASCL(m)	((m)->m_flags & M_EXT)
#define	NFSMINOFF(m) \
	do { \
		if (M_HASCL(m)) \
			(m)->m_data = (m)->m_ext.ext_buf; \
		else if ((m)->m_flags & M_PKTHDR) \
			(m)->m_data = (m)->m_pktdat; \
		else \
			(m)->m_data = (m)->m_dat; \
	} while (0)
#define	NFSMADV(m, s) \
	do { \
		(m)->m_data += (s); \
	} while (0)
#define	NFSMSIZ(m)	((M_HASCL(m))?MCLBYTES: \
				(((m)->m_flags & M_PKTHDR)?MHLEN:MLEN))

/*
 * Now for the macros that do the simple stuff and call the functions
 * for the hard stuff.
 * These macros use several vars. declared in nfsm_reqhead and these
 * vars. must not be used elsewhere unless you are careful not to corrupt
 * them. The vars. starting with pN and tN (N=1,2,3,..) are temporaries
 * that may be used so long as the value is not expected to retained
 * after a macro.
 * I know, this is kind of dorkey, but it makes the actual op functions
 * fairly clean and deals with the mess caused by the xdr discriminating
 * unions.
 */

#define	nfsm_build(a,c,s) \
	do { \
		if ((s) > M_TRAILINGSPACE(mb)) { \
			MGET(mb2, M_WAIT, MT_DATA); \
			if ((s) > MLEN) \
				panic("build > MLEN"); \
			mb->m_next = mb2; \
			mb = mb2; \
			mb->m_len = 0; \
			bpos = mtod(mb, caddr_t); \
		} \
		(a) = (c)(bpos); \
		mb->m_len += (s); \
		bpos += (s); \
	} while (0)

#define	nfsm_dissect(a, c, s) \
	do { \
		t1 = mtod(md, caddr_t)+md->m_len-dpos; \
		if (t1 >= (s)) { \
			(a) = (c)(dpos); \
			dpos += (s); \
		} else if ((t1 = nfsm_disct(&md, &dpos, (s), t1, &cp2)) != 0){ \
			error = t1; \
			m_freem(mrep); \
			goto nfsmout; \
		} else { \
			(a) = (c)cp2; \
		} \
	} while (0)

#define nfsm_fhtom(v, v3) \
	do { \
		if (v3) { \
			t2 = nfsm_rndup(VTONFS(v)->n_fhsize) + NFSX_UNSIGNED; \
			if (t2 <= M_TRAILINGSPACE(mb)) { \
				nfsm_build(tl, u_int32_t *, t2); \
				*tl++ = txdr_unsigned(VTONFS(v)->n_fhsize); \
				*(tl + ((t2>>2) - 2)) = 0; \
				bcopy((caddr_t)VTONFS(v)->n_fhp,(caddr_t)tl, \
					VTONFS(v)->n_fhsize); \
			} else if ((t2 = nfsm_strtmbuf(&mb, &bpos, \
				(caddr_t)VTONFS(v)->n_fhp, \
				VTONFS(v)->n_fhsize)) != 0) { \
				error = t2; \
				m_freem(mreq); \
				goto nfsmout; \
			} \
		} else { \
			nfsm_build(cp, caddr_t, NFSX_V2FH); \
			bcopy((caddr_t)VTONFS(v)->n_fhp, cp, NFSX_V2FH); \
		} \
	} while (0)

#define nfsm_srvfhtom(f, v3) \
	do { \
		if (v3) { \
			nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED + NFSX_V3FH);\
			*tl++ = txdr_unsigned(NFSX_V3FH); \
			bcopy((caddr_t)(f), (caddr_t)tl, NFSX_V3FH); \
		} else { \
			nfsm_build(cp, caddr_t, NFSX_V2FH); \
			bcopy((caddr_t)(f), cp, NFSX_V2FH); \
		} \
	} while (0)

#define nfsm_srvpostop_fh(f) \
	do { \
		nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED + NFSX_V3FH); \
		*tl++ = nfs_true; \
		*tl++ = txdr_unsigned(NFSX_V3FH); \
		bcopy((caddr_t)(f), (caddr_t)tl, NFSX_V3FH); \
	} while (0)

#define nfsm_mtofh(d, v, v3, f) \
	do { \
		struct nfsnode *ttnp; nfsfh_t *ttfhp; int ttfhsize; \
		if (v3) { \
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
			(f) = fxdr_unsigned(int, *tl); \
		} else \
			(f) = 1; \
		if (f) { \
			nfsm_getfh(ttfhp, ttfhsize, (v3)); \
			if ((t1 = nfs_nget((d)->v_mount, ttfhp, ttfhsize, \
				&ttnp)) != 0) { \
				error = t1; \
				m_freem(mrep); \
				goto nfsmout; \
			} \
			(v) = NFSTOV(ttnp); \
		} \
		if (v3) { \
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
			if (f) \
				(f) = fxdr_unsigned(int, *tl); \
			else if (fxdr_unsigned(int, *tl)) \
				nfsm_adv(NFSX_V3FATTR); \
		} \
		if (f) \
			nfsm_loadattr((v), (struct vattr *)0); \
	} while (0)

#define nfsm_getfh(f, s, v3) \
	do { \
		if (v3) { \
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
			if (((s) = fxdr_unsigned(int, *tl)) <= 0 || \
				(s) > NFSX_V3FHMAX) { \
				m_freem(mrep); \
				error = EBADRPC; \
				goto nfsmout; \
			} \
		} else \
			(s) = NFSX_V2FH; \
		nfsm_dissect((f), nfsfh_t *, nfsm_rndup(s)); \
	} while (0)

#define	nfsm_loadattr(v, a) \
	do { \
		struct vnode *ttvp = (v); \
		if ((t1 = nfs_loadattrcache(&ttvp, &md, &dpos, (a), 0)) != 0) { \
			error = t1; \
			m_freem(mrep); \
			goto nfsmout; \
		} \
		(v) = ttvp; \
	} while (0)

#define	nfsm_postop_attr(v, f) \
	do { \
		struct vnode *ttvp = (v); \
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
		if (((f) = fxdr_unsigned(int, *tl)) != 0) { \
			if ((t1 = nfs_loadattrcache(&ttvp, &md, &dpos, \
				(struct vattr *)0, 1)) != 0) { \
				error = t1; \
				(f) = 0; \
				m_freem(mrep); \
				goto nfsmout; \
			} \
			(v) = ttvp; \
		} \
	} while (0)

/* Used as (f) for nfsm_wcc_data() */
#define NFSV3_WCCRATTR	0
#define NFSV3_WCCCHK	1

#define	nfsm_wcc_data(v, f) \
	do { \
		int ttattrf, ttretf = 0; \
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
		if (*tl == nfs_true) { \
			nfsm_dissect(tl, u_int32_t *, 6 * NFSX_UNSIGNED); \
			if (f) \
				ttretf = (VTONFS(v)->n_mtime == \
					fxdr_unsigned(u_int32_t, *(tl + 2))); \
		} \
		nfsm_postop_attr((v), ttattrf); \
		if (f) { \
			(f) = ttretf; \
		} else { \
			(f) = ttattrf; \
		} \
	} while (0)

/* If full is true, set all fields, otherwise just set mode and time fields */
#define nfsm_v3attrbuild(a, full) \
	do { \
		if ((a)->va_mode != (mode_t)VNOVAL) {			\
			nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);	\
			*tl++ = nfs_true;				\
			*tl = txdr_unsigned((a)->va_mode);		\
		} else {						\
			nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);	\
			*tl = nfs_false;				\
		}							\
		if ((full) && (a)->va_uid != (uid_t)VNOVAL) {		\
			nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);	\
			*tl++ = nfs_true;				\
			*tl = txdr_unsigned((a)->va_uid);		\
		} else {						\
			nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);	\
			*tl = nfs_false;				\
		}							\
		if ((full) && (a)->va_gid != (gid_t)VNOVAL) {		\
			nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);	\
			*tl++ = nfs_true;				\
			*tl = txdr_unsigned((a)->va_gid);		\
		} else {						\
			nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);	\
			*tl = nfs_false;				\
		}							\
		if ((full) && (a)->va_size != VNOVAL) {			\
			nfsm_build(tl, u_int32_t *, 3 * NFSX_UNSIGNED);	\
			*tl++ = nfs_true;				\
			txdr_hyper((a)->va_size, tl);			\
		} else {						\
			nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);	\
			*tl = nfs_false;				\
		}							\
		if ((a)->va_atime.tv_sec != VNOVAL) {			\
			if ((a)->va_atime.tv_sec != time_second) {	\
				nfsm_build(tl, u_int32_t *, 3 * NFSX_UNSIGNED);\
				*tl++ = txdr_unsigned(NFSV3SATTRTIME_TOCLIENT);\
				txdr_nfsv3time(&(a)->va_atime, tl);	\
			} else {					\
				nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED); \
				*tl = txdr_unsigned(NFSV3SATTRTIME_TOSERVER); \
			}						\
		} else {						\
			nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);	\
			*tl = txdr_unsigned(NFSV3SATTRTIME_DONTCHANGE);	\
		}							\
		if ((a)->va_mtime.tv_sec != VNOVAL) {			\
			if ((a)->va_mtime.tv_sec != time_second) {	\
				nfsm_build(tl, u_int32_t *, 3 * NFSX_UNSIGNED);\
				*tl++ = txdr_unsigned(NFSV3SATTRTIME_TOCLIENT);\
				txdr_nfsv3time(&(a)->va_mtime, tl);	\
			} else {					\
				nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED); \
				*tl = txdr_unsigned(NFSV3SATTRTIME_TOSERVER); \
			}						\
		} else {						\
			nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);	\
			*tl = txdr_unsigned(NFSV3SATTRTIME_DONTCHANGE);	\
		}							\
	} while (0)
				

#define	nfsm_strsiz(s,m) \
	do { \
		nfsm_dissect(tl,u_int32_t *,NFSX_UNSIGNED); \
		if (((s) = fxdr_unsigned(int32_t,*tl)) > (m)) { \
			m_freem(mrep); \
			error = EBADRPC; \
			goto nfsmout; \
		} \
	} while (0)

#define	nfsm_srvstrsiz(s,m) \
	do { \
		nfsm_dissect(tl,u_int32_t *,NFSX_UNSIGNED); \
		if (((s) = fxdr_unsigned(int32_t,*tl)) > (m) || (s) <= 0) { \
			error = EBADRPC; \
			nfsm_reply(0); \
		} \
	} while (0)

#define	nfsm_srvnamesiz(s) \
	do { \
		nfsm_dissect(tl,u_int32_t *,NFSX_UNSIGNED); \
		if (((s) = fxdr_unsigned(int32_t,*tl)) > NFS_MAXNAMLEN) \
			error = NFSERR_NAMETOL; \
		if ((s) <= 0) \
			error = EBADRPC; \
		if (error) \
			nfsm_reply(0); \
	} while (0)

#define nfsm_mtouio(p,s) \
	do {\
		if ((s) > 0 && \
		   (t1 = nfsm_mbuftouio(&md,(p),(s),&dpos)) != 0) { \
			error = t1; \
			m_freem(mrep); \
			goto nfsmout; \
		} \
	} while (0)

#define nfsm_uiotom(p,s) \
	do { \
		if ((t1 = nfsm_uiotombuf((p),&mb,(s),&bpos)) != 0) { \
			error = t1; \
			m_freem(mreq); \
			goto nfsmout; \
		} \
	} while (0)

#define	nfsm_reqhead(v,a,s) \
	do { \
		mb = mreq = nfsm_reqh((v),(a),(s),&bpos); \
	} while (0)

#define nfsm_reqdone \
	do { \
		m_freem(mrep); \
		nfsmout: \
	} while (0)

#define nfsm_rndup(a)	(((a)+3)&(~0x3))

#define	nfsm_request(v, t, p, c) \
	do { \
		if ((error = nfs_request((v), mreq, (t), (p), \
		   (c), &mrep, &md, &dpos)) != 0) { \
			if (error & NFSERR_RETERR) \
				error &= ~NFSERR_RETERR; \
			else \
				goto nfsmout; \
		} \
	} while (0)

#define	nfsm_strtom(a,s,m) \
	do {\
		if ((s) > (m)) { \
			m_freem(mreq); \
			error = ENAMETOOLONG; \
			goto nfsmout; \
		} \
		t2 = nfsm_rndup(s)+NFSX_UNSIGNED; \
		if (t2 <= M_TRAILINGSPACE(mb)) { \
			nfsm_build(tl,u_int32_t *,t2); \
			*tl++ = txdr_unsigned(s); \
			*(tl+((t2>>2)-2)) = 0; \
			bcopy((const char *)(a), (caddr_t)tl, (s)); \
		} else if ((t2 = nfsm_strtmbuf(&mb, &bpos, (a), (s))) != 0) { \
			error = t2; \
			m_freem(mreq); \
			goto nfsmout; \
		} \
	} while (0)

#define	nfsm_srvdone \
	do { \
		nfsmout: \
		return (error); \
	} while (0)

#define	nfsm_reply(s) \
	do { \
		nfsd->nd_repstat = error; \
		if (error && !(nfsd->nd_flag & ND_NFSV3)) \
		   (void) nfs_rephead(0, nfsd, slp, error, cache, &frev, \
			mrq, &mb, &bpos); \
		else \
		   (void) nfs_rephead((s), nfsd, slp, error, cache, &frev, \
			mrq, &mb, &bpos); \
		if (mrep != NULL) { \
			m_freem(mrep); \
			mrep = NULL; \
		} \
		mreq = *mrq; \
		if (error && (!(nfsd->nd_flag & ND_NFSV3) || \
			error == EBADRPC)) { \
			error = 0; \
			goto nfsmout; \
		} \
	} while (0)

#define	nfsm_writereply(s, v3) \
	do { \
		nfsd->nd_repstat = error; \
		if (error && !(v3)) \
		   (void) nfs_rephead(0, nfsd, slp, error, cache, &frev, \
			&mreq, &mb, &bpos); \
		else \
		   (void) nfs_rephead((s), nfsd, slp, error, cache, &frev, \
			&mreq, &mb, &bpos); \
	} while (0)

#define	nfsm_adv(s) \
	do { \
		t1 = mtod(md, caddr_t)+md->m_len-dpos; \
		if (t1 >= (s)) { \
			dpos += (s); \
		} else if ((t1 = nfs_adv(&md, &dpos, (s), t1)) != 0) { \
			error = t1; \
			m_freem(mrep); \
			goto nfsmout; \
		} \
	} while (0)

#define nfsm_srvmtofh(f) \
	do { \
		int fhlen; \
		if (nfsd->nd_flag & ND_NFSV3) { \
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
			fhlen = fxdr_unsigned(int, *tl); \
			if (fhlen != 0 && fhlen != NFSX_V3FH) { \
				error = EBADRPC; \
				nfsm_reply(0); \
			} \
		} else { \
			fhlen = NFSX_V2FH; \
		} \
		if (fhlen != 0) { \
			nfsm_dissect(tl, u_int32_t *, fhlen); \
			bcopy((caddr_t)tl, (caddr_t)(f), fhlen); \
		} else {\
			bzero((caddr_t)(f), NFSX_V3FH); \
		} \
	} while (0)

#define	nfsm_clget \
	do { \
		if (bp >= be) { \
			if (mp == mb) \
				mp->m_len += bp-bpos; \
			MGET(mp, M_WAIT, MT_DATA); \
			MCLGET(mp, M_WAIT); \
			mp->m_len = NFSMSIZ(mp); \
			mp2->m_next = mp; \
			mp2 = mp; \
			bp = mtod(mp, caddr_t); \
			be = bp+mp->m_len; \
		} \
		tl = (u_int32_t *)bp; \
	} while (0)

#define	nfsm_srvfillattr(a, f) \
	do { \
		nfsm_srvfattr(nfsd, (a), (f)); \
	} while (0)

#define nfsm_srvwcc_data(br, b, ar, a) \
	do { \
		nfsm_srvwcc(nfsd, (br), (b), (ar), (a), &mb, &bpos); \
	} while (0)

#define nfsm_srvpostop_attr(r, a) \
	do { \
		nfsm_srvpostopattr(nfsd, (r), (a), &mb, &bpos); \
	} while (0)

#define nfsm_srvsattr(a) \
	do { \
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
		if (*tl == nfs_true) { \
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
			(a)->va_mode = nfstov_mode(*tl); \
		} \
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
		if (*tl == nfs_true) { \
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
			(a)->va_uid = fxdr_unsigned(uid_t, *tl); \
		} \
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
		if (*tl == nfs_true) { \
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
			(a)->va_gid = fxdr_unsigned(gid_t, *tl); \
		} \
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
		if (*tl == nfs_true) { \
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED); \
			(a)->va_size = fxdr_hyper(tl); \
		} \
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
		switch (fxdr_unsigned(int, *tl)) { \
		case NFSV3SATTRTIME_TOCLIENT: \
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED); \
			fxdr_nfsv3time(tl, &(a)->va_atime); \
			break; \
		case NFSV3SATTRTIME_TOSERVER: \
			getnanotime(&(a)->va_atime); \
			break; \
		}; \
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED); \
		switch (fxdr_unsigned(int, *tl)) { \
		case NFSV3SATTRTIME_TOCLIENT: \
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED); \
			fxdr_nfsv3time(tl, &(a)->va_mtime); \
			break; \
		case NFSV3SATTRTIME_TOSERVER: \
			getnanotime(&(a)->va_mtime); \
			break; \
		} \
	} while (0)

#endif
