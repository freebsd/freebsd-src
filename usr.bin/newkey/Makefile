# $FreeBSD$

PROG=	newkey
SRCS=	newkey.c generic.c update.c
MAN=	newkey.8
CFLAGS+= -DYP
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
