#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/quota/Makefile,v 1.6.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	quota
BINOWN=	root

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
