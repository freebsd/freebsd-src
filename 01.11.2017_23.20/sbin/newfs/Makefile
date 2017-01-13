# $NetBSD: Makefile,v 1.2 2011/03/06 17:08:41 bouyer Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/sbin/newfs

.for name in t_enable_quotas
TESTS_SH+=	${name}
TESTS_SH_SRC_${name}=	quotas_common.sh ${name}.sh
.endfor

.include <bsd.test.mk>
