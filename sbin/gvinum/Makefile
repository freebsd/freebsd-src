# $FreeBSD: src/sbin/gvinum/Makefile,v 1.4.10.2.2.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	gvinum
SRCS=	gvinum.c gvinum.h geom_vinum_share.c
MAN=	gvinum.8

CFLAGS=	-I${.CURDIR}/../../sys -I${DESTDIR}/${INCLUDEDIR}/edit

DPADD=	${LIBEDIT} ${LIBTERMCAP} ${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM}
LDADD=	-ledit -ltermcap -ldevstat -lkvm -lgeom

.PATH: ${.CURDIR}/../../sys/geom/vinum

.include <bsd.prog.mk>
