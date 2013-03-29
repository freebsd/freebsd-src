# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-as

SRCDIR=	tools/llvm-as
SRCS=	llvm-as.cpp
LLVM_REQUIRES_EH=

LIBDEPS=llvmbitwriter \
	llvmasmparser \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
