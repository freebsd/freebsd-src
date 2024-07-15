.include <bsd.init.mk>

.PATH:	${GOOGLEMOCK_SRCROOT}/src ${GOOGLEMOCK_SRCROOT}/test

GTESTS+=	gmock-actions_test
GTESTS+=	gmock-cardinalities_test
GTESTS+=	gmock-function-mocker_test
GTESTS+=	gmock-internal-utils_test
GTESTS+=	gmock-matchers-arithmetic_test
GTESTS+=	gmock-matchers-comparisons_test
GTESTS+=	gmock-matchers-containers_test
GTESTS+=	gmock-matchers-misc_test
GTESTS+=	gmock-more-actions_test
GTESTS+=	gmock-nice-strict_test
GTESTS+=	gmock-port_test
GTESTS+=	gmock-spec-builders_test
GTESTS+=	gmock_ex_test
GTESTS+=	gmock_link_test
GTESTS+=	gmock_test

CXXFLAGS+=	-I${GOOGLEMOCK_SRCROOT}/include
CXXFLAGS+=	-I${GOOGLEMOCK_SRCROOT}
CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}/include
CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}

SRCS.gmock_link_test=		\
	gmock_link_test.cc	\
	gmock_link2_test.cc

LIBADD=		gmock_main

CXXFLAGS.clang+=	-Wno-error=inconsistent-missing-override
CXXFLAGS.clang+=	-Wno-error=missing-variable-declarations
CXXFLAGS.clang+=	-Wno-error=sign-compare
CXXFLAGS.clang+=	-Wno-error=unused-parameter
MK_WERROR.gcc=		no

.include <bsd.test.mk>
