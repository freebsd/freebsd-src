/*
 * linux/fs/nfsd/nfs3xdr.c
 *
 * XDR support for nfsd/protocol version 3.
 *
 * Copyright (C) 1995, 1996, 1997 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/nfs3.h>

#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/svc.h>
#include <linux/nfsd/nfsd.h>
#include <linux/nfsd/xdr3.h>

#define NFSDDBG_FACILITY		NFSDDBG_XDR

#ifdef NFSD_OPTIMIZE_SPACE
# define inline
#endif


/*
 * Mapping of S_IF* types to NFS file types
 */
static u32	nfs3_ftypes[] = {
	NF3NON,  NF3FIFO, NF3CHR, NF3BAD,
	NF3DIR,  NF3BAD,  NF3BLK, NF3BAD,
	NF3REG,  NF3BAD,  NF3LNK, NF3BAD,
	NF3SOCK, NF3BAD,  NF3LNK, NF3BAD,
};

/*
 * XDR functions for basic NFS types
 */
static inline u32 *
encode_time3(u32 *p, time_t secs)
{
	*p++ = htonl((u32) secs); *p++ = 0;
	return p;
}

static inline u32 *
decode_time3(u32 *p, time_t *secp)
{
	*secp = ntohl(*p++);
	return p + 1;
}

static inline u32 *
decode_fh(u32 *p, struct svc_fh *fhp)
{
	unsigned int size;
	fh_init(fhp, NFS3_FHSIZE);
	size = ntohl(*p++);
	if (size > NFS3_FHSIZE)
		return NULL;

	memcpy(&fhp->fh_handle.fh_base, p, size);
	fhp->fh_handle.fh_size = size;
	return p + XDR_QUADLEN(size);
}

static inline u32 *
encode_fh(u32 *p, struct svc_fh *fhp)
{
	int size = fhp->fh_handle.fh_size;
	*p++ = htonl(size);
	if (size) p[XDR_QUADLEN(size)-1]=0;
	memcpy(p, &fhp->fh_handle.fh_base, size);
	return p + XDR_QUADLEN(size);
}

/*
 * Decode a file name and make sure that the path contains
 * no slashes or null bytes.
 */
static inline u32 *
decode_filename(u32 *p, char **namp, int *lenp)
{
	char		*name;
	int		i;

	if ((p = xdr_decode_string_inplace(p, namp, lenp, NFS3_MAXNAMLEN)) != NULL) {
		for (i = 0, name = *namp; i < *lenp; i++, name++) {
			if (*name == '\0' || *name == '/')
				return NULL;
		}
	}

	return p;
}

static inline u32 *
decode_pathname(u32 *p, char **namp, int *lenp)
{
	char		*name;
	int		i;

	if ((p = xdr_decode_string(p, namp, lenp, NFS3_MAXPATHLEN)) != NULL) {
		for (i = 0, name = *namp; i < *lenp; i++, name++) {
			if (*name == '\0')
				return NULL;
		}
	}

	return p;
}

static inline u32 *
decode_sattr3(u32 *p, struct iattr *iap)
{
	u32	tmp;

	iap->ia_valid = 0;

	if (*p++) {
		iap->ia_valid |= ATTR_MODE;
		iap->ia_mode = ntohl(*p++);
	}
	if (*p++) {
		iap->ia_valid |= ATTR_UID;
		iap->ia_uid = ntohl(*p++);
	}
	if (*p++) {
		iap->ia_valid |= ATTR_GID;
		iap->ia_gid = ntohl(*p++);
	}
	if (*p++) {
		u64	newsize;

		iap->ia_valid |= ATTR_SIZE;
		p = xdr_decode_hyper(p, &newsize);
		if (newsize <= NFS_OFFSET_MAX)
			iap->ia_size = newsize;
		else
			iap->ia_size = NFS_OFFSET_MAX;
	}
	if ((tmp = ntohl(*p++)) == 1) {	/* set to server time */
		iap->ia_valid |= ATTR_ATIME;
	} else if (tmp == 2) {		/* set to client time */
		iap->ia_valid |= ATTR_ATIME | ATTR_ATIME_SET;
		iap->ia_atime = ntohl(*p++), p++;
	}
	if ((tmp = ntohl(*p++)) == 1) {	/* set to server time */
		iap->ia_valid |= ATTR_MTIME;
	} else if (tmp == 2) {		/* set to client time */
		iap->ia_valid |= ATTR_MTIME | ATTR_MTIME_SET;
		iap->ia_mtime = ntohl(*p++), p++;
	}
	return p;
}

