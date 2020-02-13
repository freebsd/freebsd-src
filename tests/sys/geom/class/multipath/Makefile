# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/geom/class/${.CURDIR:T}

ATF_TESTS_SH+=	failloop
ATF_TESTS_SH+=	misc
TEST_METADATA.failloop+= is_exclusive=true

${PACKAGE}FILES+=		conf.sh

.include <bsd.test.mk>
