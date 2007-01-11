# $FreeBSD: src/sys/modules/linprocfs/Makefile,v 1.13.2.1 2006/06/15 17:57:50 ambrisko Exp $

.PATH: ${.CURDIR}/../../compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h \
	linprocfs.c \
	opt_compat.h

.include <bsd.kmod.mk>
