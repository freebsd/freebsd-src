/* $FreeBSD: src/sys/nfs4client/nfs4_subs.c,v 1.6 2006/11/28 19:33:28 rees Exp $ */
/* $Id: nfs4_subs.c,v 1.52 2003/11/05 14:58:59 rees Exp $ */

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

#include <sys/cdefs.h>

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
#include <sys/sysproto.h>
#include <sys/fcntl.h>

#include <machine/stdarg.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <rpc/rpcclnt.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfs4client/nfs4.h>
#include <nfsclient/nfsnode.h>
#include <nfsclient/nfsmount.h>
#include <nfs/xdr_subs.h>
#include <nfsclient/nfsm_subs.h>

#include <nfs4client/nfs4_dev.h>
#include <nfs4client/nfs4_idmap.h>
#include <nfs4client/nfs4m_subs.h>

#include <netinet/in.h>

#define NFSM_DISSECT(s) do {							\
	tl = nfsm_dissect_xx((s), md, dpos);					\
	if (tl == NULL) {							\
		printf("NFSM_DISSECT error; allocation (%s/%d) (%s:%d)\n", #s, s, __FILE__, __LINE__);	\
		return (EBADRPC);						\
	}									\
} while (0)

#define NFSM_ADV(s) do {						\
	t1 = nfsm_adv_xx((s), md, dpos);				\
	if (t1 != 0) {							\
		printf("NFSM_ADV error; allocation (%s/%d) (%s:%d)\n", #s, s, __FILE__, __LINE__);	\
		return (EBADRPC);					\
	}								\
} while (0)

#define NFSM_MTOTIME(t) do {				\
	NFSM_DISSECT(3 * NFSX_UNSIGNED);		\
	(t).tv_sec = fxdr_hyper(tl);			\
	tl += 2;					\
	(t).tv_nsec = fxdr_unsigned(long, *tl++);	\
} while (0)

static uint32_t __fsinfo_bm[2], __fsattr_bm[2], __getattr_bm[2], __readdir_bm[2];

nfsv4bitmap nfsv4_fsinfobm = { 2, __fsinfo_bm };
nfsv4bitmap nfsv4_fsattrbm = { 2, __fsattr_bm };
nfsv4bitmap nfsv4_getattrbm = { 2, __getattr_bm };
nfsv4bitmap nfsv4_readdirbm = { 2, __readdir_bm };

/* Helper routines */
int nfsm_v4build_attrs_xx(struct vattr *, struct mbuf **, caddr_t *);
int nfsm_v4dissect_changeinfo_xx(nfsv4changeinfo *,  struct mbuf **, caddr_t *);

void
nfsm_v4init(void)
{

	/* Set up bitmasks */
	FA4_SET(FA4_FSID, __fsinfo_bm);
	FA4_SET(FA4_MAXREAD, __fsinfo_bm);
	FA4_SET(FA4_MAXWRITE, __fsinfo_bm);
	FA4_SET(FA4_LEASE_TIME, __fsinfo_bm);

	FA4_SET(FA4_FSID, __fsattr_bm);
	FA4_SET(FA4_FILES_FREE, __fsattr_bm);
	FA4_SET(FA4_FILES_TOTAL, __fsattr_bm);
	FA4_SET(FA4_SPACE_AVAIL, __fsattr_bm);
	FA4_SET(FA4_SPACE_FREE, __fsattr_bm);
	FA4_SET(FA4_SPACE_TOTAL, __fsattr_bm);

	FA4_SET(FA4_TYPE, __getattr_bm);
	FA4_SET(FA4_FSID, __getattr_bm);
	FA4_SET(FA4_SIZE, __getattr_bm);
	FA4_SET(FA4_MODE, __getattr_bm);
	FA4_SET(FA4_RAWDEV, __getattr_bm);
	FA4_SET(FA4_NUMLINKS, __getattr_bm);
	FA4_SET(FA4_OWNER, __getattr_bm);
	FA4_SET(FA4_OWNER_GROUP, __getattr_bm);
	FA4_SET(FA4_FILEID, __getattr_bm);
	FA4_SET(FA4_TIME_ACCESS, __getattr_bm);
	FA4_SET(FA4_TIME_CREATE, __getattr_bm);
	FA4_SET(FA4_TIME_METADATA, __getattr_bm);
	FA4_SET(FA4_TIME_MODIFY, __getattr_bm);

	FA4_SET(FA4_TYPE, __readdir_bm);
	FA4_SET(FA4_FSID, __readdir_bm);
	FA4_SET(FA4_FILEID, __readdir_bm);
	FA4_SET(FA4_RDATTR_ERROR, __readdir_bm);
}

/*
 * Util
 */

uint32_t
nfs_v4fileid4_to_fileid(uint64_t fid)
{
        return ((uint32_t)((fid >> 32) | fid));
}

void
nfs_v4initcompound(struct nfs4_compound *cp)
{
	bzero(cp, sizeof(*cp));
}

/*
 * Build/dissect XDR buffer with a format string.
 *
 *    u - unsigned
 *    h - hyper
 *    s - stringlength, string
 *    k - skip length (bytes)
 *    a - arraylength, componentlenght, array
 *    o - opaque fix length
 *    O - opaque var length in bytes
 */

