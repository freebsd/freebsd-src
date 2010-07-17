# $FreeBSD: src/sbin/gvinum/Makefile,v 1.4.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	gvinum
SRCS=	gvinum.c gvinum.h geom_vinum_share.c
MAN=	gvinum.8

CFLAGS+=	-I${.CURDIR}/../../sys

DPADD=	${LIBREADLINE} ${LIBTERMCAP} ${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM}
LDADD=	-lreadline -ltermcap -ldevstat -lkvm -lgeom

.PATH: ${.CURDIR}/../../sys/geom/vinum

.include <bsd.prog.mk>
