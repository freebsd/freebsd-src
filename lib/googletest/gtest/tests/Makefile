# $FreeBSD$

.include <bsd.init.mk>

.PATH:	${GOOGLETEST_SRCROOT}/src ${GOOGLETEST_SRCROOT}/test

PLAIN_TESTS_CXX+=	gtest_environment_test
PLAIN_TESTS_CXX+=	gtest_no_test_unittest
PLAIN_TESTS_CXX+=	googletest-param-test-test
PLAIN_TESTS_CXX+=	gtest_premature_exit_test
PLAIN_TESTS_CXX+=	gtest_repeat_test
PLAIN_TESTS_CXX+=	gtest_stress_test
PLAIN_TESTS_CXX+=	gtest_throw_on_failure_ex_test
PLAIN_TESTS_CXX+=	gtest-unittest-api_test

SRCS.googletest-param-test-test+= googletest-param-test-test.cc
SRCS.googletest-param-test-test+= googletest-param-test2-test.cc
SRCS.gtest_premature_exit_test+= gtest_premature_exit_test.cc

.for test in ${PLAIN_TESTS_CXX}
# XXX: linker error; no main(..)
#SRCS.${test}=		gtest-all.cc
SRCS.${test}?=		gtest_main.cc
.endfor

CXXFLAGS+=		-I${GOOGLETEST_SRCROOT}/include
CXXFLAGS+=		-I${GOOGLETEST_SRCROOT}

LIBADD=		gtest

.include <bsd.test.mk>
