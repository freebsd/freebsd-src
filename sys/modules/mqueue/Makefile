# $FreeBSD: src/sys/modules/mqueue/Makefile,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h

.include <bsd.kmod.mk>
