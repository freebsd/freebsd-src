# $FreeBSD$

.include <src.opts.mk>

TESTSDIR=		${TESTSBASE}/sys/netpfil

.if ${MK_PF} != "no"
TESTS_SUBDIRS+=		pf \
					common
.endif

.include <bsd.test.mk>
