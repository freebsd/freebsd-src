# $FreeBSD: src/sys/modules/pecoff/Makefile,v 1.7.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../compat/pecoff

KMOD=	pecoff
SRCS=	imgact_pecoff.c opt_pecoff.h vnode_if.h

CFLAGS+= -DDEBUG

.include <bsd.kmod.mk>
