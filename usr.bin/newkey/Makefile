# $FreeBSD: src/usr.bin/newkey/Makefile,v 1.3 1999/08/28 01:04:33 peter Exp $

PROG= newkey
SRCS= newkey.c update.c generic.c

MAN8= newkey.8

# Later
#CFLAGS+= -DYP

DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBGMP}
LDADD=	-lrpcsvc -lmp -lgmp

.include <bsd.prog.mk>
