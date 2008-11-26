# $FreeBSD: src/sys/modules/coda/Makefile,v 1.16.12.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../coda

KMOD=	coda
SRCS=	vnode_if.h \
	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c \
	opt_coda.h

.include <bsd.kmod.mk>
