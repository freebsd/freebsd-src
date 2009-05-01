# $FreeBSD: src/sbin/gvinum/Makefile,v 1.4.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	gvinum
SRCS=	gvinum.c gvinum.h geom_vinum_share.c
MAN=	gvinum.8

CFLAGS+=	-I${.CURDIR}/../../sys

DPADD=	${LIBREADLINE} ${LIBTERMCAP} ${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM}
LDADD=	-lreadline -ltermcap -ldevstat -lkvm -lgeom

.PATH: ${.CURDIR}/../../sys/geom/vinum

.include <bsd.prog.mk>
