# $FreeBSD$

SRCDIR=${.CURDIR}/../../..
.include "Makefile.common"

PROG_CXX=	zfsd

.include <bsd.prog.mk>

# Check for the existence of the googletest and googlemock header files, which
# come from ports.  Don't compile the tests without them.
.if exists(${LOCALBASE}/include/gtest/gtest.h) && exists(${LOCALBASE}/include/gmock/gmock.h)
.else
SUBDIR=
.endif
