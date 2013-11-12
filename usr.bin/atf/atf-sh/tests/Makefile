# $FreeBSD$

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/usr.bin/atf/atf-sh

ATF=		${.CURDIR:H:H:H:H}/contrib/atf
.PATH:		${ATF}/atf-sh

ATF_TESTS_SH+=	atf_check_test
ATF_TESTS_SH+=	config_test
ATF_TESTS_SH+=	integration_test
ATF_TESTS_SH+=	misc_helpers
ATF_TESTS_SH+=	normalize_test
ATF_TESTS_SH+=	tc_test
ATF_TESTS_SH+=	tp_test

.include <atf.test.mk>