void
nfsm_buildf_xx(struct mbuf **mb, caddr_t *bpos, char *fmt, ...)
{
	uint32_t *tl, t1, len, uval;
	uint64_t hval;
	va_list args;
	char *p, *which;

	va_start(args, fmt);
	for (which = fmt; *which != '\0'; which++)
		switch (*which) {
		case 'u':	/* Unsigned */
			tl = nfsm_build_xx(NFSX_UNSIGNED, mb, bpos);
			uval = va_arg(args, uint32_t);
			*tl++ = txdr_unsigned(uval);
			break;
		case 'h':	/* Hyper */
			tl = nfsm_build_xx(2 * NFSX_UNSIGNED, mb, bpos);
			hval = va_arg(args, uint64_t);
			txdr_hyper(hval, tl);
			break;
		case 'o':	/* Fixed-length opaque */
			len = va_arg(args, uint32_t);
			p = va_arg(args, char *);
			tl = nfsm_build_xx(nfsm_rndup(len), mb, bpos);
			bcopy(p, tl, len);
			break;
		case 'O':	/* Variable-length opaque */
		case 's':	/* String */
			len = va_arg(args, uint32_t);
			p = va_arg(args, char *);
			t1 = nfsm_strtom_xx(p, len, len, mb, bpos);
			break;
		case 'k':	/* Skip */
			len = va_arg(args, uint32_t);
			nfsm_build_xx(nfsm_rndup(len), mb, bpos);
			break;
		default:
			panic("Invalid buildf string %s[%c]", fmt, *which);
			break;
		}
	va_end(args);
}

int
nfsm_dissectf_xx(struct mbuf **md, caddr_t *dpos, char *fmt, ...)
{
	uint32_t *tl, t1, len, *uval;
	uint64_t *hval;
	va_list args;
	char *p, *which;

	va_start(args, fmt);
	for (which = fmt; *which != '\0'; which++)
		switch (*which) {
		case 'u':	/* Unsigned */
			tl = nfsm_dissect_xx(NFSX_UNSIGNED, md, dpos);
			if (tl == NULL)
				return (EBADRPC);
			uval = va_arg(args, uint32_t *);
			*uval = fxdr_unsigned(uint32_t, *tl++);
			break;
		case 'h':	/* Hyper */
			tl = nfsm_dissect_xx(2 * NFSX_UNSIGNED, md, dpos);
			if (tl == NULL)
				return (EBADRPC);
			hval = va_arg(args, uint64_t *);
			*hval = fxdr_hyper(tl);
			break;
		case 'o':	/* Fixed-length opaque */
			len = va_arg(args, uint32_t);
			p = va_arg(args, void *);
			tl = nfsm_dissect_xx(nfsm_rndup(len), md, dpos);
			if (tl == NULL)
				return (EBADRPC);
			bcopy(tl, p, len);
			break;
		case 'O':	/* Variable-length opaque */
		case 's':	/* String */
			len = va_arg(args, uint32_t);
			p = va_arg(args, char *);
			tl = nfsm_dissect_xx(nfsm_rndup(len), md, dpos);
			if (tl == NULL)
				return (EBADRPC);
			bcopy(tl, p, len);
			break;
		case 'k':	/* Skip bytes */
			len = va_arg(args, uint32_t);
			t1 = nfsm_adv_xx(nfsm_rndup(len), md, dpos);
			break;
		default:
			panic("Invalid dissectf string %s[%c]", fmt, *which);
			break;
		}
	va_end(args);

	return (0);
}

/*
 * XXX - There are a few problems with the way the postops are places
 * in the code.  Ideally, they should be taken care of immediately, as
 * to avoid uneceesary waits for mutexes, but then we would be
 * introducing even more complexity by having to handle two separate
 * cases.  Also, since they are placed at the end of the vnops', there
 * may be operations which sleep in between, further extending this
 * wait.  It is conceivable that there is a deadlock condition there,
 * too.
 *
 * Also, for vnops that do multiple operations, it's inconvenient
 * since on error, individual decoding will got nfsmout.
 */

int
nfs_v4postop(struct nfs4_compound *cp, int status)
{
	struct nfs4_fctx *fcp = cp->fcp;

	/*
	 * XXX does the previous result need to be stores with the
	 * lockowner?  ack, spec is unclear ..
	 */

	if (fcp != NULL)
		if (cp->seqidused < cp->rep_nops ||
		    (cp->seqidused + 1 == cp->rep_nops &&
			NFS4_SEQIDMUTATINGERROR(status)))
			fcp->lop->lo_seqid++;

	return (status);
}

int
nfs_v4handlestatus(int status, struct nfs4_compound *cp)
{
	return (status);
}

/*
 * Initial setup of compound.
 */

int
nfsm_v4build_compound_xx(struct nfs4_compound *cp, char *tag,
    struct mbuf **mb, caddr_t *bpos)
{
	uint32_t t1, *tl, siz;

	/* Tag */
	siz = strlen(tag);
	t1 = nfsm_rndup(siz) + NFSX_UNSIGNED;
	if (t1 <= M_TRAILINGSPACE(*mb)) {
		tl = nfsm_build_xx(t1, mb, bpos);
		*tl++ = txdr_unsigned(siz);
		*(tl + ((t1 >> 2) - 2)) = 0;
		bcopy(tag, tl, siz);
	} else {
		t1 = nfsm_strtmbuf(mb, bpos, (const char *)tag, siz);
		if (t1 != 0)
			return (t1);
	}

