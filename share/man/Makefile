#	@(#)Makefile	8.2 (Berkeley) 4/16/94
# $FreeBSD$

# XXX MISSING:	man3f
SUBDIR=	man1 man3 man4 man5 man6 man7 man8 man9

MAKEWHATIS?=	/usr/bin/makewhatis

makedb:
	${MAKEWHATIS} ${DESTDIR}${BINDIR}/man
	${MAKEWHATIS} ${DESTDIR}${BINDIR}/perl/man

.include "${.CURDIR}/../Makefile.inc"
.include <bsd.subdir.mk>
