# $FreeBSD$

GTESTS_CXXFLAGS+= -DGTEST_HAS_POSIX_RE=1
GTESTS_CXXFLAGS+= -DGTEST_HAS_PTHREAD=1
GTESTS_CXXFLAGS+= -DGTEST_HAS_STREAM_REDIRECTION=1
GTESTS_CXXFLAGS+= -frtti

# XXX: src.libnames.mk should handle adding this directory for libgtest's,
# libgmock's, etc, headers.
CXXFLAGS+=	-I${DESTDIR}${INCLUDEDIR}/private

NO_WTHREAD_SAFETY=
