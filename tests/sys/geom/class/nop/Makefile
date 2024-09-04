PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/geom/class/${.CURDIR:T}

ATF_TESTS_SH+=	nop_test

# Some tests make use of the "disks" property and kyua may schedule
# them to run at the time time, which the tests do not expect.
TEST_METADATA.nop_test+= is_exclusive="true"

.include <bsd.test.mk>
