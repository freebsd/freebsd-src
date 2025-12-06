ATF_TESTS_C+=	in_cksum_test
.PATH:	${.CURDIR:H}
SRCS.in_cksum_test= in_cksum_test.c utils.c

PACKAGE= tests

ATF_TESTS_PYTEST+=	test_ping.py
ATF_TESTS_SH+=	ping_test
# Exclusive because each injection test case uses the same IP addresses
TEST_METADATA.ping_test+=	is_exclusive="true"

${PACKAGE}FILES+= ping_c1_s56_t1.out
${PACKAGE}FILES+= ping_6_c1_s8_t1.out
${PACKAGE}FILES+= ping_c1_s56_t1_S127.out
${PACKAGE}FILES+= ping_c1_s8_t1_S1.out
${PACKAGE}FILES+= injection.py

.include <bsd.test.mk>
