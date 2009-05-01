# $FreeBSD: src/sys/modules/mqueue/Makefile,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h

.include <bsd.kmod.mk>
