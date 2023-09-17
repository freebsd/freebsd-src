
.include <bsd.own.mk>

PACKAGE=	tests

PROGS+=		spin_helper
BINDIR=		${TESTSDIR}

TAP_TESTS_SH=			pgrep-_f_test
TAP_TESTS_SH+=			pgrep-_lf_test
TAP_TESTS_SH+=			pgrep-_p_test
TAP_TESTS_SH+=			pgrep-_u_test
TAP_TESTS_SH+=			pgrep-_g_test
TAP_TESTS_SH+=			pgrep-_s_test
TAP_TESTS_SH+=			pgrep-g_test
TAP_TESTS_SH+=			pgrep-i_test
TAP_TESTS_SH+=			pgrep-f_test
TAP_TESTS_SH+=			pgrep-j_test
TEST_METADATA.pgrep-j_test+=	required_user="root"
TEST_METADATA.pgrep-j_test+=	required_programs="jail jls"
TAP_TESTS_SH+=			pgrep-l_test
TAP_TESTS_SH+=			pgrep-n_test
TAP_TESTS_SH+=			pgrep-o_test
TAP_TESTS_SH+=			pgrep-q_test
TAP_TESTS_SH+=			pgrep-s_test
TAP_TESTS_SH+=			pgrep-t_test
TAP_TESTS_SH+=			pgrep-v_test
TAP_TESTS_SH+=			pgrep-x_test
TAP_TESTS_SH+=			pkill-_f_test
TAP_TESTS_SH+=			pkill-_lf_test
TAP_TESTS_SH+=			pkill-_p_test
TAP_TESTS_SH+=			pkill-_u_test
TAP_TESTS_SH+=			pkill-_g_test
TAP_TESTS_SH+=			pkill-g_test
TAP_TESTS_SH+=			pkill-i_test
TAP_TESTS_SH+=			pkill-j_test
TEST_METADATA.pkill-j_test+=	required_user="root"
TEST_METADATA.pkill-j_test+=	required_programs="jail jls"
TAP_TESTS_SH+=			pkill-s_test
TAP_TESTS_SH+=			pkill-t_test
TAP_TESTS_SH+=			pkill-x_test

.include <bsd.test.mk>
