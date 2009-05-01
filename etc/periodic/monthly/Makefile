# $FreeBSD: src/etc/periodic/monthly/Makefile,v 1.3.24.1.2.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

FILES=	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_ACCT} != "no"
FILES+=	200.accounting
.endif

.include <bsd.prog.mk>
