# $FreeBSD: src/sys/modules/coda/Makefile,v 1.7 1999/12/12 16:47:23 peter Exp $

.PATH:	${.CURDIR}/../../coda
KMOD=	coda
SRCS=	vnode_if.h \
	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c opt_devfs.h vcoda.h
NOMAN=

CLEANFILES=	vcoda.h

vcoda.h:
	echo "#define NVCODA 4" > vcoda.h

.include <bsd.kmod.mk>
