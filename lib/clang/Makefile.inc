
.include <bsd.compiler.mk>

PACKAGE=	clang

# Build only PIE static libraries; bsd.lib.mk does support this directly.
MK_PIE:=	no
CFLAGS+=	${PIEFLAG} ${SHARED_CFLAGS}
CXXFLAGS+=	${PIEFLAG} ${SHARED_CXXFLAGS}

.if ${COMPILER_TYPE} == "clang"
DEBUG_FILES_CFLAGS= -gline-tables-only
.else
DEBUG_FILES_CFLAGS= -g1
.endif

WARNS?=		0
