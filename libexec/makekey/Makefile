#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/makekey/Makefile,v 1.8.2.1 1999/08/29 15:03:26 peter Exp $

PROG=	makekey
MAN8=	makekey.8

DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt

.include <bsd.prog.mk>
