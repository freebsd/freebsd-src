# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-ranlib

SRCDIR=	tools/llvm-ranlib
SRCS=	llvm-ranlib.cpp
LLVM_REQUIRES_EH=

LIBDEPS=llvmarchive \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
