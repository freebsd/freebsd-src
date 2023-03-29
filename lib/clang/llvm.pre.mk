
# $FreeBSD$

LLVM_BASE=	${SRCTOP}/contrib/llvm-project
LLVM_SRCS=	${LLVM_BASE}/llvm

LLVM_TBLGEN?=	llvm-tblgen
LLVM_TBLGEN_BIN!= which ${LLVM_TBLGEN}
