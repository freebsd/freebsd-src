/* $srcdir/conf/sa_dref/sa_dref_linux.h */
#define	NFS_SA_DREF(dst, src) memmove((char *)&dst->addr, (char *) src, sizeof(struct sockaddr_in))
/* #undef NFS_ARGS_T_ADDR_IS_POINTER */
