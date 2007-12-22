/* $srcdir/conf/sa_dref/sa_dref_bsd44.h */
#define	NFS_SA_DREF(dst, src) { \
		(dst)->addr = (struct sockaddr *) (src); \
		(dst)->addrlen = sizeof(*src); \
	}
