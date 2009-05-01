#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/quota/Makefile,v 1.6.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	quota
BINOWN=	root

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
