.include <src.opts.mk>

.if defined(TOOLS_PREFIX)
MK_PIE:=	no	# Explicit libXXX.a references
.endif

.if ${MK_LLVM_FULL_DEBUGINFO} == "no"
.if ${COMPILER_TYPE} == "clang"
DEBUG_FILES_CFLAGS= -gline-tables-only
.else
DEBUG_FILES_CFLAGS= -g1
.endif
.endif

WARNS?=		0

.include "../Makefile.inc"
