# $FreeBSD: src/usr.bin/chkey/Makefile,v 1.7 2002/02/08 22:31:36 markm Exp $

.PATH: ${.CURDIR}/../newkey

PROG=	chkey
SRCS=	chkey.c generic.c update.c
CFLAGS+=-DYP -I${.CURDIR}/../newkey
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
