# $FreeBSD: src/sbin/ggate/ggatel/Makefile,v 1.4.26.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../shared

PROG=	ggatel
MAN=	ggatel.8
SRCS=	ggatel.c ggate.c

CFLAGS+= -DLIBGEOM
CFLAGS+= -I${.CURDIR}/../shared

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL}
LDADD=	-lgeom -lsbuf -lbsdxml -lutil

.include <bsd.prog.mk>
