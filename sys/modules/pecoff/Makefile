# $FreeBSD: src/sys/modules/pecoff/Makefile,v 1.5 2002/09/08 02:59:38 peter Exp $

MAINTAINER=	takawata@FreeBSD.org

.PATH: ${.CURDIR}/../../compat/pecoff

KMOD=	pecoff
SRCS=	imgact_pecoff.c opt_pecoff.h vnode_if.h

CFLAGS+= -g -DDEBUG

.include <bsd.kmod.mk>
