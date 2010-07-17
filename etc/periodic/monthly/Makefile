# $FreeBSD: src/etc/periodic/monthly/Makefile,v 1.4.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

FILES=	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_ACCT} != "no"
FILES+=	200.accounting
.endif

.include <bsd.prog.mk>
