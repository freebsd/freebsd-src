#	@(#)Makefile	8.1 (Berkeley) 6/9/93
# $FreeBSD: src/sbin/md5/Makefile,v 1.10.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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
