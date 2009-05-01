# $FreeBSD: src/sbin/geom/class/virstor/Makefile,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/../../misc ${.CURDIR}/../../../../sys/geom/virstor

CLASS=	virstor

SRCS+=	binstream.c
SRCS+=	g_virstor_md.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.lib.mk>
