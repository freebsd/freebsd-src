# $FreeBSD$

.include <bsd.init.mk>

.PATH:	${GOOGLETEST_SRCROOT}/src ${GOOGLETEST_SRCROOT}/test

GTESTS+=	gtest-death-test_ex_catch_test
GTESTS+=	gtest-death-test_ex_nocatch_test
GTESTS+=	gtest_environment_test
GTESTS+=	gtest_no_test_unittest
GTESTS+=	googletest-param-test-test
GTESTS+=	gtest_premature_exit_test
GTESTS+=	gtest_repeat_test
GTESTS+=	gtest_stress_test
GTESTS+=	gtest_throw_on_failure_ex_test
GTESTS+=	gtest-unittest-api_test

CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}/include
CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}

gtest-death-test_ex_catch_test.cc gtest-death-test_ex_nocatch_test.cc: googletest-death-test_ex_test.cc
	${CP} ${.ALLSRC} ${.TARGET}

CXXFLAGS.gtest-death-test_ex_catch_test+=       \
	-DGTEST_ENABLE_CATCH_EXCEPTIONS_=1	\
	-fexceptions

CXXFLAGS.gtest-death-test_ex_nocatch_test+=     \
	-DGTEST_ENABLE_CATCH_EXCEPTIONS_=0	\
	-fexceptions

SRCS.googletest-param-test-test=	\
	googletest-param-test-test.cc	\
	googletest-param-test2-test.cc

LIBADD+=	gtest

# XXX: explicitly listing -lpthread is incorrect. src.libnames.mk should be
#      handling this.
LIBADD.gtest_stress_test+=	pthread

# XXX: https://github.com/google/googletest/pull/2119
NO_WERROR=

.include <bsd.test.mk>
