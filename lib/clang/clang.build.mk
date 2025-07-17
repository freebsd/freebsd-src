
.include <src.opts.mk>

.ifndef CLANG_SRCS
.error Please define CLANG_SRCS before including this file
.endif

CFLAGS+=	-I${CLANG_SRCS}/include

.if ${MK_CLANG_FULL} != "no"
CFLAGS+=	-DCLANG_ENABLE_ARCMT
CFLAGS+=	-DCLANG_ENABLE_STATIC_ANALYZER
.endif

CFLAGS.gcc+=	-fno-strict-aliasing

.include "llvm.build.mk"
