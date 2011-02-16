# $FreeBSD: src/sys/modules/coda5/Makefile,v 1.9.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../fs/coda

KMOD=	coda5
SRCS=	vnode_if.h \
	coda_fbsd.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c \
	opt_coda.h

CFLAGS+= -DCODA5_MODULE

.include <bsd.kmod.mk>
