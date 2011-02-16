# $FreeBSD: src/sbin/geom/class/virstor/Makefile,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../../misc ${.CURDIR}/../../../../sys/geom/virstor

CLASS=	virstor

SRCS+=	binstream.c
SRCS+=	g_virstor_md.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.lib.mk>
