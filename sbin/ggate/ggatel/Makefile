# $FreeBSD$

.PATH:	../shared

BINDIR=	/sbin
PROG=	ggatel
SRCS=	ggatel.c ggate.c
MAN=	ggatel.8
DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML}
LDADD=	-lgeom -lsbuf -lbsdxml
WARNS=	6
CFLAGS+=-I../shared
CFLAGS+=-DLIBGEOM

.include <bsd.prog.mk>
