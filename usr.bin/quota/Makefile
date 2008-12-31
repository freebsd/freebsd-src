#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/quota/Makefile,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	quota
BINOWN=	root

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
