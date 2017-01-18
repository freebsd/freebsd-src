#	$NetBSD: Makefile,v 1.5 2016/11/24 09:05:17 ozaki-r Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/route

.for name in change flags flags6 route
TESTS_SH+=		t_${name}
TESTS_SH_SRC_t_${name}=	../net_common.sh t_${name}.sh
.endfor


.include <bsd.test.mk>
