# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-ranlib

SRCDIR=	tools/llvm-ranlib
SRCS=	llvm-ranlib.cpp

LIBDEPS=llvmarchive \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
