
GTESTS_CXXFLAGS+= -DGTEST_HAS_POSIX_RE=1
GTESTS_CXXFLAGS+= -DGTEST_HAS_PTHREAD=1
GTESTS_CXXFLAGS+= -DGTEST_HAS_STREAM_REDIRECTION=1
GTESTS_CXXFLAGS+= -frtti

.include <bsd.compiler.mk>

.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 100000
# Required until googletest is upgraded to a more recent version (after
# upstream commit efecb0bfa687cf87836494f5d62868485c00fb66).
GTESTS_CXXFLAGS+= -Wno-deprecated-copy

# Required until googletest is upgraded to a more recent version (after
# upstream commit d44b137fd104dfffdcdea103f7de11b9eccc45c2).
GTESTS_CXXFLAGS+= -Wno-signed-unsigned-wchar
.endif

# XXX: src.libnames.mk should handle adding this directory for libgtest's,
# libgmock's, etc, headers.
CXXFLAGS+=	-I${DESTDIR}${INCLUDEDIR}/private

CXXSTD?=	c++14

NO_WTHREAD_SAFETY=
