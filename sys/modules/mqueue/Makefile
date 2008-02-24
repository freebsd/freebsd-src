# $FreeBSD: src/sys/modules/mqueue/Makefile,v 1.2 2005/11/27 08:30:12 davidxu Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h

.include <bsd.kmod.mk>
