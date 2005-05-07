# $FreeBSD: src/sys/modules/pecoff/Makefile,v 1.6 2004/02/13 10:11:01 ru Exp $

MAINTAINER=	takawata@FreeBSD.org

.PATH: ${.CURDIR}/../../compat/pecoff

KMOD=	pecoff
SRCS=	imgact_pecoff.c opt_pecoff.h vnode_if.h

CFLAGS+= -DDEBUG

.include <bsd.kmod.mk>
