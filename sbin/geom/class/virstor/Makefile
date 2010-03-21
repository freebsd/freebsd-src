# $FreeBSD: src/sbin/geom/class/virstor/Makefile,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/../../misc ${.CURDIR}/../../../../sys/geom/virstor

CLASS=	virstor

SRCS+=	binstream.c
SRCS+=	g_virstor_md.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.lib.mk>
