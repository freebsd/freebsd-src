# $FreeBSD$

.PATH: ${.CURDIR}/../newkey

PROG=	chkey
SRCS=	chkey.c generic.c update.c
CFLAGS+=-DYP
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBGMP}
LDADD=	-lrpcsvc -lmp -lgmp

.include <bsd.prog.mk>
