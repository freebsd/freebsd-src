/*
 * linux/fs/nfsd/xdr.c
 *
 * XDR support for nfsd
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/nfs.h>

#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/svc.h>
#include <linux/nfsd/nfsd.h>
#include <linux/nfsd/xdr.h>

#define NFSDDBG_FACILITY		NFSDDBG_XDR


#ifdef NFSD_OPTIMIZE_SPACE
# define inline
#endif

/*
 * Mapping of S_IF* types to NFS file types
 */
static u32	nfs_ftypes[] = {
	NFNON,  NFCHR,  NFCHR, NFBAD,
	NFDIR,  NFBAD,  NFBLK, NFBAD,
	NFREG,  NFBAD,  NFLNK, NFBAD,
	NFSOCK, NFBAD,  NFLNK, NFBAD,
};


/*
 * XDR functions for basic NFS types
 */
static inline u32 *
decode_fh(u32 *p, struct svc_fh *fhp)
{
	fh_init(fhp, NFS_FHSIZE);
	memcpy(&fhp->fh_handle.fh_base, p, NFS_FHSIZE);
	fhp->fh_handle.fh_size = NFS_FHSIZE;

	/* FIXME: Look up export pointer here and verify
	 * Sun Secure RPC if requested */
	return p + (NFS_FHSIZE >> 2);
}

static inline u32 *
encode_fh(u32 *p, struct svc_fh *fhp)
{
	memcpy(p, &fhp->fh_handle.fh_base, NFS_FHSIZE);
	return p + (NFS_FHSIZE>> 2);
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

	if ((p = xdr_decode_string_inplace(p, namp, lenp, NFS_MAXNAMLEN)) != NULL) {
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

	if ((p = xdr_decode_string(p, namp, lenp, NFS_MAXPATHLEN)) != NULL) {
		for (i = 0, name = *namp; i < *lenp; i++, name++) {
			if (*name == '\0')
				return NULL;
		}
	}

	return p;
}

static inline u32 *
decode_sattr(u32 *p, struct iattr *iap)
{
	u32	tmp, tmp1;

	iap->ia_valid = 0;

	/* Sun client bug compatibility check: some sun clients seem to
	 * put 0xffff in the mode field when they mean 0xffffffff.
	 * Quoting the 4.4BSD nfs server code: Nah nah nah nah na nah.
	 */
	if ((tmp = ntohl(*p++)) != (u32)-1 && tmp != 0xffff) {
		iap->ia_valid |= ATTR_MODE;
		iap->ia_mode = tmp;
	}
	if ((tmp = ntohl(*p++)) != (u32)-1) {
		iap->ia_valid |= ATTR_UID;
		iap->ia_uid = tmp;
	}
	if ((tmp = ntohl(*p++)) != (u32)-1) {
		iap->ia_valid |= ATTR_GID;
		iap->ia_gid = tmp;
	}
	if ((tmp = ntohl(*p++)) != (u32)-1) {
		iap->ia_valid |= ATTR_SIZE;
		iap->ia_size = tmp;
	}
	tmp  = ntohl(*p++); tmp1 = ntohl(*p++);
	if (tmp != (u32)-1 && tmp1 != (u32)-1) {
		iap->ia_valid |= ATTR_ATIME | ATTR_ATIME_SET;
		iap->ia_atime = tmp;
	}
	tmp  = ntohl(*p++); tmp1 = ntohl(*p++);
	if (tmp != (u32)-1 && tmp1 != (u32)-1) {
		iap->ia_valid |= ATTR_MTIME | ATTR_MTIME_SET;
		iap->ia_mtime = tmp;
		/*
		 * Passing the invalid value useconds=1000000 for mtime
		 * is a Sun convention for "set both mtime and atime to
		 * current server time".  It's needed to make permissions
		 * checks for the "touch" program across v2 mounts to
		 * Solaris and Irix boxes work correctly. See description of
		 * sattr in section 6.1 of "NFS Illustrated" by
		 * Brent Callaghan, Addison-Wesley, ISBN 0-201-32750-5
		 */
		if (tmp1 == 1000000)
			iap->ia_valid &= ~(ATTR_ATIME_SET|ATTR_MTIME_SET);
	}
	return p;
}

static inline u32 *
encode_fattr(struct svc_rqst *rqstp, u32 *p, struct svc_fh *fhp)
{
	struct inode *inode = fhp->fh_dentry->d_inode;
	int type = (inode->i_mode & S_IFMT);

	*p++ = htonl(nfs_ftypes[type >> 12]);
	*p++ = htonl((u32) inode->i_mode);
	*p++ = htonl((u32) inode->i_nlink);
	*p++ = htonl((u32) nfsd_ruid(rqstp, inode->i_uid));
	*p++ = htonl((u32) nfsd_rgid(rqstp, inode->i_gid));

	if (S_ISLNK(type) && inode->i_size > NFS_MAXPATHLEN) {
		*p++ = htonl(NFS_MAXPATHLEN);
	} else {
		*p++ = htonl((u32) inode->i_size);
	}
	*p++ = htonl((u32) inode->i_blksize);
	if (S_ISCHR(type) || S_ISBLK(type))
		*p++ = htonl((u32) inode->i_rdev);
	else
		*p++ = htonl(0xffffffff);
	*p++ = htonl((u32) inode->i_blocks);
	if (rqstp->rq_reffh->fh_version == 1 
	    && rqstp->rq_reffh->fh_fsid_type == 1
	    && (fhp->fh_export->ex_flags & NFSEXP_FSID))
		*p++ = htonl((u32) fhp->fh_export->ex_fsid);
	else
		*p++ = htonl((u32) inode->i_dev);
	*p++ = htonl((u32) inode->i_ino);
	*p++ = htonl((u32) inode->i_atime);
	*p++ = 0;
	*p++ = htonl((u32) lease_get_mtime(inode));
	*p++ = 0;
	*p++ = htonl((u32) inode->i_ctime);
	*p++ = 0;

	return p;
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
nfssvc_decode_void(struct svc_rqst *rqstp, u32 *p, void *dummy)
{
	return xdr_argsize_check(rqstp, p);
}

int
nfssvc_decode_fhandle(struct svc_rqst *rqstp, u32 *p, struct svc_fh *fhp)
{
	if (!(p = decode_fh(p, fhp)))
		return 0;
	return xdr_argsize_check(rqstp, p);
}

int
nfssvc_decode_sattrargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_sattrargs *args)
{
	if (!(p = decode_fh(p, &args->fh))
	 || !(p = decode_sattr(p, &args->attrs)))
		return 0;

	return xdr_argsize_check(rqstp, p);
}

int
nfssvc_decode_diropargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_diropargs *args)
{
	if (!(p = decode_fh(p, &args->fh))
	 || !(p = decode_filename(p, &args->name, &args->len)))
		return 0;

	 return xdr_argsize_check(rqstp, p);
}

