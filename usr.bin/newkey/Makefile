# $FreeBSD$

PROG=	newkey
SRCS=	generic.c newkey.c update.c
MAN=	newkey.8
# For later:
#CFLAGS+= -DYP
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
