#	$Id: Makefile,v 1.6 1998/02/06 23:54:22 eivind Exp $

.PATH:	${.CURDIR}/../../sys/coda
KMOD=	coda_mod
SRCS=	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DCODA

.include <bsd.kmod.mk>
