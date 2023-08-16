
PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/geom/class/${.CURDIR:T}

ATF_TESTS_SH+=	ggate_test

# Tests listen on the same port.
TEST_METADATA.ggate_test+= is_exclusive=true

.include <bsd.test.mk>
