# $FreeBSD$

PROG=	newkey
SRCS=	newkey.c generic.c update.c
CFLAGS+= -DYP
MAN=	newkey.8
DISTRIBUTION= crypto
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
