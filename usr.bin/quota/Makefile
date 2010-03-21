#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/quota/Makefile,v 1.6.12.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	quota
BINOWN=	root

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
