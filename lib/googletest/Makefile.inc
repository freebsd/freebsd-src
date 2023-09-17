
.include <googletest.test.inc.mk>

GTEST_DIR=	${SRCTOP}/contrib/googletest
GOOGLEMOCK_SRCROOT= ${GTEST_DIR}/googlemock
GOOGLETEST_SRCROOT= ${GTEST_DIR}/googletest

CXXFLAGS+=	${GTESTS_FLAGS}

# Silence warnings about usage of deprecated implicit copy constructors
CXXWARNFLAGS+=  -Wno-deprecated-copy
