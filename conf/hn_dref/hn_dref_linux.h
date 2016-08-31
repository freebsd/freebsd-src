/* $srcdir/conf/hn_dref/hn_dref_linux.h */
#define NFS_HN_DREF(dst, src) xstrlcpy((dst), (src), MAXHOSTNAMELEN)
