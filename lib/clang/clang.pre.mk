# $FreeBSD$

.include "llvm.pre.mk"

CLANG_SRCS=	${LLVM_BASE}/clang

CLANG_TBLGEN?=	clang-tblgen
CLANG_TBLGEN_BIN!= which ${CLANG_TBLGEN}