	/* Minor version and argarray*/
	tl = nfsm_build_xx(2 * NFSX_UNSIGNED, mb, bpos);
	*tl++ = txdr_unsigned(NFS4_MINOR_VERSION);
	/* Save for backfill */
	cp->req_nopsp = tl;
	*tl = txdr_unsigned(0);

	cp->curvp = NULL;
	cp->savevp = NULL;

	return (0);
}

/*
 * XXX
 * - backfill for stateid, and such
 */
int
nfsm_v4build_finalize_xx(struct nfs4_compound *cp, struct mbuf **mb, caddr_t *bpos)
{
	*cp->req_nopsp = txdr_unsigned(cp->req_nops);

	return (0);
}

int
nfsm_v4build_putfh_xx(struct nfs4_compound *cp, struct vnode *vp,
    struct mbuf **mb, caddr_t *bpos)
{
	uint32_t t1;

	/* Op */
	nfsm_buildf_xx(mb, bpos, "u", NFSV4OP_PUTFH);

	/* FH */
	t1 = nfsm_fhtom_xx(vp, 1, mb, bpos);
	if (t1 != 0)
		return (t1);

	cp->req_nops++;
	cp->curvp = vp;

	return (0);
}

int
nfsm_v4build_putfh_nv_xx(struct nfs4_compound *cp, struct nfs4_oparg_getfh *gfh,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "uuo",
	    NFSV4OP_PUTFH,
	    gfh->fh_len,
	    gfh->fh_len,
	    &gfh->fh_val);

	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_simple_xx(struct nfs4_compound *cp, uint32_t op,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "u", op);

	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_getattr_xx(struct nfs4_compound *cp, struct nfs4_oparg_getattr *ga,
    struct mbuf **mb, caddr_t *bpos)
{
	int i;

	/* Op + bitmap length + bitmap */
	nfsm_buildf_xx(mb, bpos, "uu", NFSV4OP_GETATTR, ga->bm->bmlen);
	for (i = 0; i < ga->bm->bmlen; i++)
		nfsm_buildf_xx(mb, bpos, "u", ga->bm->bmval[i]);

	ga->vp = cp->curvp;
	cp->req_nops++;		 

	return (0);
}

int
nfsm_v4build_setattr_xx(struct nfs4_compound *cp, struct vattr *vap,
    struct nfs4_fctx *fcp, struct mbuf **mb, caddr_t *bpos)
{
	int error;
	static char zero_stateid[NFSX_V4STATEID];

	nfsm_buildf_xx(mb, bpos, "uo",
	    NFSV4OP_SETATTR,
	    NFSX_V4STATEID, fcp ? fcp->stateid : zero_stateid);
	error = nfsm_v4build_attrs_xx(vap, mb, bpos);
	if (error == 0)
		cp->req_nops++;

	return (error);
}

int
nfsm_v4build_getfh_xx(struct nfs4_compound *cp, struct nfs4_oparg_getfh *gfh,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "u", NFSV4OP_GETFH);

	gfh->vp = cp->curvp;
	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_lookup_xx(struct nfs4_compound *cp, struct nfs4_oparg_lookup *l,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "us", NFSV4OP_LOOKUP, l->namelen, l->name);

	cp->curvp = l->vp;
	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_setclientid_xx(struct nfs4_compound *cp,
    struct nfs4_oparg_setclientid *sci, struct mbuf **mb, caddr_t *bpos)
{
	struct timeval tv;

	microtime(&tv);

	nfsm_buildf_xx(mb, bpos, "uuusussu",
	    NFSV4OP_SETCLIENTID,
	    tv.tv_sec, tv.tv_usec,
	    sci->namelen, sci->name,
	    sci->cb_prog,
	    sci->cb_netidlen, sci->cb_netid,
	    sci->cb_univaddrlen, sci->cb_univaddr,
	    0xCA11BACC);

	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_setclientid_confirm_xx(struct nfs4_compound *cp,
    struct nfs4_oparg_setclientid *sci, struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "uho",
	    NFSV4OP_SETCLIENTID_CONFIRM,
	    sci->clientid,
	    sizeof(sci->verf), sci->verf);

	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_open_xx(struct nfs4_compound *cp, struct nfs4_oparg_open *op,
    struct mbuf **mb, caddr_t *bpos)
{
	int error = 0;
	struct nfs4_lowner *lop = op->fcp->lop;

	nfsm_buildf_xx(mb, bpos, "uuuuhuu",
	    NFSV4OP_OPEN,
	    lop->lo_seqid,
	    op->flags & O_ACCMODE,
	    NFSV4OPENSHARE_DENY_NONE,
	    cp->nmp->nm_clientid,
	    4, lop->lo_id);

	if (op->flags & O_CREAT) {
		nfsm_buildf_xx(mb, bpos, "u", OTCREATE);
		/* openflag4: mode */
		nfsm_buildf_xx(mb, bpos, "u", CMUNCHECKED);
		/* openflag4: createattrs... */
		if (op->vap != NULL) {
			if (op->flags & O_TRUNC)
				op->vap->va_size = 0;
			error = nfsm_v4build_attrs_xx(op->vap, mb, bpos);
			if (error != 0)
				return (error);
		} else
			nfsm_buildf_xx(mb, bpos, "uu", 0, 0);
	} else
		nfsm_buildf_xx(mb, bpos, "u", OTNOCREATE);

