#ifndef _LINUX_NFS_XDR_H
#define _LINUX_NFS_XDR_H

struct nfs_fattr {
	unsigned short		valid;		/* which fields are valid */
	__u64			pre_size;	/* pre_op_attr.size	  */
	__u64			pre_mtime;	/* pre_op_attr.mtime	  */
	__u64			pre_ctime;	/* pre_op_attr.ctime	  */
	enum nfs_ftype		type;		/* always use NFSv2 types */
	__u32			mode;
	__u32			nlink;
	__u32			uid;
	__u32			gid;
	__u64			size;
	union {
		struct {
			__u32	blocksize;
			__u32	blocks;
		} nfs2;
		struct {
			__u64	used;
		} nfs3;
	} du;
	__u32			rdev;
	__u64			fsid;
	__u64			fileid;
	__u64			atime;
	__u64			mtime;
	__u64			ctime;
};

#define NFS_ATTR_WCC		0x0001		/* pre-op WCC data    */
#define NFS_ATTR_FATTR		0x0002		/* post-op attributes */
#define NFS_ATTR_FATTR_V3	0x0004		/* NFSv3 attributes */

/*
 * Info on the file system
 */
struct nfs_fsinfo {
	__u32			rtmax;	/* max.  read transfer size */
	__u32			rtpref;	/* pref. read transfer size */
	__u32			rtmult;	/* reads should be multiple of this */
	__u32			wtmax;	/* max.  write transfer size */
	__u32			wtpref;	/* pref. write transfer size */
	__u32			wtmult;	/* writes should be multiple of this */
	__u32			dtpref;	/* pref. readdir transfer size */
	__u64			maxfilesize;
	__u64			bsize;	/* block size */
	__u64			tbytes;	/* total size in bytes */
	__u64			fbytes;	/* # of free bytes */
	__u64			abytes;	/* # of bytes available to user */
	__u64			tfiles;	/* # of files */
	__u64			ffiles;	/* # of free files */
	__u64			afiles;	/* # of files available to user */
	__u32			linkmax;/* max # of hard links */
	__u32			namelen;/* max name length */
};

/* Arguments to the read call.
 * Note that NFS_READ_MAXIOV must be <= (MAX_IOVEC-2) from sunrpc/xprt.h
 */
#define NFS_READ_MAXIOV		(9)

struct nfs_readargs {
	struct nfs_fh *		fh;
	__u64			offset;
	__u32			count;
	unsigned int		pgbase;
	struct page **		pages;
};

struct nfs_readres {
	struct nfs_fattr *	fattr;
	__u32			count;
	int                     eof;
};

/* Arguments to the write call.
 * Note that NFS_WRITE_MAXIOV must be <= (MAX_IOVEC-2) from sunrpc/xprt.h
 */
#define NFS_WRITE_MAXIOV	(9)
struct nfs_writeargs {
	struct nfs_fh *		fh;
	__u64			offset;
	__u32			count;
	enum nfs3_stable_how	stable;
	unsigned int		pgbase;
	struct page **		pages;
};

struct nfs_writeverf {
	enum nfs3_stable_how	committed;
	__u32			verifier[2];
};

struct nfs_writeres {
	struct nfs_fattr *	fattr;
	struct nfs_writeverf *	verf;
	__u32			count;
};

/*
 * Argument struct for decode_entry function
 */
struct nfs_entry {
	__u64			ino;
	__u64			cookie,
				prev_cookie;
	const char *		name;
	unsigned int		len;
	int			eof;
};

/*
 * The following types are for NFSv2 only.
 */
struct nfs_sattrargs {
	struct nfs_fh *		fh;
	struct iattr *		sattr;
};

struct nfs_diropargs {
	struct nfs_fh *		fh;
	const char *		name;
	unsigned int		len;
};

struct nfs_createargs {
	struct nfs_fh *		fh;
	const char *		name;
	unsigned int		len;
	struct iattr *		sattr;
};

struct nfs_renameargs {
	struct nfs_fh *		fromfh;
	const char *		fromname;
	unsigned int		fromlen;
	struct nfs_fh *		tofh;
	const char *		toname;
	unsigned int		tolen;
};

struct nfs_linkargs {
	struct nfs_fh *		fromfh;
	struct nfs_fh *		tofh;
	const char *		toname;
	unsigned int		tolen;
};

struct nfs_symlinkargs {
	struct nfs_fh *		fromfh;
	const char *		fromname;
	unsigned int		fromlen;
	const char *		topath;
	unsigned int		tolen;
	struct iattr *		sattr;
};

struct nfs_readdirargs {
	struct nfs_fh *		fh;
	__u32			cookie;
	unsigned int		count;
	struct page **		pages;
};

struct nfs_diropok {
	struct nfs_fh *		fh;
	struct nfs_fattr *	fattr;
};

struct nfs_readlinkargs {
	struct nfs_fh *		fh;
	unsigned int		count;
	struct page **		pages;
};

struct nfs3_sattrargs {
	struct nfs_fh *		fh;
	struct iattr *		sattr;
	unsigned int		guard;
	__u64			guardtime;
};

struct nfs3_diropargs {
	struct nfs_fh *		fh;
	const char *		name;
	unsigned int		len;
};

struct nfs3_accessargs {
	struct nfs_fh *		fh;
	__u32			access;
};