static inline u32 *
encode_fattr3(struct svc_rqst *rqstp, u32 *p, struct svc_fh *fhp)
{
	struct inode	*inode = fhp->fh_dentry->d_inode;

	*p++ = htonl(nfs3_ftypes[(inode->i_mode & S_IFMT) >> 12]);
	*p++ = htonl((u32) inode->i_mode);
	*p++ = htonl((u32) inode->i_nlink);
	*p++ = htonl((u32) nfsd_ruid(rqstp, inode->i_uid));
	*p++ = htonl((u32) nfsd_rgid(rqstp, inode->i_gid));
	if (S_ISLNK(inode->i_mode) && inode->i_size > NFS3_MAXPATHLEN) {
		p = xdr_encode_hyper(p, (u64) NFS3_MAXPATHLEN);
	} else {
		p = xdr_encode_hyper(p, (u64) inode->i_size);
	}
	if (inode->i_blksize == 0 && inode->i_blocks == 0)
		/* Minix file system(?) i_size is (hopefully) close enough */
		p = xdr_encode_hyper(p, (u64)(inode->i_size +511)& ~511);
	else
		p = xdr_encode_hyper(p, ((u64)inode->i_blocks) << 9);
	*p++ = htonl((u32) MAJOR(inode->i_rdev));
	*p++ = htonl((u32) MINOR(inode->i_rdev));
	if (rqstp->rq_reffh->fh_version == 1
	    && rqstp->rq_reffh->fh_fsid_type == 1
	    && (fhp->fh_export->ex_flags & NFSEXP_FSID))
		p = xdr_encode_hyper(p, (u64) fhp->fh_export->ex_fsid);
	else
		p = xdr_encode_hyper(p, (u64) inode->i_dev);
	p = xdr_encode_hyper(p, (u64) inode->i_ino);
	p = encode_time3(p, inode->i_atime);
	p = encode_time3(p, lease_get_mtime(inode));
	p = encode_time3(p, inode->i_ctime);

	return p;
}

static inline u32 *
encode_saved_post_attr(struct svc_rqst *rqstp, u32 *p, struct svc_fh *fhp)
{
	struct inode	*inode = fhp->fh_dentry->d_inode;

	/* Attributes to follow */
	*p++ = xdr_one;

	*p++ = htonl(nfs3_ftypes[(fhp->fh_post_mode & S_IFMT) >> 12]);
	*p++ = htonl((u32) fhp->fh_post_mode);
	*p++ = htonl((u32) fhp->fh_post_nlink);
	*p++ = htonl((u32) nfsd_ruid(rqstp, fhp->fh_post_uid));
	*p++ = htonl((u32) nfsd_rgid(rqstp, fhp->fh_post_gid));
	if (S_ISLNK(fhp->fh_post_mode) && fhp->fh_post_size > NFS3_MAXPATHLEN) {
		p = xdr_encode_hyper(p, (u64) NFS3_MAXPATHLEN);
	} else {
		p = xdr_encode_hyper(p, (u64) fhp->fh_post_size);
	}
	p = xdr_encode_hyper(p, ((u64)fhp->fh_post_blocks) << 9);
	*p++ = htonl((u32) MAJOR(fhp->fh_post_rdev));
	*p++ = htonl((u32) MINOR(fhp->fh_post_rdev));
	if (rqstp->rq_reffh->fh_version == 1
	    && rqstp->rq_reffh->fh_fsid_type == 1
	    && (fhp->fh_export->ex_flags & NFSEXP_FSID))
		p = xdr_encode_hyper(p, (u64) fhp->fh_export->ex_fsid);
	else
		p = xdr_encode_hyper(p, (u64) inode->i_dev);
	p = xdr_encode_hyper(p, (u64) inode->i_ino);
	p = encode_time3(p, fhp->fh_post_atime);
	p = encode_time3(p, fhp->fh_post_mtime);
	p = encode_time3(p, fhp->fh_post_ctime);

	return p;
}

/*
 * Encode post-operation attributes.
 * The inode may be NULL if the call failed because of a stale file
 * handle. In this case, no attributes are returned.
 */
