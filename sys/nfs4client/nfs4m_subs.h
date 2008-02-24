/* $FreeBSD: src/sys/nfs4client/nfs4m_subs.h,v 1.2 2005/01/07 01:45:50 imp Exp $ */
/* $Id: nfs4m_subs.h,v 1.36 2003/11/05 14:59:01 rees Exp $ */

/*-
 * copyright (c) 2003
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and redistribute
 * this software and such derivative works for any purpose, so long as the name
 * of the university of michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software without specific,
 * written prior authorization.  if the above copyright notice or any other
 * identification of the university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the university
 * of michigan as to its fitness for any purpose, and without warranty by the
 * university of michigan of any kind, either express or implied, including
 * without limitation the implied warranties of merchantability and fitness for
 * a particular purpose. the regents of the university of michigan shall not be
 * liable for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been or is hereafter
 * advised of the possibility of such damages.
 */

#ifndef _NFS4CLIENT_NFSM4_SUBS_H
#define _NFS4CLIENT_NFSM4_SUBS_H

void nfsm_v4init(void);

void nfsm_buildf_xx(struct mbuf **mb, caddr_t *bpos, char *fmt, ...);
int nfsm_dissectf_xx(struct mbuf **md, caddr_t *dpos, char *fmt, ...);

int nfsm_v4build_compound_xx(struct nfs4_compound *, char *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_putfh_xx(struct nfs4_compound *, struct vnode *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_putfh_nv_xx(struct nfs4_compound *, struct nfs4_oparg_getfh *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_getattr_xx(struct nfs4_compound *, struct nfs4_oparg_getattr *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_finalize_xx(struct nfs4_compound *, struct mbuf **, caddr_t *);
int nfsm_v4build_getfh_xx(struct nfs4_compound *, struct nfs4_oparg_getfh *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_lookup_xx(struct nfs4_compound *, struct nfs4_oparg_lookup *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_setclientid_xx(struct nfs4_compound *,
        struct nfs4_oparg_setclientid *, struct mbuf **, caddr_t *);
int nfsm_v4build_setclientid_confirm_xx(struct nfs4_compound *,
        struct nfs4_oparg_setclientid *, struct mbuf **, caddr_t *);
int nfsm_v4build_close_xx(struct nfs4_compound *, struct nfs4_fctx *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_access_xx(struct nfs4_compound *, struct nfs4_oparg_access *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_open_xx(struct nfs4_compound *, struct nfs4_oparg_open *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_open_confirm_xx(struct nfs4_compound *, struct nfs4_oparg_open *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_read_xx(struct nfs4_compound *, struct nfs4_oparg_read *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_write_xx(struct nfs4_compound *, struct nfs4_oparg_write *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_commit_xx(struct nfs4_compound *, struct nfs4_oparg_commit *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_readdir_xx(struct nfs4_compound *, struct nfs4_oparg_readdir *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_renew_xx(struct nfs4_compound *, uint64_t,
        struct mbuf **, caddr_t *);
int nfsm_v4build_setattr_xx(struct nfs4_compound *, struct vattr *,
        struct nfs4_fctx *, struct mbuf **, caddr_t *);
int nfsm_v4build_create_xx(struct nfs4_compound *, struct nfs4_oparg_create *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_rename_xx(struct nfs4_compound *, struct nfs4_oparg_rename *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_link_xx(struct nfs4_compound *, struct nfs4_oparg_link *,
        struct mbuf **, caddr_t *);
int nfsm_v4build_remove_xx(struct nfs4_compound *, const char *, u_int,
        struct mbuf **, caddr_t *);

int nfsm_v4dissect_compound_xx(struct nfs4_compound *, struct mbuf **, caddr_t *);
int nfsm_v4dissect_getattr_xx(struct nfs4_compound *, struct nfs4_oparg_getattr *,
        struct mbuf **, caddr_t *);
int nfsm_v4dissect_getfh_xx(struct nfs4_compound *, struct nfs4_oparg_getfh *,
        struct mbuf **, caddr_t *);
int nfsm_v4dissect_setclientid_xx(struct nfs4_compound *,
        struct nfs4_oparg_setclientid *, struct mbuf **, caddr_t *);
int nfsm_v4dissect_close_xx(struct nfs4_compound *, struct nfs4_fctx *,
        struct mbuf **, caddr_t *);
int nfsm_v4dissect_access_xx(struct nfs4_compound *, struct nfs4_oparg_access *,
        struct mbuf **, caddr_t *);
int nfsm_v4dissect_open_xx(struct nfs4_compound *, struct nfs4_oparg_open *,
        struct mbuf **, caddr_t *);
int nfsm_v4dissect_open_confirm_xx(struct nfs4_compound *, struct nfs4_oparg_open *,
        struct mbuf **, caddr_t *);
int nfsm_v4dissect_read_xx(struct nfs4_compound *, struct nfs4_oparg_read *,
        struct mbuf **, caddr_t *);
int nfsm_v4dissect_write_xx(struct nfs4_compound *, struct nfs4_oparg_write *,
        struct mbuf **, caddr_t *);
int nfsm_v4dissect_commit_xx(struct nfs4_compound *, struct nfs4_oparg_commit *,
        struct mbuf **, caddr_t *);
int nfsm_v4dissect_setattr_xx(struct nfs4_compound *, struct mbuf **, caddr_t *);
int nfsm_v4dissect_create_xx(struct nfs4_compound *, struct nfs4_oparg_create *,
        struct mbuf **, caddr_t *);
int nfsm_v4dissect_readlink_xx(struct nfs4_compound *, struct uio *, 
        struct mbuf **, caddr_t *);

int nfsm_v4dissect_attrs_xx(struct nfsv4_fattr *, struct mbuf **, caddr_t *);

int nfsm_v4build_simple_xx(struct nfs4_compound *, uint32_t,
        struct mbuf **, caddr_t *);
int nfsm_v4dissect_simple_xx(struct nfs4_compound *, uint32_t,
        uint32_t, struct mbuf **, caddr_t *);

#define nfsm_v4build_putrootfh_xx(cp, mb, bpos) \
	nfsm_v4build_simple_xx((cp), NFSV4OP_PUTROOTFH, (mb), (bpos))

#define nfsm_v4build_lookupp_xx(cp, mb, bpos) \
	nfsm_v4build_simple_xx((cp), NFSV4OP_LOOKUPP, (mb), (bpos))

#define nfsm_v4build_savefh_xx(cp, mb, bpos)				\
	nfsm_v4build_simple_xx((cp), NFSV4OP_SAVEFH, (mb), (bpos))

#define nfsm_v4build_readlink_xx(cp, mb, bpos)				\
	nfsm_v4build_simple_xx((cp), NFSV4OP_READLINK, (mb), (bpos))


#define nfsm_v4dissect_putrootfh_xx(cp, mb, bpos)				\
	nfsm_v4dissect_simple_xx((cp), NFSV4OP_PUTROOTFH, 0, (mb), (bpos))

#define nfsm_v4dissect_lookup_xx(cp, mb, bpos) \
	nfsm_v4dissect_simple_xx((cp), NFSV4OP_LOOKUP, 0, (mb), (bpos))

#define nfsm_v4dissect_lookupp_xx(cp, mb, bpos) \
	nfsm_v4dissect_simple_xx((cp), NFSV4OP_LOOKUPP, 0, (mb), (bpos))

#define nfsm_v4dissect_putfh_xx(cp, mb, bpos) \
	nfsm_v4dissect_simple_xx((cp), NFSV4OP_PUTFH, 0, (mb), (bpos))

#define nfsm_v4dissect_renew_xx(cp, mb, bpos) \
	nfsm_v4dissect_simple_xx((cp), NFSV4OP_RENEW, 0, (mb), (bpos))

#define nfsm_v4dissect_setclientid_confirm_xx(cp, mb, bpos) \
	nfsm_v4dissect_simple_xx((cp), NFSV4OP_SETCLIENTID_CONFIRM, 0, (mb), (bpos))

#define nfsm_v4dissect_rename_xx(cp, mb, bpos)				\
	nfsm_v4dissect_simple_xx((cp), NFSV4OP_RENAME, 0, (mb), (bpos))

#define nfsm_v4dissect_link_xx(cp, mb, bpos)				\
	nfsm_v4dissect_simple_xx((cp), NFSV4OP_LINK, 0, (mb), (bpos))

#define nfsm_v4dissect_savefh_xx(cp, mb, bpos)				\
	nfsm_v4dissect_simple_xx((cp), NFSV4OP_SAVEFH, 0, (mb), (bpos))

#define nfsm_v4dissect_remove_xx(cp, mb, bpos)				\
	nfsm_v4dissect_simple_xx((cp), NFSV4OP_REMOVE, 0, (mb), (bpos))

#define nfsm_v4build_compound(cp, tag) do {			\
	int32_t t1;						\
	t1 = nfsm_v4build_compound_xx((cp), (tag), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_finalize(cp) do {			\
	int32_t t1;					\
	t1 = nfsm_v4build_finalize_xx((cp), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);				\
} while (0)

#define nfsm_v4build_putfh(cp, vp) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_putfh_xx((cp), (vp), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_putfh_nv(cp, gfh) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_putfh_nv_xx((cp), (gfh), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_putrootfh(cp) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_putrootfh_xx((cp), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_getattr(cp, ga) do {			\
	int32_t t1;						\
	t1 = nfsm_v4build_getattr_xx((cp), (ga), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_setattr(cp, vap, fcp) do {				\
	int32_t t1;							\
	t1 = nfsm_v4build_setattr_xx((cp), (vap), (fcp), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);						\
} while (0)

#define nfsm_v4build_lookup(cp, l) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_lookup_xx((cp), (l), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_lookupp(cp) do {			\
	int32_t t1;					\
	t1 = nfsm_v4build_lookupp_xx((cp), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);				\
} while (0)

#define nfsm_v4build_getfh(cp, gfh) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_getfh_xx((cp), (gfh), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_close(cp, fcp) do {			\
	int32_t t1;						\
	t1 = nfsm_v4build_close_xx((cp), (fcp), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_access(cp, acc) do {			\
	int32_t t1;						\
	t1 = nfsm_v4build_access_xx((cp), (acc), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_open(cp, o) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_open_xx((cp), (o), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_open_confirm(cp, o) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_open_confirm_xx((cp), (o), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_read(cp, r) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_read_xx((cp), (r), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_write(cp, w) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_write_xx((cp), (w), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_commit(cp, c) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_commit_xx((cp), (c), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_readdir(cp, r) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_readdir_xx((cp), (r), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_renew(cp, cid) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_renew_xx((cp), (cid), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_setclientid(cp, cid) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_setclientid_xx((cp), (cid), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_setclientid_confirm(cp, cid) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_setclientid_confirm_xx((cp), (cid), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_create(cp, c) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_create_xx((cp), (c), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_rename(cp, r) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_rename_xx((cp), (r), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_link(cp, r) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_link_xx((cp), (r), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_savefh(cp) do {			\
	int32_t t1;					\
	t1 = nfsm_v4build_savefh_xx((cp), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);				\
} while (0)

#define nfsm_v4build_readlink(cp) do {				\
	int32_t t1;						\
	t1 = nfsm_v4build_readlink_xx((cp), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);					\
} while (0)

#define nfsm_v4build_remove(cp, name, namelen) do {				\
	int32_t t1;								\
	t1 = nfsm_v4build_remove_xx((cp), (name), (namelen), &mb, &bpos);	\
	nfsm_bcheck(t1, mreq);							\
} while (0)

/* --- */

#define nfsm_v4dissect_compound(cp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_compound_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_putfh(cp)			\
do {							\
        int32_t t1;					\
        t1 = nfsm_v4dissect_putfh_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);				\
} while (0)

#define nfsm_v4dissect_putrootfh(cp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_putrootfh_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_getattr(cp, ga)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_getattr_xx((cp), (ga), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_setattr(cp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_setattr_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_lookup(cp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_lookup_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_lookupp(cp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_lookupp_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_getfh(cp, gfh)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_getfh_xx((cp), (gfh), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_setclientid(cp, sci)				\
do {									\
        int32_t t1;							\
        t1 = nfsm_v4dissect_setclientid_xx((cp), (sci), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);						\
} while (0)

#define nfsm_v4dissect_setclientid_confirm(cp)				\
do {									\
        int32_t t1;							\
        t1 = nfsm_v4dissect_setclientid_confirm_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);						\
} while (0)

#define nfsm_v4dissect_close(cp, fcp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_close_xx((cp), (fcp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_access(cp, acc)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_access_xx((cp), (acc), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_open(cp, openp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_open_xx((cp), (openp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_open_confirm(cp, openp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_open_confirm_xx((cp), (openp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_read(cp, r)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_read_xx((cp), (r), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_write(cp, w)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_write_xx((cp), (w), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_commit(cp, c)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_commit_xx((cp), (c), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_attrs(fattr)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_attrs_xx((fattr), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_renew(cp)			\
do {							\
        int32_t t1;					\
        t1 = nfsm_v4dissect_renew_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);				\
} while (0)

#define nfsm_v4dissect_create(cp, c)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_create_xx((cp), (c), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_rename(cp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_rename_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_link(cp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_link_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_savefh(cp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_savefh_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#define nfsm_v4dissect_readlink(cp, uiop)				\
do {									\
        int32_t t1;							\
        t1 = nfsm_v4dissect_readlink_xx((cp), (uiop), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);						\
} while (0)

#define nfsm_v4dissect_remove(cp)				\
do {								\
        int32_t t1;						\
        t1 = nfsm_v4dissect_remove_xx((cp), &md, &dpos);	\
        nfsm_dcheck(t1, mrep);					\
} while (0)

#endif /* _NFS4CLIENT_NFSM4_SUBS_H */
