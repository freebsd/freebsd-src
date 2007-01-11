# $FreeBSD: src/sbin/ggate/ggatec/Makefile,v 1.6 2005/07/10 21:10:20 pjd Exp $

.PATH: ${.CURDIR}/../shared

PROG=	ggatec
MAN=	ggatec.8
SRCS=	ggatec.c ggate.c

CFLAGS+= -DLIBGEOM
CFLAGS+= -I${.CURDIR}/../shared

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL} ${LIBPTHREAD}
LDADD=	-lgeom -lsbuf -lbsdxml -lutil -lpthread

.include <bsd.prog.mk>
