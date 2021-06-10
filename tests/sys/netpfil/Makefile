# $FreeBSD$

.include <src.opts.mk>

TESTSDIR=		${TESTSBASE}/sys/netpfil
TESTS_SUBDIRS+=		common

.if ${MK_PF} != "no"
TESTS_SUBDIRS+=		pf
.endif

.include <bsd.test.mk>
