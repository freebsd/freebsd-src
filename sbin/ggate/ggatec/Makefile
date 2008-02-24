# $FreeBSD: src/sbin/ggate/ggatec/Makefile,v 1.7 2007/04/06 11:19:48 pjd Exp $

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
