# $FreeBSD: src/usr.bin/newkey/Makefile,v 1.3.2.1 2001/04/25 11:29:30 ru Exp $

PROG= newkey
SRCS= newkey.c update.c generic.c
MAN= newkey.8

# Later
#CFLAGS+= -DYP

DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBGMP}
LDADD=	-lrpcsvc -lmp -lgmp

.include <bsd.prog.mk>
