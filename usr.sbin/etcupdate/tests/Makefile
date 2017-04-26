# $FreeBSD$

PLAIN_TESTS_SH=
.for test in	always_test \
		conflicts_test \
		fbsdid_test \
		ignore_test \
		preworld_test \
		tests_test \
		tzsetup_test
PLAIN_TESTS_SH+=	${test}
TEST_METADATA.${test}+=	required_user="root"
# Double the default timeout, some are very slow under emulation
TEST_METADATA.${test}+=	timeout="600"
.endfor

.include <bsd.test.mk>
