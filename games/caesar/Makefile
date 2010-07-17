#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/caesar/Makefile,v 1.10.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	caesar
DPADD=	${LIBM}
LDADD=	-lm
SCRIPTS=rot13.sh
MAN=	caesar.6
MLINKS=	caesar.6 rot13.6

WARNS?=	6

.include <bsd.prog.mk>
