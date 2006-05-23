#	@(#)Makefile	8.1 (Berkeley) 6/9/93
# $FreeBSD: src/sbin/md5/Makefile,v 1.9.2.1 2005/06/24 13:32:25 cperciva Exp $

PROG=	md5

LINKS=	${BINDIR}/md5 ${BINDIR}/rmd160 \
	${BINDIR}/md5 ${BINDIR}/sha1 \
	${BINDIR}/md5 ${BINDIR}/sha256

MLINKS=	md5.1 rmd160.1 \
	md5.1 sha1.1 \
	md5.1 sha256.1

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
