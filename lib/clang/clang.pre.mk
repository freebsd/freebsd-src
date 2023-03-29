# $FreeBSD$

.include "llvm.pre.mk"

CLANG_SRCS=		${LLVM_BASE}/clang

CLANG_TBLGEN?=		clang-tblgen
CLANG_TBLGEN_BIN!=	which ${CLANG_TBLGEN} || echo __nonexistent_clang_tblgen__
CLANG_TBLGEN_OPTS?=	--write-if-changed
