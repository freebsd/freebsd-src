# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/geom/class/${.CURDIR:T}

TAP_TESTS_SH+= attach_d_test
TAP_TESTS_SH+= configure_b_B_test
TAP_TESTS_SH+= delkey_test
TAP_TESTS_SH+= detach_l_test
TAP_TESTS_SH+= init_B_test
TAP_TESTS_SH+= init_J_test
TAP_TESTS_SH+= init_a_test
TAP_TESTS_SH+= init_i_P_test
TAP_TESTS_SH+= init_test
TAP_TESTS_SH+= integrity_copy_test
TAP_TESTS_SH+= integrity_data_test
TAP_TESTS_SH+= integrity_hmac_test
TAP_TESTS_SH+= kill_test
TAP_TESTS_SH+= nokey_test
TAP_TESTS_SH+= onetime_a_test
TAP_TESTS_SH+= onetime_d_test
TAP_TESTS_SH+= onetime_test
TAP_TESTS_SH+= readonly_test
TAP_TESTS_SH+= resize_test
TAP_TESTS_SH+= setkey_test

TEST_METADATA.init_a_test+=	timeout="1200"
TEST_METADATA.init_test+=	timeout="300"
TEST_METADATA.integrity_copy_test+=	timeout="1200"
TEST_METADATA.integrity_data_test+=	timeout="600"
TEST_METADATA.integrity_hmac_test+=	timeout="600"
TEST_METADATA.onetime_a_test+=	timeout="600"
TEST_METADATA.onetime_test+=	timeout="600"

FILES+=		conf.sh
FILESNAME_conf.sh=	conf.sh
FILESDIR=	${TESTSDIR}

.for t in ${TAP_TESTS_SH}
TEST_METADATA.$t+=	required_user="root"
.endfor

.include <bsd.test.mk>
