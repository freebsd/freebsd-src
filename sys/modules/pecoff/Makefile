# $FreeBSD$

MAINTAINER=	takawata@FreeBSD.org

.PATH: ${.CURDIR}/../../compat/pecoff

KMOD=	pecoff
SRCS=	imgact_pecoff.c opt_kstack_pages.h opt_pecoff.h vnode_if.h

CFLAGS+= -g -DDEBUG

.include <bsd.kmod.mk>
