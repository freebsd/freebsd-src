
PACKAGE=	tests

ATF_TESTS_SH=	port

# "ctladm port" does not report the name of the port just created, so we can't
# cleanup unless we assume that no other test created a port too.
TEST_METADATA+= is_exclusive="true"

.include <bsd.test.mk>
