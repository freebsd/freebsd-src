
.include <bsd.init.mk>

.PATH:	${GOOGLEMOCK_SRCROOT}/include ${GOOGLEMOCK_SRCROOT}/src

PACKAGE=	libprivategtest
LIB_CXX=	gmock
SHLIB_MAJOR=	0
PRIVATELIB=	true

CXXFLAGS+=	-I${GOOGLEMOCK_SRCROOT}/include
CXXFLAGS+=	-I${GOOGLEMOCK_SRCROOT}
CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}/include

LIBADD+=	gtest
LDFLAGS+=	-L${.OBJDIR:H}/gtest

INCSGROUPS=	MAIN_INCS INTERNAL_INCS INTERNAL_CUSTOM_INCS

MAIN_INCSDIR=	${INCLUDEDIR}${PRIVATELIB:D/private/${LIB}}
MAIN_INCS+=	gmock/gmock-actions.h
MAIN_INCS+=	gmock/gmock-cardinalities.h
MAIN_INCS+=	gmock/gmock-function-mocker.h
MAIN_INCS+=	gmock/gmock-matchers.h
MAIN_INCS+=	gmock/gmock-more-actions.h
MAIN_INCS+=	gmock/gmock-more-matchers.h
MAIN_INCS+=	gmock/gmock-nice-strict.h
MAIN_INCS+=	gmock/gmock-spec-builders.h
MAIN_INCS+=	gmock/gmock.h

INTERNAL_INCSDIR=	${MAIN_INCSDIR}/internal
INTERNAL_INCS+=		gmock/internal/gmock-internal-utils.h
INTERNAL_INCS+=		gmock/internal/gmock-port.h
INTERNAL_INCS+=		gmock/internal/gmock-pp.h

INTERNAL_CUSTOM_INCSDIR= ${INTERNAL_INCSDIR}/custom
INTERNAL_CUSTOM_INCS+=	gmock/internal/custom/gmock-generated-actions.h
INTERNAL_CUSTOM_INCS+=	gmock/internal/custom/gmock-matchers.h
INTERNAL_CUSTOM_INCS+=	gmock/internal/custom/gmock-port.h

SRCS+=		gmock-all.cc

.include <bsd.lib.mk>