static u32 *
encode_post_op_attr(struct svc_rqst *rqstp, u32 *p, struct svc_fh *fhp)
{
	struct dentry *dentry = fhp->fh_dentry;
	if (dentry && dentry->d_inode != NULL) {
		*p++ = xdr_one;		/* attributes follow */
		return encode_fattr3(rqstp, p, fhp);
	}
	*p++ = xdr_zero;
	return p;
}

/*
 * Enocde weak cache consistency data
 */
static u32 *
encode_wcc_data(struct svc_rqst *rqstp, u32 *p, struct svc_fh *fhp)
{
	struct dentry	*dentry = fhp->fh_dentry;

	if (dentry && dentry->d_inode && fhp->fh_post_saved) {
		if (fhp->fh_pre_saved) {
			*p++ = xdr_one;
			p = xdr_encode_hyper(p, (u64) fhp->fh_pre_size);
			p = encode_time3(p, fhp->fh_pre_mtime);
			p = encode_time3(p, fhp->fh_pre_ctime);
		} else {
			*p++ = xdr_zero;
		}
		return encode_saved_post_attr(rqstp, p, fhp);
	}
	/* no pre- or post-attrs */
	*p++ = xdr_zero;
	return encode_post_op_attr(rqstp, p, fhp);
}

/*
 * Check buffer bounds after decoding arguments
 */
static inline int
xdr_argsize_check(struct svc_rqst *rqstp, u32 *p)
{
	struct svc_buf	*buf = &rqstp->rq_argbuf;

	return p - buf->base <= buf->buflen;
}

static inline int
xdr_ressize_check(struct svc_rqst *rqstp, u32 *p)
{
	struct svc_buf	*buf = &rqstp->rq_resbuf;

	buf->len = p - buf->base;
	dprintk("nfsd: ressize_check p %p base %p len %d\n",
			p, buf->base, buf->buflen);
	return (buf->len <= buf->buflen);
}

/*
 * XDR decode functions
 */
int
nfs3svc_decode_fhandle(struct svc_rqst *rqstp, u32 *p, struct svc_fh *fhp)
{
	if (!(p = decode_fh(p, fhp)))
		return 0;
	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_sattrargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_sattrargs *args)
{
	if (!(p = decode_fh(p, &args->fh))
	 || !(p = decode_sattr3(p, &args->attrs)))
		return 0;

	if ((args->check_guard = ntohl(*p++)) != 0)
		p = decode_time3(p, &args->guardtime);

	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_diropargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_diropargs *args)
{
	if (!(p = decode_fh(p, &args->fh))
	 || !(p = decode_filename(p, &args->name, &args->len)))
		return 0;

	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_accessargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_accessargs *args)
{
	if (!(p = decode_fh(p, &args->fh)))
		return 0;
	args->access = ntohl(*p++);

	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_readargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_readargs *args)
{
	if (!(p = decode_fh(p, &args->fh))
	 || !(p = xdr_decode_hyper(p, &args->offset)))
		return 0;

	args->count = ntohl(*p++);
	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_writeargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_writeargs *args)
{
	if (!(p = decode_fh(p, &args->fh))
	 || !(p = xdr_decode_hyper(p, &args->offset)))
		return 0;

	args->count = ntohl(*p++);
	args->stable = ntohl(*p++);
	args->len = ntohl(*p++);
	args->data = (char *) p;
	p += XDR_QUADLEN(args->len);

	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_createargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_createargs *args)
{
	if (!(p = decode_fh(p, &args->fh))
	 || !(p = decode_filename(p, &args->name, &args->len)))
		return 0;

	switch (args->createmode = ntohl(*p++)) {
	case NFS3_CREATE_UNCHECKED:
	case NFS3_CREATE_GUARDED:
		if (!(p = decode_sattr3(p, &args->attrs)))
			return 0;
		break;
	case NFS3_CREATE_EXCLUSIVE:
		args->verf = p;
		p += 2;
		break;
	default:
		return 0;
	}

	return xdr_argsize_check(rqstp, p);
}
int
nfs3svc_decode_mkdirargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_createargs *args)
{
	if (!(p = decode_fh(p, &args->fh))
	 || !(p = decode_filename(p, &args->name, &args->len))
	 || !(p = decode_sattr3(p, &args->attrs)))
		return 0;

	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_symlinkargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_symlinkargs *args)
{
	if (!(p = decode_fh(p, &args->ffh))
	 || !(p = decode_filename(p, &args->fname, &args->flen))
	 || !(p = decode_sattr3(p, &args->attrs))
	 || !(p = decode_pathname(p, &args->tname, &args->tlen)))
		return 0;

	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_mknodargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_mknodargs *args)
{
	if (!(p = decode_fh(p, &args->fh))
	 || !(p = decode_filename(p, &args->name, &args->len)))
		return 0;

	args->ftype = ntohl(*p++);

	if (args->ftype == NF3BLK  || args->ftype == NF3CHR
	 || args->ftype == NF3SOCK || args->ftype == NF3FIFO) {
		if (!(p = decode_sattr3(p, &args->attrs)))
			return 0;
	}

	if (args->ftype == NF3BLK || args->ftype == NF3CHR) {
		args->major = ntohl(*p++);
		args->minor = ntohl(*p++);
	}

	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_renameargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_renameargs *args)
{
	if (!(p = decode_fh(p, &args->ffh))
	 || !(p = decode_filename(p, &args->fname, &args->flen))
	 || !(p = decode_fh(p, &args->tfh))
	 || !(p = decode_filename(p, &args->tname, &args->tlen)))
		return 0;

	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_linkargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_linkargs *args)
{
	if (!(p = decode_fh(p, &args->ffh))
	 || !(p = decode_fh(p, &args->tfh))
	 || !(p = decode_filename(p, &args->tname, &args->tlen)))
		return 0;

	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_readdirargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_readdirargs *args)
{
	if (!(p = decode_fh(p, &args->fh)))
		return 0;
	p = xdr_decode_hyper(p, &args->cookie);
	args->verf   = p; p += 2;
	args->dircount = ~0;
	args->count  = ntohl(*p++);

	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_readdirplusargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_readdirargs *args)
{
	if (!(p = decode_fh(p, &args->fh)))
		return 0;
	p = xdr_decode_hyper(p, &args->cookie);
	args->verf     = p; p += 2;
	args->dircount = ntohl(*p++);
	args->count    = ntohl(*p++);

	return xdr_argsize_check(rqstp, p);
}