	nfsm_buildf_xx(mb, bpos, "us", op->ctype,
	    op->cnp->cn_namelen, op->cnp->cn_nameptr);

	cp->seqidused = cp->req_nops++;
	cp->fcp = op->fcp;

	return (error);
}

/*
 * XXX
 * - Wait on recovery
 */
int
nfsm_v4build_open_confirm_xx(struct nfs4_compound *cp, struct nfs4_oparg_open *op,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "uou",
	    NFSV4OP_OPEN_CONFIRM,
	    NFSX_V4STATEID, op->fcp->stateid,
	    op->fcp->lop->lo_seqid);

	cp->seqidused = cp->req_nops++;
	cp->fcp = op->fcp;

	return (0);
}

/*
 * XXX
 * - Wait on recovery
 */
int
nfsm_v4build_close_xx(struct nfs4_compound *cp, struct nfs4_fctx *fcp,
    struct mbuf **mb, caddr_t *bpos)
{
	struct nfs4_lowner *lop = fcp->lop;

	nfsm_buildf_xx(mb, bpos, "uuo",
	    NFSV4OP_CLOSE,
	    lop->lo_seqid,
	    NFSX_V4STATEID, fcp->stateid);

	cp->seqidused = cp->req_nops++;
	cp->fcp = fcp;

	return (0);
}

int
nfsm_v4build_access_xx(struct nfs4_compound *cp, struct nfs4_oparg_access *acc,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "uu", NFSV4OP_ACCESS, acc->mode);
	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_read_xx(struct nfs4_compound *cp, struct nfs4_oparg_read *r,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "uohu",
	    NFSV4OP_READ,
	    NFSX_V4STATEID, r->fcp->stateid,
	    r->off,
	    r->maxcnt);
	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_write_xx(struct nfs4_compound *cp, struct nfs4_oparg_write *w,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "uohuu",
	    NFSV4OP_WRITE,
	    NFSX_V4STATEID, w->fcp->stateid,
	    w->off,
	    w->stable,
	    w->cnt);
	cp->req_nops++;
	return (nfsm_uiotombuf(w->uiop, mb, w->cnt, bpos));
}

int
nfsm_v4build_commit_xx(struct nfs4_compound *cp, struct nfs4_oparg_commit *c,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "uhu", NFSV4OP_COMMIT, c->start, c->len);
	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_readdir_xx(struct nfs4_compound *cp, struct nfs4_oparg_readdir *r,
    struct mbuf **mb, caddr_t *bpos)
{
	int i;

	nfsm_buildf_xx(mb, bpos, "uhouuu",
	    NFSV4OP_READDIR,
	    r->cookie,
	    sizeof(r->verf), r->verf,
	    r->cnt >> 4,	/* meaningless "dircount" field */
	    r->cnt,
	    r->bm->bmlen);

	for (i = 0; i < r->bm->bmlen; i++)
		nfsm_buildf_xx(mb, bpos, "u", r->bm->bmval[i]);

	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_renew_xx(struct nfs4_compound *cp, uint64_t cid,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "uh", NFSV4OP_RENEW, cid);
	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_create_xx(struct nfs4_compound *cp, struct nfs4_oparg_create *c,
    struct mbuf **mb, caddr_t *bpos)
{
	uint32_t t1;

	nfsm_buildf_xx(mb, bpos, "uu", NFSV4OP_CREATE, c->type);

	if (c->type == NFLNK)
		/* XXX strlen */
		nfsm_buildf_xx(mb, bpos, "s", strlen(c->linktext), c->linktext);
	else if (c->type == NFCHR || c->type == NFBLK)
		nfsm_buildf_xx(mb, bpos, "uu",
		    umajor(c->vap->va_rdev), uminor(c->vap->va_rdev));

	/* Name */
	nfsm_buildf_xx(mb, bpos, "s", c->namelen, c->name);	

	/* Attributes */
	t1 = nfsm_v4build_attrs_xx(c->vap, mb, bpos);
	if (t1 != 0)
		return (t1);

	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_rename_xx(struct nfs4_compound *cp, struct nfs4_oparg_rename *r,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "uss", NFSV4OP_RENAME, r->fnamelen, r->fname,
	    r->tnamelen, r->tname);

	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_link_xx(struct nfs4_compound *cp, struct nfs4_oparg_link *l,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "us", NFSV4OP_LINK, l->namelen, l->name);

	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_remove_xx(struct nfs4_compound *cp, const char *name, u_int namelen,
    struct mbuf **mb, caddr_t *bpos)
{
	nfsm_buildf_xx(mb, bpos, "us", NFSV4OP_REMOVE, namelen, name);

	cp->req_nops++;

