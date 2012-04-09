# $FreeBSD: src/sys/modules/mqueue/Makefile,v 1.2.10.2.2.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h opt_compat.h

.include <bsd.kmod.mk>
