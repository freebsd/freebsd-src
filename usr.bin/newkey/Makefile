# $FreeBSD$

PROG=	newkey
SRCS=	newkey.c generic.c update.c
WARNS?=	2
MAN=	newkey.8
CFLAGS+= -DYP
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
