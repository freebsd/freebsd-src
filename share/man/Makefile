#	@(#)Makefile	8.2 (Berkeley) 4/16/94
# $FreeBSD: src/share/man/Makefile,v 1.15 2002/05/27 15:52:40 ru Exp $

# XXX MISSING:	man3f
SUBDIR=	man1 man3 man4 man5 man6 man7 man8 man9

MAKEWHATIS?=	makewhatis

makedb:
	${MAKEWHATIS} ${DESTDIR}${BINDIR}/man

.include "${.CURDIR}/../Makefile.inc"
.include <bsd.subdir.mk>
