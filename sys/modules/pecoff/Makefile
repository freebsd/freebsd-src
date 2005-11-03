# $FreeBSD: src/sys/modules/pecoff/Makefile,v 1.7 2005/06/03 15:35:36 imp Exp $

.PATH: ${.CURDIR}/../../compat/pecoff

KMOD=	pecoff
SRCS=	imgact_pecoff.c opt_pecoff.h vnode_if.h

CFLAGS+= -DDEBUG

.include <bsd.kmod.mk>
