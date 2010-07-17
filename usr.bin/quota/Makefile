#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/quota/Makefile,v 1.6.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	quota
BINOWN=	root

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
