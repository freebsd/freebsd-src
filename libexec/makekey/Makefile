#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/makekey/Makefile,v 1.13 2001/03/26 14:22:05 ru Exp $

PROG=	makekey
MAN=	makekey.8

DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt

.include <bsd.prog.mk>
