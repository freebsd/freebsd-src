/* $srcdir/conf/fh_dref/fh_dref_linux.h */
#define	NFS_FH_DREF(dst, src) memcpy((char *) &(dst.data), (char *) src, sizeof(struct nfs_fh))
