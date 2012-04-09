# $FreeBSD: src/etc/periodic/monthly/Makefile,v 1.4.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>

FILES=	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_ACCT} != "no"
FILES+=	200.accounting
.endif

.include <bsd.prog.mk>
