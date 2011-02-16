# $FreeBSD: src/sbin/ggate/ggatec/Makefile,v 1.7.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../shared

PROG=	ggatec
MAN=	ggatec.8
SRCS=	ggatec.c ggate.c

CFLAGS+= -DMAX_SEND_SIZE=32768
CFLAGS+= -DLIBGEOM
CFLAGS+= -I${.CURDIR}/../shared

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL} ${LIBPTHREAD}
LDADD=	-lgeom -lsbuf -lbsdxml -lutil -lpthread

.include <bsd.prog.mk>
