#	@(#)Makefile	8.2 (Berkeley) 4/16/94
# $FreeBSD$

# XXX MISSING:	man3f
SUBDIR=	man1 man3 man4 man5 man6 man7 man8 man9

makedb:
	makewhatis ${DESTDIR}${BINDIR}/man
	makewhatis ${DESTDIR}${BINDIR}/perl/man

.include "${.CURDIR}/../Makefile.inc"
.include <bsd.subdir.mk>
