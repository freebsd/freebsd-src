# $FreeBSD: src/sys/modules/null/Makefile,v 1.1 2003/02/27 18:08:44 markm Exp $

.PATH: ${.CURDIR}/../../dev/null

KMOD=	null
SRCS=	null.c
SRCS+=	bus_if.h device_if.h vnode_if.h

.include <bsd.kmod.mk>
