#	$Id: Makefile,v 1.2 1998/09/29 20:19:45 rvb Exp $

.PATH:	${.CURDIR}/../../coda
KMOD=	coda
SRCS=	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c
NOMAN=
VFS_KLD=

.include <bsd.kmod.mk>
