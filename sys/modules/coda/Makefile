#	$Id: Makefile,v 1.3 1998/10/16 04:30:42 peter Exp $

.PATH:	${.CURDIR}/../../coda
KMOD=	coda
SRCS=	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c
NOMAN=
VFS_KLD=

.include <bsd.kmod.mk>
