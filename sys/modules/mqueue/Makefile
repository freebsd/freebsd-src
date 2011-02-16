# $FreeBSD: src/sys/modules/mqueue/Makefile,v 1.2.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h

.include <bsd.kmod.mk>
