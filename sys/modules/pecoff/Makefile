# $FreeBSD$

MAINTAINER=	takawata@FreeBSD.org

.PATH: ${.CURDIR}/../../compat/pecoff

KMOD=	pecoff
SRCS=	imgact_pecoff.c opt_pecoff.h vnode_if.h

CFLAGS+= -g -DDEBUG
#EXPORT_SYMS=_pecoff_mod

opt_pecoff.h: Makefile
	touch opt_pecoff.h

.include <bsd.kmod.mk>
