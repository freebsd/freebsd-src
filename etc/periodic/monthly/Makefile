# $FreeBSD$

.include <bsd.own.mk>

FILES=	450.status-security \
	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_UTMPX} != "no"
FILES+=	200.accounting
.endif

.include <bsd.prog.mk>
