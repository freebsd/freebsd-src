# $FreeBSD$

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}
TESTSDIR=	${TESTSBASE}/sys/geom/class/${.CURDIR:T}
TESTS+=		${FILES}

TAP_TESTS_SH+=	1_test
TAP_TESTS_SH+=	2_test
TAP_TESTS_SH+=	3_test

FILES+=		conf.sh
FILESNAME_conf.sh=	conf.sh
FILESDIR=	${TESTSDIR}

.for t in ${TAP_TESTS_SH}
TEST_METADATA.$t+=	required_user="root"
.endfor

.include <bsd.test.mk>
