.include <bsd.init.mk>

.PATH:	${GOOGLETEST_SRCROOT}/src ${GOOGLETEST_SRCROOT}/test

GTESTS+=	googletest-death-test-test
GTESTS+=	googletest-message-test
GTESTS+=	googletest-options-test
GTESTS+=	googletest-port-test
GTESTS+=	googletest-printers-test
GTESTS+=	googletest-test-part-test
GTESTS+=	gtest-typed-test_test
GTESTS+=	gtest_main_unittest
GTESTS+=	gtest_pred_impl_unittest
GTESTS+=	gtest_prod_test
GTESTS+=	gtest_sole_header_test

# Crashes when listing testcases using googletest engine in kyua 0.15.0
# (probably test bugs).
PLAIN_TESTS_CXX+= googletest-filepath-test
PLAIN_TESTS_CXX+= googletest-listener-test
# These test scearios don't work with the googletest engine in kyua 0.15.0.
PLAIN_TESTS_CXX+= gtest_skip_in_environment_setup_test
PLAIN_TESTS_CXX+= gtest_skip_test
# Some of the test scenarios show up as "skipped", when in reality they're just
# testing skipped functionality. Don't inflate the testcase numbers
# unnecessarily as it can serve to confuse downstream users.
PLAIN_TESTS_CXX+= gtest_unittest

CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}/include
CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}

SRCS.gtest-typed-test_test=		\
	gtest-typed-test_test.cc	\
	gtest-typed-test2_test.cc

SRCS.gtest_prod_test=			\
	gtest_prod_test.cc		\
	production.cc

LIBADD+=	gtest_main

LIBADD.googletest-port-test+=	pthread
LIBADD.gtest_unittest+=		pthread

CXXFLAGS.clang+=	-Wno-error=missing-variable-declarations
CXXFLAGS.clang+=	-Wno-error=unused-but-set-variable
MK_WERROR.gcc=		no

.include <bsd.test.mk>
