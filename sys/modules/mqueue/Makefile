# $FreeBSD: src/sys/modules/mqueue/Makefile,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h

.include <bsd.kmod.mk>
