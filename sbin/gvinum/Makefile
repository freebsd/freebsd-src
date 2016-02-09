# $FreeBSD$

PACKAGE=runtime
PROG=	gvinum
SRCS=	gvinum.c gvinum.h geom_vinum_share.c
MAN=	gvinum.8

WARNS?=	2
CFLAGS+=	-I${.CURDIR}/../../sys -I${DESTDIR}/${INCLUDEDIR}/edit

LIBADD=	edit geom

.PATH: ${.CURDIR}/../../sys/geom/vinum

.include <bsd.prog.mk>
