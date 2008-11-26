#	@(#)Makefile	8.2 (Berkeley) 4/16/94
# $FreeBSD: src/share/man/Makefile,v 1.18.12.1 2008/10/02 02:57:24 kensmith Exp $

# XXX MISSING:	man3f
SUBDIR=	man1 man3 man4 man5 man6 man7 man8 man9

MAKEWHATIS?=	makewhatis

makedb:
	${MAKEWHATIS} ${DESTDIR}${BINDIR}/man
.if !defined(NO_CRYPT) && !defined(NO_OPENSSL)
	${MAKEWHATIS} ${DESTDIR}${BINDIR}/openssl/man
.endif

.include "${.CURDIR}/../Makefile.inc"
.include <bsd.subdir.mk>
