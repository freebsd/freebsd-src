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
 *	@(#)nfsm_subs.h	8.1 (Berkeley) 6/16/93
 */

/*
 * These macros do strange and peculiar things to mbuf chains for
 * the assistance of the nfs code. To attempt to use them for any
 * other purpose will be dangerous. (they make weird assumptions)
 */

/*
 * First define what the actual subs. return
 */
extern struct mbuf *nfsm_reqh();

#define	M_HASCL(m)	((m)->m_flags & M_EXT)
#define	NFSMINOFF(m) \
		if (M_HASCL(m)) \
			(m)->m_data = (m)->m_ext.ext_buf; \
		else if ((m)->m_flags & M_PKTHDR) \
			(m)->m_data = (m)->m_pktdat; \
		else \
			(m)->m_data = (m)->m_dat
#define	NFSMADV(m, s)	(m)->m_data += (s)
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
		{ if ((s) > M_TRAILINGSPACE(mb)) { \
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
		bpos += (s); }

#define	nfsm_dissect(a,c,s) \
		{ t1 = mtod(md, caddr_t)+md->m_len-dpos; \
		if (t1 >= (s)) { \
			(a) = (c)(dpos); \
			dpos += (s); \
		} else if (error = nfsm_disct(&md, &dpos, (s), t1, &cp2)) { \
			m_freem(mrep); \
			goto nfsmout; \
		} else { \
			(a) = (c)cp2; \
		} }

#define nfsm_fhtom(v) \
		nfsm_build(cp,caddr_t,NFSX_FH); \
		bcopy((caddr_t)&(VTONFS(v)->n_fh), cp, NFSX_FH)

#define nfsm_srvfhtom(f) \
		nfsm_build(cp,caddr_t,NFSX_FH); \
		bcopy((caddr_t)(f), cp, NFSX_FH)

#define nfsm_mtofh(d,v) \
		{ struct nfsnode *np; nfsv2fh_t *fhp; \
		nfsm_dissect(fhp,nfsv2fh_t *,NFSX_FH); \
		if (error = nfs_nget((d)->v_mount, fhp, &np)) { \
			m_freem(mrep); \
			goto nfsmout; \
		} \
		(v) = NFSTOV(np); \
		nfsm_loadattr(v, (struct vattr *)0); \
		}

#define	nfsm_loadattr(v,a) \
		{ struct vnode *tvp = (v); \
		if (error = nfs_loadattrcache(&tvp, &md, &dpos, (a))) { \
			m_freem(mrep); \
			goto nfsmout; \
		} \
		(v) = tvp; }

#define	nfsm_strsiz(s,m) \
		{ nfsm_dissect(tl,u_long *,NFSX_UNSIGNED); \
		if (((s) = fxdr_unsigned(long,*tl)) > (m)) { \
			m_freem(mrep); \
			error = EBADRPC; \
			goto nfsmout; \
		} }

#define	nfsm_srvstrsiz(s,m) \
		{ nfsm_dissect(tl,u_long *,NFSX_UNSIGNED); \
		if (((s) = fxdr_unsigned(long,*tl)) > (m) || (s) <= 0) { \
			error = EBADRPC; \
			nfsm_reply(0); \
		} }

#define nfsm_mtouio(p,s) \
		if ((s) > 0 && \
		   (error = nfsm_mbuftouio(&md,(p),(s),&dpos))) { \
			m_freem(mrep); \
			goto nfsmout; \
		}

#define nfsm_uiotom(p,s) \
		if (error = nfsm_uiotombuf((p),&mb,(s),&bpos)) { \
			m_freem(mreq); \
			goto nfsmout; \
		}

#define	nfsm_reqhead(v,a,s) \
		mb = mreq = nfsm_reqh((v),(a),(s),&bpos)

#define nfsm_reqdone	m_freem(mrep); \
		nfsmout: 

#define nfsm_rndup(a)	(((a)+3)&(~0x3))

#define	nfsm_request(v, t, p, c)	\
		if (error = nfs_request((v), mreq, (t), (p), \
		   (c), &mrep, &md, &dpos)) \
			goto nfsmout

#define	nfsm_strtom(a,s,m) \
		if ((s) > (m)) { \
			m_freem(mreq); \
			error = ENAMETOOLONG; \
			goto nfsmout; \
		} \
		t2 = nfsm_rndup(s)+NFSX_UNSIGNED; \
		if (t2 <= M_TRAILINGSPACE(mb)) { \
			nfsm_build(tl,u_long *,t2); \
			*tl++ = txdr_unsigned(s); \
			*(tl+((t2>>2)-2)) = 0; \
			bcopy((caddr_t)(a), (caddr_t)tl, (s)); \
		} else if (error = nfsm_strtmbuf(&mb, &bpos, (a), (s))) { \
			m_freem(mreq); \
			goto nfsmout; \
		}

#define	nfsm_srvdone \
		nfsmout: \
		return(error)

#define	nfsm_reply(s) \
		{ \
		nfsd->nd_repstat = error; \
		if (error) \
		   (void) nfs_rephead(0, nfsd, error, cache, &frev, \
			mrq, &mb, &bpos); \
		else \
		   (void) nfs_rephead((s), nfsd, error, cache, &frev, \
			mrq, &mb, &bpos); \
		m_freem(mrep); \
		mreq = *mrq; \
		if (error) \
			return(0); \
		}

#define	nfsm_adv(s) \
		t1 = mtod(md, caddr_t)+md->m_len-dpos; \
		if (t1 >= (s)) { \
			dpos += (s); \
		} else if (error = nfs_adv(&md, &dpos, (s), t1)) { \
			m_freem(mrep); \
			goto nfsmout; \
		}

#define nfsm_srvmtofh(f) \
		nfsm_dissect(tl, u_long *, NFSX_FH); \
		bcopy((caddr_t)tl, (caddr_t)f, NFSX_FH)

#define	nfsm_clget \
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
		tl = (u_long *)bp

#define	nfsm_srvfillattr \
	fp->fa_type = vtonfs_type(vap->va_type); \
	fp->fa_mode = vtonfs_mode(vap->va_type, vap->va_mode); \
	fp->fa_nlink = txdr_unsigned(vap->va_nlink); \
	fp->fa_uid = txdr_unsigned(vap->va_uid); \
	fp->fa_gid = txdr_unsigned(vap->va_gid); \
	if (nfsd->nd_nqlflag == NQL_NOVAL) { \
		fp->fa_nfsblocksize = txdr_unsigned(vap->va_blocksize); \
		if (vap->va_type == VFIFO) \
			fp->fa_nfsrdev = 0xffffffff; \
		else \
			fp->fa_nfsrdev = txdr_unsigned(vap->va_rdev); \
		fp->fa_nfsfsid = txdr_unsigned(vap->va_fsid); \
		fp->fa_nfsfileid = txdr_unsigned(vap->va_fileid); \
		fp->fa_nfssize = txdr_unsigned(vap->va_size); \
		fp->fa_nfsblocks = txdr_unsigned(vap->va_bytes / NFS_FABLKSIZE); \
		txdr_nfstime(&vap->va_atime, &fp->fa_nfsatime); \
		txdr_nfstime(&vap->va_mtime, &fp->fa_nfsmtime); \
		fp->fa_nfsctime.nfs_sec = txdr_unsigned(vap->va_ctime.ts_sec); \
		fp->fa_nfsctime.nfs_usec = txdr_unsigned(vap->va_gen); \
	} else { \
		fp->fa_nqblocksize = txdr_unsigned(vap->va_blocksize); \
		if (vap->va_type == VFIFO) \
			fp->fa_nqrdev = 0xffffffff; \
		else \
			fp->fa_nqrdev = txdr_unsigned(vap->va_rdev); \
		fp->fa_nqfsid = txdr_unsigned(vap->va_fsid); \
		fp->fa_nqfileid = txdr_unsigned(vap->va_fileid); \
		txdr_hyper(&vap->va_size, &fp->fa_nqsize); \
		txdr_hyper(&vap->va_bytes, &fp->fa_nqbytes); \
		txdr_nqtime(&vap->va_atime, &fp->fa_nqatime); \
		txdr_nqtime(&vap->va_mtime, &fp->fa_nqmtime); \
		txdr_nqtime(&vap->va_ctime, &fp->fa_nqctime); \
		fp->fa_nqflags = txdr_unsigned(vap->va_flags); \
		fp->fa_nqgen = txdr_unsigned(vap->va_gen); \
		txdr_hyper(&vap->va_filerev, &fp->fa_nqfilerev); \
	}

