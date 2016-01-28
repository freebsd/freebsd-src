# $FreeBSD$

.include <src.opts.mk>

FILESGROUPS=FILES

FILES=	450.status-security \
	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_UTMPX} != "no"
FILESGROUPS+=	ACCT
ACCT+=		200.accounting
.endif
ACCTDIR=	/etc/periodic/monthly
ACCTMODE=	${BINMODE}
ACCTPACKAGE=	acct

.include <bsd.prog.mk>
