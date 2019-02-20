# $FreeBSD$

.include <bsd.init.mk>

.PATH:	${GOOGLETEST_SRCROOT}/src ${GOOGLETEST_SRCROOT}/test

GTESTS+=	gtest_environment_test
GTESTS+=	gtest_no_test_unittest
GTESTS+=	googletest-param-test-test
GTESTS+=	gtest_premature_exit_test
GTESTS+=	gtest_repeat_test
GTESTS+=	gtest_stress_test
GTESTS+=	gtest_throw_on_failure_ex_test
GTESTS+=	gtest-unittest-api_test

SRCS.googletest-param-test-test+= googletest-param-test-test.cc
SRCS.googletest-param-test-test+= googletest-param-test2-test.cc
SRCS.gtest_premature_exit_test+= gtest_premature_exit_test.cc

.for test in ${GTESTS}
# XXX: linker error; no main(..)
#SRCS.${test}=	gtest-all.cc
SRCS.${test}?=	gtest_main.cc
.endfor

CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}/include
CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}

LIBADD=		gtest

# XXX: https://github.com/google/googletest/pull/2119
NO_WERROR=

.include <bsd.test.mk>
