# $FreeBSD$

PROG=	chkey
SRCS=	chkey.c update.c generic.c

.PATH: ${.CURDIR}/../newkey

CFLAGS+=-DYP

DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBGMP}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
