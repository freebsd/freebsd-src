# $FreeBSD: src/usr.bin/chkey/Makefile,v 1.3 1999/08/28 00:59:35 peter Exp $

PROG=	chkey
SRCS=	chkey.c update.c generic.c

.PATH: ${.CURDIR}/../newkey

CFLAGS+=-DYP

DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBGMP}
LDADD=	-lrpcsvc -lmp -lgmp

.include <bsd.prog.mk>
