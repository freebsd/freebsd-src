#	@(#)Makefile	8.1 (Berkeley) 6/9/93
# $FreeBSD: src/sbin/md5/Makefile,v 1.2.6.2 2004/06/25 16:35:12 eik Exp $

PROG=	md5

LINKS=	${BINDIR}/md5 ${BINDIR}/rmd160 \
	${BINDIR}/md5 ${BINDIR}/sha1

MLINKS=	md5.1 rmd160.1 \
	md5.1 sha1.1

WARNS?=	2
WFORMAT?=	1

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
