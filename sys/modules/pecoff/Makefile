# $FreeBSD: src/sys/modules/pecoff/Makefile,v 1.7.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../compat/pecoff

KMOD=	pecoff
SRCS=	imgact_pecoff.c opt_pecoff.h vnode_if.h

CFLAGS+= -DDEBUG

.include <bsd.kmod.mk>
