# $FreeBSD$

.PATH: ${.CURDIR}/../newkey

PROG=	chkey
SRCS=	chkey.c generic.c update.c
CFLAGS+=-DYP -I${.CURDIR}/../newkey
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
