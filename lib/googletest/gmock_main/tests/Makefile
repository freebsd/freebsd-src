# $FreeBSD$

.include <bsd.init.mk>

.PATH:	${GOOGLEMOCK_SRCROOT}/src ${GOOGLEMOCK_SRCROOT}/test

PLAIN_TESTS_CXX+=	gmock-actions_test
PLAIN_TESTS_CXX+=	gmock-cardinalities_test
PLAIN_TESTS_CXX+=	gmock-ex_test
PLAIN_TESTS_CXX+=	gmock-generated-actions_test
PLAIN_TESTS_CXX+=	gmock-generated-function-mockers_test
PLAIN_TESTS_CXX+=	gmock-generated-internal-utils_test
PLAIN_TESTS_CXX+=	gmock-generated-matchers_test
PLAIN_TESTS_CXX+=	gmock-internal-utils_test
PLAIN_TESTS_CXX+=	gmock-matchers_test
PLAIN_TESTS_CXX+=	gmock-more-actions_test
PLAIN_TESTS_CXX+=	gmock-nice-strict_test
PLAIN_TESTS_CXX+=	gmock-port_test
PLAIN_TESTS_CXX+=	gmock-spec-builders_test
PLAIN_TESTS_CXX+=	gmock_link_test
PLAIN_TESTS_CXX+=	gmock_test

CXXFLAGS+=		-I${GOOGLEMOCK_SRCROOT}/include
CXXFLAGS+=		-I${GOOGLEMOCK_SRCROOT}

.for test in ${PLAIN_TESTS_CXX}
SRCS.${test}=		gmock_main.cc
.endfor

SRCS.gmock_link_test+=	gmock_link2_test.cc

LIBADD=		gmock

.include <bsd.test.mk>
