# $FreeBSD: src/etc/periodic/monthly/Makefile,v 1.3.24.1.4.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

FILES=	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_ACCT} != "no"
FILES+=	200.accounting
.endif

.include <bsd.prog.mk>
