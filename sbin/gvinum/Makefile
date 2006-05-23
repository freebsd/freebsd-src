# $FreeBSD: src/sbin/gvinum/Makefile,v 1.1.2.1 2005/02/13 07:23:19 obrien Exp $

PROG=	gvinum
SRCS=	gvinum.c gvinum.h geom_vinum_share.c
NOMAN=	# not yet

CFLAGS+=	-I${.CURDIR}/../../sys

DPADD=	${LIBREADLINE} ${LIBTERMCAP} ${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM}
LDADD=	-lreadline -ltermcap -ldevstat -lkvm -lgeom

.PATH: ${.CURDIR}/../../sys/geom/vinum

.include <bsd.prog.mk>