int
nfs3svc_decode_commitargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_commitargs *args)
{
	if (!(p = decode_fh(p, &args->fh)))
		return 0;
	p = xdr_decode_hyper(p, &args->offset);
	args->count = ntohl(*p++);

	return xdr_argsize_check(rqstp, p);
}

/*
 * XDR encode functions
 */
/*
 * There must be an encoding function for void results so svc_process
 * will work properly.
 */
int
nfs3svc_encode_voidres(struct svc_rqst *rqstp, u32 *p, void *dummy)
{
	return xdr_ressize_check(rqstp, p);
}

/* GETATTR */
int
nfs3svc_encode_attrstat(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_attrstat *resp)
{
	if (resp->status == 0)
		p = encode_fattr3(rqstp, p, &resp->fh);
	return xdr_ressize_check(rqstp, p);
}

/* SETATTR, REMOVE, RMDIR */
int
nfs3svc_encode_wccstat(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_attrstat *resp)
{
	p = encode_wcc_data(rqstp, p, &resp->fh);
	return xdr_ressize_check(rqstp, p);
}

/* LOOKUP */
int
nfs3svc_encode_diropres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_diropres *resp)
{
	if (resp->status == 0) {
		p = encode_fh(p, &resp->fh);
		p = encode_post_op_attr(rqstp, p, &resp->fh);
	}
	p = encode_post_op_attr(rqstp, p, &resp->dirfh);
	return xdr_ressize_check(rqstp, p);
}

/* ACCESS */
int
nfs3svc_encode_accessres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_accessres *resp)
{
	p = encode_post_op_attr(rqstp, p, &resp->fh);
	if (resp->status == 0)
		*p++ = htonl(resp->access);
	return xdr_ressize_check(rqstp, p);
}

/* READLINK */
int
nfs3svc_encode_readlinkres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_readlinkres *resp)
{
	p = encode_post_op_attr(rqstp, p, &resp->fh);
	if (resp->status == 0) {
		*p++ = htonl(resp->len);
		p += XDR_QUADLEN(resp->len);
	}
	return xdr_ressize_check(rqstp, p);
}

/* READ */
int
nfs3svc_encode_readres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_readres *resp)
{
	p = encode_post_op_attr(rqstp, p, &resp->fh);
	if (resp->status == 0) {
		*p++ = htonl(resp->count);
		*p++ = htonl(resp->eof);
		*p++ = htonl(resp->count);	/* xdr opaque count */
		p += XDR_QUADLEN(resp->count);
	}
	return xdr_ressize_check(rqstp, p);
}