	return (0);
}

int
nfsm_v4build_attrs_xx(struct vattr *vap, struct mbuf **mb, caddr_t *bpos)
{
	uint32_t *tl, *attrlenp, *bmvalp, len;
	size_t siz;

	tl = nfsm_build_xx(4 * NFSX_UNSIGNED, mb, bpos);

	*tl++ = txdr_unsigned(2);    /* bitmap length */
	bmvalp = tl;
	bzero(bmvalp, 8);
	tl += 2;
	attrlenp = tl;

	len = 0;
	if (vap->va_size != VNOVAL) {
		tl = nfsm_build_xx(2 * NFSX_UNSIGNED, mb, bpos);
		FA4_SET(FA4_SIZE, bmvalp);
		txdr_hyper(vap->va_size, tl); tl += 2;
		len += 2 * NFSX_UNSIGNED;
	}
	if (vap->va_mode != (u_short)VNOVAL) {
		tl = nfsm_build_xx(NFSX_UNSIGNED, mb, bpos);
		FA4_SET(FA4_MODE, bmvalp);
		*tl++ = txdr_unsigned(vap->va_mode);
		len += NFSX_UNSIGNED;
	}
	if (vap->va_uid != VNOVAL) {
		int error;
		char *name;
		error = idmap_uid_to_name(vap->va_uid, &name, &siz);
		if (error || name == NULL || siz == 0) {
		  	/* XXX */
			siz = sizeof("nobody") - 1;
			tl = nfsm_build_xx(NFSX_UNSIGNED + nfsm_rndup(siz), mb, 
			    bpos);
			*tl++ = txdr_unsigned(siz);
			bcopy("nobody", tl, siz);
			len += NFSX_UNSIGNED + nfsm_rndup(siz);
		} else {
			tl = nfsm_build_xx(NFSX_UNSIGNED + nfsm_rndup(siz), mb,
			    bpos);
			*tl++ = txdr_unsigned(siz);
			bcopy(name, tl, siz);
			len += NFSX_UNSIGNED + nfsm_rndup(siz);
		}
		FA4_SET(FA4_OWNER, bmvalp);
	}
	if (vap->va_gid != VNOVAL) {
		int error;
		char *name;
		error = idmap_gid_to_name(vap->va_gid, &name, &siz);
		if (error || name == NULL || siz == 0) {
		  	/* XXX */
			siz = sizeof("nogroup") - 1;
			tl = nfsm_build_xx(NFSX_UNSIGNED + nfsm_rndup(siz), mb, 
			    bpos);
			*tl++ = txdr_unsigned(siz);
			bcopy("nogroup", tl, siz);
			len += NFSX_UNSIGNED + nfsm_rndup(siz);
		} else {
			tl = nfsm_build_xx(NFSX_UNSIGNED + nfsm_rndup(siz), mb,
			    bpos);
			*tl++ = txdr_unsigned(siz);
			bcopy(name, tl, siz);
			len += NFSX_UNSIGNED + nfsm_rndup(siz);
		}
		FA4_SET(FA4_OWNER_GROUP, bmvalp);
	}
	if (vap->va_atime.tv_sec != VNOVAL) {
		uint64_t val = vap->va_atime.tv_sec;
		tl = nfsm_build_xx(4 * NFSX_UNSIGNED, mb, bpos);
		FA4_SET(FA4_TIME_ACCESS_SET, bmvalp);
		*tl++ = txdr_unsigned(THCLIENTTIME);
		txdr_hyper(val, tl); tl += 2;
		*tl++ = txdr_unsigned(vap->va_atime.tv_nsec);
		len += 4 * NFSX_UNSIGNED;
	}
	if (vap->va_mtime.tv_sec != VNOVAL) {
		uint64_t val = vap->va_mtime.tv_sec;
		tl = nfsm_build_xx(4 * NFSX_UNSIGNED, mb, bpos);
		FA4_SET(FA4_TIME_MODIFY_SET, bmvalp);
		*tl++ = txdr_unsigned(THCLIENTTIME);
		txdr_hyper(val, tl); tl += 2;
		*tl++ = txdr_unsigned(vap->va_mtime.tv_nsec);
		len += 4 * NFSX_UNSIGNED;
	}

	bmvalp[0] = txdr_unsigned(bmvalp[0]);
	bmvalp[1] = txdr_unsigned(bmvalp[1]);

	*attrlenp = txdr_unsigned(len);

	return (0);
}

int
nfsm_v4dissect_compound_xx(struct nfs4_compound *cp, struct mbuf **md, caddr_t *dpos)
{
	uint32_t taglen, t1, *tl;

	tl = nfsm_dissect_xx(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return (EBADRPC);

	/* Reply status is handled by the RPC code */

	taglen = fxdr_unsigned(uint32_t, *tl++);
	t1 = nfsm_adv_xx(nfsm_rndup(taglen), md, dpos);
	if (t1 != 0)
		return (EBADRPC);

	tl = nfsm_dissect_xx(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return (EBADRPC);

	cp->rep_nops = fxdr_unsigned(uint32_t, *tl++);

	return (0);
}

int
nfsm_v4dissect_simple_xx(struct nfs4_compound *cp, uint32_t op,
    uint32_t skipbytes, struct mbuf **md, caddr_t *dpos)
{
	uint32_t t1, dop, status;

	t1 = nfsm_dissectf_xx(md, dpos, "uu", &dop, &status);
	if (t1 != 0)
		return (t1);

	if (dop != op || status != 0)
		return (EBADRPC);

	if (skipbytes > 0)
		NFSM_ADV(nfsm_rndup(skipbytes));

	return (0);
}

int
nfsm_v4dissect_getattr_xx(struct nfs4_compound *cp, struct nfs4_oparg_getattr *ga,
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t *tl;

	tl = nfsm_dissect_xx(2 * NFSX_UNSIGNED, md, dpos);
	if (tl == NULL || fxdr_unsigned(uint32_t, *tl++) != NFSV4OP_GETATTR ||
	    *tl++ != 0)
		return (EBADRPC);

	return (nfsm_v4dissect_attrs_xx(&ga->fa, md, dpos));
}

int
nfsm_v4dissect_setattr_xx(struct nfs4_compound *cp, struct mbuf **md, caddr_t *dpos)
{
	uint32_t t1, op, bmlen, status;

	t1 = nfsm_dissectf_xx(md, dpos, "uu", &op, &status);
	if (t1 != 0)
		return (t1);

	if (op != NFSV4OP_SETATTR || status != 0)
		return (EBADRPC);

	t1 = nfsm_dissectf_xx(md, dpos, "u", &bmlen);
	if (t1 != 0)
		return (t1);

	return (nfsm_dissectf_xx(md, dpos, "k", bmlen << 2));
}

int
nfsm_v4dissect_getfh_xx(struct nfs4_compound *cp, struct nfs4_oparg_getfh *gfh,
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t *tl, len, xdrlen;

	tl = nfsm_dissect_xx(2 * NFSX_UNSIGNED, md, dpos);
	if (tl == NULL || fxdr_unsigned(uint32_t, *tl++) != NFSV4OP_GETFH)
		return (EBADRPC);

	if (*tl++ != 0)
		return (EBADRPC);

	NFSM_DISSECT(NFSX_UNSIGNED);
	len = fxdr_unsigned(uint32_t, *tl++);
	if (len > NFSX_V4FH)
		return (EBADRPC);

	/* XXX integrate this into nfs_mtofh()? */

	gfh->fh_len = len;
	xdrlen = nfsm_rndup(len);

	NFSM_DISSECT(xdrlen);
	bcopy(tl, &gfh->fh_val, xdrlen);

	return (0);
}

int
nfsm_v4dissect_setclientid_xx(struct nfs4_compound *cp,
    struct nfs4_oparg_setclientid *sci, struct mbuf **md, caddr_t *dpos)
{
	uint32_t *tl;

	tl = nfsm_dissect_xx(2 * NFSX_UNSIGNED, md, dpos);
	if (tl == NULL || fxdr_unsigned(uint32_t, *tl++) != NFSV4OP_SETCLIENTID)
		return (EBADRPC);

	/* Handle NFS4ERR_CLID_INUSE specially */
	if (*tl++ != 0)
		return (EBADRPC);

	NFSM_DISSECT(2 * NFSX_UNSIGNED);
	sci->clientid = fxdr_hyper(tl);

	NFSM_DISSECT(nfsm_rndup(NFSX_V4VERF));
	bcopy(tl, sci->verf, NFSX_V4VERF);

	return (0);
}

int
nfsm_v4dissect_close_xx(struct nfs4_compound *cp, struct nfs4_fctx *fcp,
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t *tl, t1;

	tl = nfsm_dissect_xx(2 * NFSX_UNSIGNED, md, dpos);
	if (tl == NULL || fxdr_unsigned(uint32_t, *tl++) != NFSV4OP_CLOSE ||
	    *tl++ != 0)
		return (EBADRPC);

	/* Copy stateid */
	t1 = nfsm_dissectf_xx(md, dpos, "o", NFSX_V4STATEID, fcp->stateid);
	if (t1 != 0)
		return (t1);

	return (0);
}

int
nfsm_v4dissect_access_xx(struct nfs4_compound *cp, struct nfs4_oparg_access *acc,
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t *tl;

	tl = nfsm_dissect_xx(4 * NFSX_UNSIGNED, md, dpos);
	if (tl == NULL || fxdr_unsigned(uint32_t, *tl++) != NFSV4OP_ACCESS ||
	    *tl++ != 0)
		return (EBADRPC);

	acc->supported = fxdr_unsigned(uint32_t, *tl++);
	acc->rmode = fxdr_unsigned(uint32_t, *tl++);

	return (0);
}

int
nfsm_v4dissect_open_xx(struct nfs4_compound *cp, struct nfs4_oparg_open *op,
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t *tl, t1, bmlen, delegtype = ODNONE;
	int error = 0;
	nfsv4changeinfo cinfo;
	struct nfs4_fctx *fcp = op->fcp;

	tl = nfsm_dissect_xx(2 * NFSX_UNSIGNED, md, dpos);
	if (tl == NULL || fxdr_unsigned(uint32_t, *tl++) != NFSV4OP_OPEN ||
	    *tl++ != 0)
		return (EBADRPC);

	t1 = nfsm_dissectf_xx(md, dpos, "o", NFSX_V4STATEID, fcp->stateid);
	if (t1 != 0)
		return (t1);

	error = nfsm_v4dissect_changeinfo_xx(&cinfo, md, dpos);
	if (error != 0)
		goto nfsmout;

	NFSM_DISSECT(2 * NFSX_UNSIGNED);

	op->rflags = fxdr_unsigned(uint32_t, *tl++);
	bmlen = fxdr_unsigned(uint32_t, *tl++);
	if (bmlen > 2) {
		error = EBADRPC;
		goto nfsmout;
	}

	/* Skip */
	NFSM_ADV(nfsm_rndup(bmlen << 2));

	NFSM_DISSECT(NFSX_UNSIGNED);
	delegtype = fxdr_unsigned(uint32_t, *tl++);
	switch (delegtype) {
	case ODREAD:
	case ODWRITE:
		printf("nfs4: client delegation not yet supported\n");
		error = EOPNOTSUPP;
		goto nfsmout;
		break;
	case ODNONE:
	default:
		break;
	}

 nfsmout:
	return (error);
}

int
nfsm_v4dissect_open_confirm_xx(struct nfs4_compound *cp, struct nfs4_oparg_open *op,
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t *tl;

	tl = nfsm_dissect_xx(2 * NFSX_UNSIGNED, md, dpos);
	if (tl == NULL || fxdr_unsigned(uint32_t, *tl++) != NFSV4OP_OPEN_CONFIRM ||
	    *tl++ != 0)
		return (EBADRPC);

	return nfsm_dissectf_xx(md, dpos, "o", NFSX_V4STATEID, op->fcp->stateid);
}

int
nfsm_v4dissect_read_xx(struct nfs4_compound *cp, struct nfs4_oparg_read *r,
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t op, status, t1;

	t1 = nfsm_dissectf_xx(md, dpos, "uu", &op, &status);
	if (t1 != 0)
		return (t1);

	if (op != NFSV4OP_READ || status != 0)
		return (EBADRPC);

	t1 = nfsm_dissectf_xx(md, dpos, "uu", &r->eof, &r->retlen);
	if (t1 != 0)
		return (t1);

	return (nfsm_mbuftouio(md, r->uiop, r->retlen, dpos));
}

int
nfsm_v4dissect_write_xx(struct nfs4_compound *cp, struct nfs4_oparg_write *w,
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t op, status, t1;

	t1 = nfsm_dissectf_xx(md, dpos, "uu", &op, &status);
	if (t1 != 0)
		return (t1);

	if (op != NFSV4OP_WRITE || status != 0)
		return (EBADRPC);

	return (nfsm_dissectf_xx(md, dpos, "uuo", &w->retlen, &w->committed,
		    NFSX_V4VERF, w->wverf));
}

int
nfsm_v4dissect_commit_xx(struct nfs4_compound *cp, struct nfs4_oparg_commit *c,
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t t1, op, status;

	t1 = nfsm_dissectf_xx(md, dpos, "uu", &op, &status);
	if (t1 != 0)
		return (t1);

	if (op != NFSV4OP_COMMIT || status != 0)
		return (EBADRPC);

	return (nfsm_dissectf_xx(md, dpos, "o", NFSX_V4VERF, c->verf));
}

int
nfsm_v4dissect_create_xx(struct nfs4_compound *cp, struct nfs4_oparg_create *c,
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t t1, *tl, op, status, bmlen;
	nfsv4changeinfo ci;

	t1 = nfsm_dissectf_xx(md, dpos, "uu", &op, &status);
	if (t1 != 0)
		return (t1);

	if (op != NFSV4OP_CREATE || status != 0)
		return (EBADRPC);

	/* Just throw this away for now */
	t1 = nfsm_v4dissect_changeinfo_xx(&ci, md, dpos);
	if (t1 != 0)
		return (t1);

	/* Throw this away too */
	NFSM_DISSECT(NFSX_UNSIGNED);
	bmlen = fxdr_unsigned(uint32_t, *tl++);
	NFSM_DISSECT(bmlen * NFSX_UNSIGNED);
	tl += bmlen;

	return 0;
}

int
nfsm_v4dissect_readlink_xx(struct nfs4_compound *cp, struct uio *uiop, 
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t t1, *tl, op, status, linklen;

	t1 = nfsm_dissectf_xx(md, dpos, "uu", &op, &status);
	if (t1 != 0)
		return (t1);

	if (op != NFSV4OP_READLINK || status != 0)
		return (EBADRPC);

	/* Do this one manually for careful checking of sizes. */
	NFSM_DISSECT(NFSX_UNSIGNED);
	linklen = fxdr_unsigned(uint32_t, *tl++);
	if (linklen <= 0)
		return (EBADRPC);

	return (nfsm_mbuftouio(md, uiop, MIN(linklen, uiop->uio_resid), dpos));
}

int
nfsm_v4dissect_changeinfo_xx(nfsv4changeinfo *ci,
    struct mbuf **md, caddr_t *dpos)
{
	uint32_t *tl;

	NFSM_DISSECT(5 * NFSX_UNSIGNED);

	ci->ciatomic = fxdr_unsigned(uint32_t, *tl++);
	ci->cibefore = fxdr_hyper(tl); tl += 2;
	ci->ciafter = fxdr_hyper(tl); tl += 2;

	return (0);
}

int
nfsm_v4dissect_attrs_xx(struct nfsv4_fattr *fa, struct mbuf **md, caddr_t *dpos)
{
	uint32_t t1, *tl, bmlen, bmval[2], attrlen, len = 0;

	/* Bitmap length + value */
	NFSM_DISSECT(NFSX_UNSIGNED);

	bmlen = fxdr_unsigned(uint32_t, *tl++);
	if (bmlen > 2)
		return (EBADRPC);

	if (bmlen == 0)
		return (0);

	NFSM_DISSECT(nfsm_rndup(bmlen << 2) + NFSX_UNSIGNED);

	bmval[0] = bmlen > 0 ? fxdr_unsigned(uint32_t, *tl++) : 0;
	bmval[1] = bmlen > 1 ? fxdr_unsigned(uint32_t, *tl++) : 0;

	/* Attribute length */
	attrlen = fxdr_unsigned(uint32_t, *tl++);

	/*
	 * XXX check for correct (<=) attributes mask return from
	 * server.  need to pass this in.
	 */

	if (FA4_ISSET(FA4_TYPE, bmval)) {
		/* overflow check */
		NFSM_DISSECT(NFSX_UNSIGNED);
		fa->fa4_type = fxdr_unsigned(uint32_t, *tl++);
		fa->fa4_valid |= FA4V_TYPE;
		len += NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_CHANGE, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_changeid = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_CHANGEID;
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_SIZE, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_size = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_SIZE;
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_FSID, bmval)) {
		NFSM_DISSECT(4 * NFSX_UNSIGNED);
		fa->fa4_fsid_major = fxdr_hyper(tl); tl += 2;
		fa->fa4_fsid_minor = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_SIZE;
		len += 4 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_LEASE_TIME, bmval)) {
		NFSM_DISSECT(NFSX_UNSIGNED);
		fa->fa4_lease_time = fxdr_unsigned(uint32_t, *tl++);
		fa->fa4_valid |= FA4V_LEASE_TIME;
		len += NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_RDATTR_ERROR, bmval)) {
		/* ignore for now; we only ask for it so the compound won't fail */
		NFSM_DISSECT(NFSX_UNSIGNED);
		tl++;
		len += NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_FILEID, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_fileid = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_FILEID;
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_FILES_FREE, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_ffree = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_FFREE;
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_FILES_TOTAL, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_ftotal = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_FTOTAL;
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_MAXFILESIZE, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_maxfilesize = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_MAXFILESIZE;
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_MAXNAME, bmval)) {
		NFSM_DISSECT(NFSX_UNSIGNED);
		fa->fa4_maxname = fxdr_unsigned(uint32_t, *tl++);
		fa->fa4_valid |= FA4V_MAXNAME;
		len += NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_MAXREAD, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_maxread = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_MAXREAD;
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_MAXWRITE, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_maxwrite = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_MAXWRITE;
		len += 2 * NFSX_UNSIGNED;
	}

