# $FreeBSD$

.PATH:	${.CURDIR}/../../compat/pecoff

MAINTAINER=	takawata@FreeBSD.org

KMOD=	pecoff
SRCS=	imgact_pecoff.c opt_pecoff.h vnode_if.h


CFLAGS+=	-g -DDEBUG
#EXPORT_SYMS=_pecoff_mod

opt_pecoff.h:Makefile
	touch opt_pecoff.h
.include <bsd.kmod.mk>
