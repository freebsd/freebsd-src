# $FreeBSD$

.include <bsd.init.mk>

.PATH:	${GOOGLETEST_SRCROOT}/src ${GOOGLETEST_SRCROOT}/test

PLAIN_TESTS_CXX+=	googletest-death-test-test
PLAIN_TESTS_CXX+=	googletest-filepath-test
PLAIN_TESTS_CXX+=	googletest-linked-ptr-test
PLAIN_TESTS_CXX+=	googletest-listener-test
PLAIN_TESTS_CXX+=	gtest_main_unittest
PLAIN_TESTS_CXX+=	googletest-message-test
PLAIN_TESTS_CXX+=	googletest-options-test
PLAIN_TESTS_CXX+=	googletest-port-test
PLAIN_TESTS_CXX+=	gtest_pred_impl_unittest
PLAIN_TESTS_CXX+=	googletest-printers-test
PLAIN_TESTS_CXX+=	gtest_prod_test
PLAIN_TESTS_CXX+=	gtest_sole_header_test
PLAIN_TESTS_CXX+=	googletest-test-part-test
PLAIN_TESTS_CXX+=	gtest-typed-test_test
PLAIN_TESTS_CXX+=	gtest_unittest

.for test in ${PLAIN_TESTS_CXX}
SRCS.${test}=		gtest_main.cc
.endfor

CXXFLAGS+=		-I${GOOGLETEST_SRCROOT}/include
CXXFLAGS+=		-I${GOOGLETEST_SRCROOT}

SRCS.gtest-typed-test_test+=	gtest-typed-test2_test.cc

LIBADD=		gtest

.include <bsd.test.mk>
