# $FreeBSD$

MAINTAINER=	takawata@FreeBSD.org

.PATH: ${.CURDIR}/../../compat/pecoff

KMOD=	pecoff
SRCS=	imgact_pecoff.c opt_pecoff.h vnode_if.h

CFLAGS+= -DDEBUG

.include <bsd.kmod.mk>
