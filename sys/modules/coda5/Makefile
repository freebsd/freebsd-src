# $FreeBSD: src/sys/modules/coda5/Makefile,v 1.8.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/coda

KMOD=	coda5
SRCS=	vnode_if.h \
	coda_fbsd.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c \
	opt_coda.h

CFLAGS+= -DCODA5_MODULE

.include <bsd.kmod.mk>
