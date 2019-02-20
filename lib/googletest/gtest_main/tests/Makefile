# $FreeBSD$

.include <bsd.init.mk>

.PATH:	${GOOGLETEST_SRCROOT}/src ${GOOGLETEST_SRCROOT}/test

GTESTS+=	googletest-death-test-test
GTESTS+=	googletest-filepath-test
GTESTS+=	googletest-linked-ptr-test
GTESTS+=	googletest-listener-test
GTESTS+=	gtest_main_unittest
GTESTS+=	googletest-message-test
GTESTS+=	googletest-options-test
GTESTS+=	googletest-port-test
GTESTS+=	gtest_pred_impl_unittest
GTESTS+=	googletest-printers-test
GTESTS+=	gtest_prod_test
GTESTS+=	gtest_sole_header_test
GTESTS+=	googletest-test-part-test
GTESTS+=	gtest-typed-test_test
GTESTS+=	gtest_unittest

.for test in ${GTESTS}
SRCS.${test}=	gtest_main.cc
.endfor

CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}/include
CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}

SRCS.gtest-typed-test_test+=	gtest-typed-test2_test.cc

LIBADD=		gtest

.include <bsd.test.mk>
