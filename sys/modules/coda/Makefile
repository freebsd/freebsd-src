#	$Id: Makefile,v 1.1.1.1 1998/09/25 17:47:48 rvb Exp $

.PATH:	${.CURDIR}/../../sys/coda
KMOD=	coda_mod
SRCS=	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c
NOMAN=
VFS_LKM=

.include <bsd.kmod.mk>