/* WRITE */
int
nfs3svc_encode_writeres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_writeres *resp)
{
	p = encode_wcc_data(rqstp, p, &resp->fh);
	if (resp->status == 0) {
		*p++ = htonl(resp->count);
		*p++ = htonl(resp->committed);
		*p++ = htonl(nfssvc_boot.tv_sec);
		*p++ = htonl(nfssvc_boot.tv_usec);
	}
	return xdr_ressize_check(rqstp, p);
}

/* CREATE, MKDIR, SYMLINK, MKNOD */
int
nfs3svc_encode_createres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_diropres *resp)
{
	if (resp->status == 0) {
		*p++ = xdr_one;
		p = encode_fh(p, &resp->fh);
		p = encode_post_op_attr(rqstp, p, &resp->fh);
	}
	p = encode_wcc_data(rqstp, p, &resp->dirfh);
	return xdr_ressize_check(rqstp, p);
}

/* RENAME */
int
nfs3svc_encode_renameres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_renameres *resp)
{
	p = encode_wcc_data(rqstp, p, &resp->ffh);
	p = encode_wcc_data(rqstp, p, &resp->tfh);
	return xdr_ressize_check(rqstp, p);
}

/* LINK */
int
nfs3svc_encode_linkres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_linkres *resp)
{
	p = encode_post_op_attr(rqstp, p, &resp->fh);
	p = encode_wcc_data(rqstp, p, &resp->tfh);
	return xdr_ressize_check(rqstp, p);
}

/* READDIR */
int
nfs3svc_encode_readdirres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_readdirres *resp)
{
	p = encode_post_op_attr(rqstp, p, &resp->fh);
	if (resp->status == 0) {
		/* stupid readdir cookie */
		memcpy(p, resp->verf, 8); p += 2;
		p += XDR_QUADLEN(resp->count);
	}

	return xdr_ressize_check(rqstp, p);
}

/*
 * Encode a directory entry. This one works for both normal readdir
 * and readdirplus.
 * The normal readdir reply requires 2 (fileid) + 1 (stringlen)
 * + string + 2 (cookie) + 1 (next) words, i.e. 6 + strlen.
 * 
 * The readdirplus baggage is 1+21 words for post_op_attr, plus the
 * file handle.
 */

#define NFS3_ENTRY_BAGGAGE	(2 + 1 + 2 + 1)
#define NFS3_ENTRYPLUS_BAGGAGE	(1 + 21 + 1 + (NFS3_FHSIZE >> 2))
static int
encode_entry(struct readdir_cd *cd, const char *name,
	     int namlen, off_t offset, ino_t ino, unsigned int d_type, int plus)
{
	u32		*p = cd->buffer;
	int		buflen, slen, elen;

	if (cd->offset)
		xdr_encode_hyper(cd->offset, (u64) offset);

	/* nfsd_readdir calls us with name == 0 when it wants us to
	 * set the last offset entry. */
	if (name == 0)
		return 0;

	/*
	dprintk("encode_entry(%.*s @%ld%s)\n",
		namlen, name, (long) offset, plus? " plus" : "");
	 */

	/* truncate filename if too long */
	if (namlen > NFS3_MAXNAMLEN)
		namlen = NFS3_MAXNAMLEN;

	slen = XDR_QUADLEN(namlen);
	elen = slen + NFS3_ENTRY_BAGGAGE
		+ (plus? NFS3_ENTRYPLUS_BAGGAGE : 0);
	if ((buflen = cd->buflen - elen) < 0) {
		cd->eob = 1;
		return -EINVAL;
	}
	*p++ = xdr_one;				 /* mark entry present */
	p    = xdr_encode_hyper(p, ino);	 /* file id */
	p    = xdr_encode_array(p, name, namlen);/* name length & name */

	cd->offset = p;			/* remember pointer */
	p = xdr_encode_hyper(p, NFS_OFFSET_MAX);	/* offset of next entry */

	/* throw in readdirplus baggage */
	if (plus) {
		struct svc_fh	fh;
		struct svc_export	*exp;
		struct dentry		*dparent, *dchild;

		dparent = cd->dirfh->fh_dentry;
		exp  = cd->dirfh->fh_export;

		fh_init(&fh, NFS3_FHSIZE);
		if (isdotent(name, namlen)) {
			dchild = dparent;
			if (namlen == 2)
				dchild = dchild->d_parent;
			dchild = dget(dchild);
		} else
			dchild = lookup_one_len(name, dparent,namlen);
		if (IS_ERR(dchild))
			goto noexec;
		if (fh_compose(&fh, exp, dchild, cd->dirfh) != 0 || !dchild->d_inode)
			goto noexec;
		p = encode_post_op_attr(cd->rqstp, p, &fh);
		*p++ = xdr_one; /* yes, a file handle follows */
		p = encode_fh(p, &fh);
		fh_put(&fh);
	}

out:
	cd->buflen = buflen;
	cd->buffer = p;
	return 0;

noexec:
	*p++ = 0;
	*p++ = 0;
	goto out;
}

