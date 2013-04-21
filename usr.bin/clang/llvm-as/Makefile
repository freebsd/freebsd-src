# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-as

SRCDIR=	tools/llvm-as
SRCS=	llvm-as.cpp

LIBDEPS=llvmbitwriter \
	llvmasmparser \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
