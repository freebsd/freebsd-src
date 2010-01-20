# $FreeBSD$

.include <bsd.own.mk>

FILES=	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_ACCT} != "no"
FILES+=	200.accounting
.endif

.include <bsd.prog.mk>
