/* $srcdir/conf/fh_dref/fh_dref_aix3.h */
#define	NFS_FH_DREF(dst, src) memcpy((char *) &(dst.x), (char *) src, sizeof(struct nfs_fh))