int
nfssvc_decode_readargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_readargs *args)
{
	if (!(p = decode_fh(p, &args->fh)))
		return 0;

	args->offset    = ntohl(*p++);
	args->count     = ntohl(*p++);
	args->totalsize = ntohl(*p++);

	return xdr_argsize_check(rqstp, p);
}

int
nfssvc_decode_writeargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_writeargs *args)
{
	if (!(p = decode_fh(p, &args->fh)))
		return 0;

	p++;				/* beginoffset */
	args->offset = ntohl(*p++);	/* offset */
	p++;				/* totalcount */
	args->len = ntohl(*p++);
	args->data = (char *) p;
	p += XDR_QUADLEN(args->len);

	return xdr_argsize_check(rqstp, p);
}

int
nfssvc_decode_createargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_createargs *args)
{
	if (!(p = decode_fh(p, &args->fh))
	 || !(p = decode_filename(p, &args->name, &args->len))
	 || !(p = decode_sattr(p, &args->attrs)))
		return 0;

	return xdr_argsize_check(rqstp, p);
}

int
nfssvc_decode_renameargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_renameargs *args)
{
	if (!(p = decode_fh(p, &args->ffh))
	 || !(p = decode_filename(p, &args->fname, &args->flen))
	 || !(p = decode_fh(p, &args->tfh))
	 || !(p = decode_filename(p, &args->tname, &args->tlen)))
		return 0;

	return xdr_argsize_check(rqstp, p);
}

