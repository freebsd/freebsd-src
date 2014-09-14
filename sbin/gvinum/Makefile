# $FreeBSD$

PROG=	gvinum
SRCS=	gvinum.c gvinum.h geom_vinum_share.c
MAN=	gvinum.8

WARNS?=	2
CFLAGS+=	-I${.CURDIR}/../../sys -I${DESTDIR}/${INCLUDEDIR}/edit

DPADD=	${LIBEDIT} ${LIBTERMCAPW} ${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM}
LDADD=	-ledit -ltermcapw -ldevstat -lkvm -lgeom

.PATH: ${.CURDIR}/../../sys/geom/vinum

.include <bsd.prog.mk>
