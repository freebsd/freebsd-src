# $FreeBSD: src/usr.bin/chkey/Makefile,v 1.2.2.1 1999/08/29 15:26:10 peter Exp $

PROG=	chkey
SRCS=	chkey.c update.c generic.c

.PATH: ${.CURDIR}/../newkey

CFLAGS+=-DYP

DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBGMP}
LDADD=	-lrpcsvc -lmp -lgmp

.include <bsd.prog.mk>
