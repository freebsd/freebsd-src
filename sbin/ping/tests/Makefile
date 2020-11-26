# $FreeBSD$

ATF_TESTS_C+=	in_cksum_test
SRCS.in_cksum_test= in_cksum_test.c ../utils.c

PACKAGE= tests

ATF_TESTS_SH+=	ping_test
${PACKAGE}FILES+= ping_c1_s56_t1.out
${PACKAGE}FILES+= ping_6_c1_s8_t1.out

.include <bsd.test.mk>
