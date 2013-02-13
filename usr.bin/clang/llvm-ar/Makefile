# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-ar

SRCDIR=	tools/llvm-ar
SRCS=	llvm-ar.cpp
LLVM_REQUIRES_EH=

LIBDEPS=llvmarchive \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
