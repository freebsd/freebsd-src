# $NetBSD: Makefile,v 1.3 2011/01/05 02:25:27 riz Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/sbin/resize_ffs

.for name in t_grow t_shrink t_grow_swapped t_shrink_swapped
TESTS_SH+=	${name}
TESTS_SH_SRC_${name}=	common.sh ${name}.sh
.endfor

FILESDIR=	${TESTSDIR}
FILES=		testdata.tar.gz.base64
FILES+=		testdata.md5

.include <bsd.test.mk>
