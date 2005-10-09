# $FreeBSD$

PROG=	newkey
SRCS=	newkey.c generic.c update.c
.if !defined(NO_NIS)
CFLAGS+= -DYP
.endif
MAN=	newkey.8
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
