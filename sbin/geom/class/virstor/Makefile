# $FreeBSD: src/sbin/geom/class/virstor/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../misc ${.CURDIR}/../../../../sys/geom/virstor

CLASS=	virstor

SRCS+=	binstream.c
SRCS+=	g_virstor_md.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.lib.mk>