	if (FA4_ISSET(FA4_MODE, bmval)) {
		NFSM_DISSECT(NFSX_UNSIGNED);
		fa->fa4_mode = fxdr_unsigned(mode_t, *tl++);
		fa->fa4_valid |= FA4V_MODE;
		len += NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_NUMLINKS, bmval)) {
		NFSM_DISSECT(NFSX_UNSIGNED);
		fa->fa4_nlink = fxdr_unsigned(nlink_t, *tl++);
		fa->fa4_valid |= FA4V_NLINK;
		len += NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_OWNER, bmval)) {
		uint32_t ownerlen;
		int error;

		NFSM_DISSECT(NFSX_UNSIGNED);

		ownerlen = fxdr_unsigned(uint32_t, *tl++);
		NFSM_DISSECT(nfsm_rndup(ownerlen));
		error = idmap_name_to_uid((char *)tl, ownerlen, &fa->fa4_uid);
		if (error) 
			fa->fa4_uid = -2;
		fa->fa4_valid |= FA4V_UID;
		len += NFSX_UNSIGNED + nfsm_rndup(ownerlen);
	}
	if (FA4_ISSET(FA4_OWNER_GROUP, bmval)) {
		uint32_t ownergrouplen;
		int error;

		NFSM_DISSECT(NFSX_UNSIGNED);
		ownergrouplen = fxdr_unsigned(uint32_t, *tl++);
		NFSM_DISSECT(nfsm_rndup(ownergrouplen));
		error = idmap_name_to_gid((char *)tl, ownergrouplen, &fa->fa4_gid);
		if (error) 
			fa->fa4_gid = -2;
		fa->fa4_valid |= FA4V_GID;
		len += NFSX_UNSIGNED + nfsm_rndup(ownergrouplen);
	}
	if (FA4_ISSET(FA4_RAWDEV, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_rdev_major = fxdr_unsigned(uint32_t, *tl++);
		fa->fa4_rdev_minor = fxdr_unsigned(uint32_t, *tl++);
		fa->fa4_valid |= FA4V_RDEV;
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_SPACE_AVAIL, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_savail = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_SAVAIL;
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_SPACE_FREE, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_sfree = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_SFREE;
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_SPACE_TOTAL, bmval)) {
		NFSM_DISSECT(2 * NFSX_UNSIGNED);
		fa->fa4_stotal = fxdr_hyper(tl);
		fa->fa4_valid |= FA4V_STOTAL;
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_SPACE_USED, bmval)) {
		NFSM_ADV(2 * NFSX_UNSIGNED);
		len += 2 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_TIME_ACCESS, bmval)) {
		NFSM_MTOTIME(fa->fa4_atime);
		fa->fa4_valid |= FA4V_ATIME;
		len += 3 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_TIME_CREATE, bmval)) {
		NFSM_MTOTIME(fa->fa4_btime);
		fa->fa4_valid |= FA4V_BTIME;
		len += 3 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_TIME_METADATA, bmval)) {
		NFSM_MTOTIME(fa->fa4_ctime);
		fa->fa4_valid |= FA4V_CTIME;
		len += 3 * NFSX_UNSIGNED;
	}
	if (FA4_ISSET(FA4_TIME_MODIFY, bmval)) {
		NFSM_MTOTIME(fa->fa4_mtime);
		fa->fa4_valid |= FA4V_MTIME;
		len += 3 * NFSX_UNSIGNED;
	}

	if (len != attrlen)
		return (EBADRPC);

	return (0);
}
