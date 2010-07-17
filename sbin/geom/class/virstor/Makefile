# $FreeBSD: src/sbin/geom/class/virstor/Makefile,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../../misc ${.CURDIR}/../../../../sys/geom/virstor

CLASS=	virstor

SRCS+=	binstream.c
SRCS+=	g_virstor_md.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.lib.mk>
