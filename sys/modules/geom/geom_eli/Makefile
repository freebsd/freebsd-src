# $FreeBSD: src/sys/modules/geom/geom_eli/Makefile,v 1.4.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../../geom/eli

KMOD=	geom_eli
SRCS=	g_eli.c
SRCS+=	g_eli_crypto.c
SRCS+=	g_eli_ctl.c
SRCS+=	g_eli_integrity.c
SRCS+=	g_eli_key.c
SRCS+=	g_eli_privacy.c
SRCS+=	pkcs5v2.c
SRCS+=	vnode_if.h
WARNS?=	2

.include <bsd.kmod.mk>