int
nfs3svc_encode_entry(struct readdir_cd *cd, const char *name,
		     int namlen, loff_t offset, ino_t ino, unsigned int d_type)
{
	return encode_entry(cd, name, namlen, offset, ino, d_type, 0);
}

int
nfs3svc_encode_entry_plus(struct readdir_cd *cd, const char *name,
			  int namlen, loff_t offset, ino_t ino, unsigned int d_type)
{
	return encode_entry(cd, name, namlen, offset, ino, d_type, 1);
}

/* FSSTAT */
int
nfs3svc_encode_fsstatres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_fsstatres *resp)
{
	struct statfs	*s = &resp->stats;
	u64		bs = s->f_bsize;

	*p++ = xdr_zero;	/* no post_op_attr */

	if (resp->status == 0) {
		p = xdr_encode_hyper(p, bs * s->f_blocks);	/* total bytes */
		p = xdr_encode_hyper(p, bs * s->f_bfree);	/* free bytes */
		p = xdr_encode_hyper(p, bs * s->f_bavail);	/* user available bytes */
		p = xdr_encode_hyper(p, s->f_files);	/* total inodes */
		p = xdr_encode_hyper(p, s->f_ffree);	/* free inodes */
		p = xdr_encode_hyper(p, s->f_ffree);	/* user available inodes */
		*p++ = htonl(resp->invarsec);	/* mean unchanged time */
	}
	return xdr_ressize_check(rqstp, p);
}

/* FSINFO */
int
nfs3svc_encode_fsinfores(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_fsinfores *resp)
{
	*p++ = xdr_zero;	/* no post_op_attr */

	if (resp->status == 0) {
		*p++ = htonl(resp->f_rtmax);
		*p++ = htonl(resp->f_rtpref);
		*p++ = htonl(resp->f_rtmult);
		*p++ = htonl(resp->f_wtmax);
		*p++ = htonl(resp->f_wtpref);
		*p++ = htonl(resp->f_wtmult);
		*p++ = htonl(resp->f_dtpref);
		p = xdr_encode_hyper(p, resp->f_maxfilesize);
		*p++ = xdr_one;
		*p++ = xdr_zero;
		*p++ = htonl(resp->f_properties);
	}

	return xdr_ressize_check(rqstp, p);
}

/* PATHCONF */
int
nfs3svc_encode_pathconfres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_pathconfres *resp)
{
	*p++ = xdr_zero;	/* no post_op_attr */

	if (resp->status == 0) {
		*p++ = htonl(resp->p_link_max);
		*p++ = htonl(resp->p_name_max);
		*p++ = htonl(resp->p_no_trunc);
		*p++ = htonl(resp->p_chown_restricted);
		*p++ = htonl(resp->p_case_insensitive);
		*p++ = htonl(resp->p_case_preserving);
	}

	return xdr_ressize_check(rqstp, p);
}

/* COMMIT */
int
nfs3svc_encode_commitres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_commitres *resp)
{
	p = encode_wcc_data(rqstp, p, &resp->fh);
	/* Write verifier */
	if (resp->status == 0) {
		*p++ = htonl(nfssvc_boot.tv_sec);
		*p++ = htonl(nfssvc_boot.tv_usec);
	}
	return xdr_ressize_check(rqstp, p);
}

/*
 * XDR release functions
 */
int
nfs3svc_release_fhandle(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_attrstat *resp)
{
	fh_put(&resp->fh);
	return 1;
}

int
nfs3svc_release_fhandle2(struct svc_rqst *rqstp, u32 *p,
					struct nfsd3_fhandle_pair *resp)
{
	fh_put(&resp->fh1);
	fh_put(&resp->fh2);
	return 1;
}