int
nfssvc_decode_linkargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_linkargs *args)
{
	if (!(p = decode_fh(p, &args->ffh))
	 || !(p = decode_fh(p, &args->tfh))
	 || !(p = decode_filename(p, &args->tname, &args->tlen)))
		return 0;

	return xdr_argsize_check(rqstp, p);
}

int
nfssvc_decode_symlinkargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_symlinkargs *args)
{
	if (!(p = decode_fh(p, &args->ffh))
	 || !(p = decode_filename(p, &args->fname, &args->flen))
	 || !(p = decode_pathname(p, &args->tname, &args->tlen))
	 || !(p = decode_sattr(p, &args->attrs)))
		return 0;

	return xdr_argsize_check(rqstp, p);
}

int
nfssvc_decode_readdirargs(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_readdirargs *args)
{
	if (!(p = decode_fh(p, &args->fh)))
		return 0;
	args->cookie = ntohl(*p++);
	args->count  = ntohl(*p++);

	return xdr_argsize_check(rqstp, p);
}

/*
 * XDR encode functions
 */
int
nfssvc_encode_void(struct svc_rqst *rqstp, u32 *p, void *dummy)
{
	return xdr_ressize_check(rqstp, p);
}

int
nfssvc_encode_attrstat(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_attrstat *resp)
{
	p = encode_fattr(rqstp, p, &resp->fh);
	return xdr_ressize_check(rqstp, p);
}

int
nfssvc_encode_diropres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_diropres *resp)
{
	p = encode_fh(p, &resp->fh);
	p = encode_fattr(rqstp, p, &resp->fh);
	return xdr_ressize_check(rqstp, p);
}

int
nfssvc_encode_readlinkres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_readlinkres *resp)
{
	*p++ = htonl(resp->len);
	p += XDR_QUADLEN(resp->len);
	return xdr_ressize_check(rqstp, p);
}

int
nfssvc_encode_readres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_readres *resp)
{
	p = encode_fattr(rqstp, p, &resp->fh);
	*p++ = htonl(resp->count);
	p += XDR_QUADLEN(resp->count);

	return xdr_ressize_check(rqstp, p);
}

int
nfssvc_encode_readdirres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_readdirres *resp)
{
	p += XDR_QUADLEN(resp->count);
	return xdr_ressize_check(rqstp, p);
}

int
nfssvc_encode_statfsres(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_statfsres *resp)
{
	struct statfs	*stat = &resp->stats;

	*p++ = htonl(NFSSVC_MAXBLKSIZE);	/* max transfer size */
	*p++ = htonl(stat->f_bsize);
	*p++ = htonl(stat->f_blocks);
	*p++ = htonl(stat->f_bfree);
	*p++ = htonl(stat->f_bavail);
	return xdr_ressize_check(rqstp, p);
}

int
nfssvc_encode_entry(struct readdir_cd *cd, const char *name,
		    int namlen, loff_t offset, ino_t ino, unsigned int d_type)
{
	u32	*p = cd->buffer;
	int	buflen, slen;

	/*
	dprintk("nfsd: entry(%.*s off %ld ino %ld)\n",
			namlen, name, offset, ino);
	 */

	if (offset > ~((u32) 0))
		return -EINVAL;
	if (cd->offset)
		*cd->offset = htonl(offset);
	if (namlen > NFS2_MAXNAMLEN)
		namlen = NFS2_MAXNAMLEN;/* truncate filename */

	slen = XDR_QUADLEN(namlen);
	if ((buflen = cd->buflen - slen - 4) < 0) {
		cd->eob = 1;
		return -EINVAL;
	}
	*p++ = xdr_one;				/* mark entry present */
	*p++ = htonl((u32) ino);		/* file id */
	p    = xdr_encode_array(p, name, namlen);/* name length & name */
	cd->offset = p;			/* remember pointer */
	*p++ = ~(u32) 0;		/* offset of next entry */

	cd->buflen = buflen;
	cd->buffer = p;
	return 0;
}

/*
 * XDR release functions
 */
int
nfssvc_release_fhandle(struct svc_rqst *rqstp, u32 *p,
					struct nfsd_fhandle *resp)
{
	fh_put(&resp->fh);
	return 1;
}
