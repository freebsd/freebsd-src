# $FreeBSD: src/etc/periodic/monthly/Makefile,v 1.4.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.include <bsd.own.mk>

FILES=	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_ACCT} != "no"
FILES+=	200.accounting
.endif

.include <bsd.prog.mk>
