# $FreeBSD: src/sys/modules/null/Makefile,v 1.3 2003/11/18 00:52:30 rwatson Exp $

.PATH: ${.CURDIR}/../../dev/null

KMOD=	null
SRCS=	null.c
SRCS+=	bus_if.h device_if.h vnode_if.h

.include <bsd.kmod.mk>
