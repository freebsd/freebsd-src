# $FreeBSD: src/sys/modules/mqueue/Makefile,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h

.include <bsd.kmod.mk>
