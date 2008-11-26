# $FreeBSD: src/sys/modules/linprocfs/Makefile,v 1.13.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h \
	linprocfs.c \
	opt_compat.h

.include <bsd.kmod.mk>
