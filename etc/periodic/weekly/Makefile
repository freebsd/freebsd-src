# $FreeBSD: src/etc/periodic/weekly/Makefile,v 1.11.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.include <bsd.own.mk>

FILES=	340.noid \
	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_LOCATE} != "no"
FILES+=	310.locate
.endif

.if ${MK_MAN} != "no"
FILES+=	320.whatis 330.catman
.endif

.if ${MK_PKGTOOLS} != "no"
FILES+=	400.status-pkg
.endif

.include <bsd.prog.mk>
