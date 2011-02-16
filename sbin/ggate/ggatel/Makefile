# $FreeBSD: src/sbin/ggate/ggatel/Makefile,v 1.4.30.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../shared

PROG=	ggatel
MAN=	ggatel.8
SRCS=	ggatel.c ggate.c

CFLAGS+= -DLIBGEOM
CFLAGS+= -I${.CURDIR}/../shared

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL}
LDADD=	-lgeom -lsbuf -lbsdxml -lutil

.include <bsd.prog.mk>
