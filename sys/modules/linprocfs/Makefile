# $FreeBSD: src/sys/modules/linprocfs/Makefile,v 1.11.4.1 2004/08/29 06:15:44 tjr Exp $

.PATH: ${.CURDIR}/../../compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h \
	linprocfs.c \
	opt_compat.h

opt_compat.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
