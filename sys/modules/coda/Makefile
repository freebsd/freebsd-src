# $FreeBSD: src/sys/modules/coda/Makefile,v 1.17.2.1.6.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/coda

KMOD=	coda
SRCS=	vnode_if.h \
	coda_fbsd.c coda_psdev.c coda_subr.c coda_venus.c coda_vfsops.c \
	coda_vnops.c opt_coda.h

.include <bsd.kmod.mk>
