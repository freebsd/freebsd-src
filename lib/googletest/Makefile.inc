.include <googletest.test.inc.mk>

PACKAGE?=	googletest

GTEST_DIR=	${SRCTOP}/contrib/googletest
GOOGLEMOCK_SRCROOT= ${GTEST_DIR}/googlemock
GOOGLETEST_SRCROOT= ${GTEST_DIR}/googletest

CXXFLAGS+=	${GTESTS_FLAGS}

# Silence warnings about usage of deprecated implicit copy constructors
CXXWARNFLAGS+=  -Wno-deprecated-copy

# Silence warnings about implicit character conversions in gtest-printers.h
CXXWARNFLAGS+=	${NO_WCHARACTER_CONVERSION}
