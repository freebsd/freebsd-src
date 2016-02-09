# $FreeBSD$

.include "Makefile.common"

# C++
WANT_CHERI=none

PROG_CXX=	zfsd
MAN=		zfsd.8

.include <bsd.prog.mk>

# The unittests require devel/googletest and devel/googlemock from ports.
# Don't automatically build them.
SUBDIR=
