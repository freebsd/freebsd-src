#	@(#)Makefile	8.2 (Berkeley) 4/16/94
# $FreeBSD$

# XXX MISSING:	man3f
SUBDIR=	man1 man3 man4 man5 man6 man7 man8 man9

MAKEWHATIS?=	makewhatis

makedb:
	${MAKEWHATIS} ${DESTDIR}${BINDIR}/man
.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
	${MAKEWHATIS} ${DESTDIR}${BINDIR}/openssl/man
.endif

.include "${.CURDIR}/../Makefile.inc"
.include <bsd.subdir.mk>