struct nfs3_createargs {
	struct nfs_fh *		fh;
	const char *		name;
	unsigned int		len;
	struct iattr *		sattr;
	enum nfs3_createmode	createmode;
	__u32			verifier[2];
};

struct nfs3_mkdirargs {
	struct nfs_fh *		fh;
	const char *		name;
	unsigned int		len;
	struct iattr *		sattr;
};

struct nfs3_symlinkargs {
	struct nfs_fh *		fromfh;
	const char *		fromname;
	unsigned int		fromlen;
	const char *		topath;
	unsigned int		tolen;
	struct iattr *		sattr;
};

struct nfs3_mknodargs {
	struct nfs_fh *		fh;
	const char *		name;
	unsigned int		len;
	enum nfs3_ftype		type;
	struct iattr *		sattr;
	dev_t			rdev;
};

struct nfs3_renameargs {
	struct nfs_fh *		fromfh;
	const char *		fromname;
	unsigned int		fromlen;
	struct nfs_fh *		tofh;
	const char *		toname;
	unsigned int		tolen;
};

struct nfs3_linkargs {
	struct nfs_fh *		fromfh;
	struct nfs_fh *		tofh;
	const char *		toname;
	unsigned int		tolen;
};

struct nfs3_readdirargs {
	struct nfs_fh *		fh;
	__u64			cookie;
	__u32			verf[2];
	int			plus;
	unsigned int            count;
	struct page **		pages;
};

struct nfs3_diropres {
	struct nfs_fattr *	dir_attr;
	struct nfs_fh *		fh;
	struct nfs_fattr *	fattr;
};

struct nfs3_accessres {
	struct nfs_fattr *	fattr;
	__u32			access;
};

struct nfs3_readlinkargs {
	struct nfs_fh *		fh;
	unsigned int		count;
	struct page **		pages;
};

struct nfs3_renameres {
	struct nfs_fattr *	fromattr;
	struct nfs_fattr *	toattr;
};

struct nfs3_linkres {
	struct nfs_fattr *	dir_attr;
	struct nfs_fattr *	fattr;
};

struct nfs3_readdirres {
	struct nfs_fattr *	dir_attr;
	__u32 *			verf;
	int			plus;
};

/*
 * RPC procedure vector for NFSv2/NFSv3 demuxing
 */
struct nfs_rpc_ops {
	int	version;		/* Protocol version */

	int	(*getroot) (struct nfs_server *, struct nfs_fh *,
			    struct nfs_fattr *);
	int	(*getattr) (struct inode *, struct nfs_fattr *);
	int	(*setattr) (struct inode *, struct nfs_fattr *,
			    struct iattr *);
	int	(*lookup)  (struct inode *, struct qstr *,
			    struct nfs_fh *, struct nfs_fattr *);
	int	(*access)  (struct inode *, int , int);
	int	(*readlink)(struct inode *, struct page *);
	int	(*read)    (struct inode *, struct rpc_cred *,
			    struct nfs_fattr *,
			    int, unsigned int, unsigned int,
			    struct page *, int *eofp);
	int	(*write)   (struct inode *, struct rpc_cred *,
			    struct nfs_fattr *,
			    int, unsigned int, unsigned int,
			    struct page *, struct nfs_writeverf *verfp);
	int	(*commit)  (struct inode *, struct nfs_fattr *,
			    unsigned long, unsigned int);
	int	(*create)  (struct inode *, struct qstr *, struct iattr *,
			    int, struct nfs_fh *, struct nfs_fattr *);
	int	(*remove)  (struct inode *, struct qstr *);
	int	(*unlink_setup)  (struct rpc_message *,
			    struct dentry *, struct qstr *);
	void	(*unlink_done) (struct dentry *, struct rpc_message *);
	int	(*rename)  (struct inode *, struct qstr *,
			    struct inode *, struct qstr *);
	int	(*link)    (struct inode *, struct inode *, struct qstr *);
	int	(*symlink) (struct inode *, struct qstr *, struct qstr *,
			    struct iattr *, struct nfs_fh *,
			    struct nfs_fattr *);
	int	(*mkdir)   (struct inode *, struct qstr *, struct iattr *,
			    struct nfs_fh *, struct nfs_fattr *);
	int	(*rmdir)   (struct inode *, struct qstr *);
	int	(*readdir) (struct inode *, struct rpc_cred *,
			    u64, struct page *, unsigned int, int);
	int	(*mknod)   (struct inode *, struct qstr *, struct iattr *,
			    dev_t, struct nfs_fh *, struct nfs_fattr *);
	int	(*statfs)  (struct nfs_server *, struct nfs_fh *,
			    struct nfs_fsinfo *);
	u32 *	(*decode_dirent)(u32 *, struct nfs_entry *, int plus);
};

/*
 * 	NFS_CALL(getattr, inode, (fattr));
 * into
 *	NFS_PROTO(inode)->getattr(fattr);
 */
#define NFS_CALL(op, inode, args)	NFS_PROTO(inode)->op args

/*
 * Function vectors etc. for the NFS client
 */
extern struct nfs_rpc_ops	nfs_v2_clientops;
extern struct nfs_rpc_ops	nfs_v3_clientops;
extern struct rpc_version	nfs_version2;
extern struct rpc_version	nfs_version3;
extern struct rpc_program	nfs_program;
extern struct rpc_stat		nfs_rpcstat;

#endif
