# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../newkey

PROG=	chkey
SRCS=	chkey.c generic.c update.c
CFLAGS+= -I${.CURDIR}/../newkey
.if ${MK_NIS} != "no"
CFLAGS+= -DYP
.endif
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
