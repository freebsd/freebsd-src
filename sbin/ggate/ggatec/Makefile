# $FreeBSD$

.PATH:	${.CURDIR}/../shared

BINDIR=	/sbin
PROG=	ggatec
SRCS=	ggatec.c ggate.c
MAN=	ggatec.8
DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML}
LDADD=	-lgeom -lsbuf -lbsdxml
WARNS=	6
CFLAGS+=-I${.CURDIR}/../shared
CFLAGS+=-DLIBGEOM

.include <bsd.prog.mk>
