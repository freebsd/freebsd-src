# $FreeBSD: src/sbin/gvinum/Makefile,v 1.1 2004/06/12 21:22:47 le Exp $

PROG=	gvinum
SRCS=	gvinum.c gvinum.h geom_vinum_share.c
NOMAN=	notyet

CFLAGS+=	-I${.CURDIR}/../../sys

DPADD=	${LIBREADLINE} ${LIBTERMCAP} ${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM}
LDADD=	-lreadline -ltermcap -ldevstat -lkvm -lgeom

.PATH: ${.CURDIR}/../../sys/geom/vinum

.include <bsd.prog.mk>
