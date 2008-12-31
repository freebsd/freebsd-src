# $FreeBSD: src/sys/modules/pecoff/Makefile,v 1.7.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../compat/pecoff

KMOD=	pecoff
SRCS=	imgact_pecoff.c opt_pecoff.h vnode_if.h

CFLAGS+= -DDEBUG

.include <bsd.kmod.mk>
