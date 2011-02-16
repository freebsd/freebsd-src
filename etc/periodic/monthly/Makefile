# $FreeBSD: src/etc/periodic/monthly/Makefile,v 1.4.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.include <bsd.own.mk>

FILES=	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_ACCT} != "no"
FILES+=	200.accounting
.endif

.include <bsd.prog.mk>
