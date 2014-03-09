# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-ar

SRCDIR=	tools/llvm-ar
SRCS=	llvm-ar.cpp

LIBDEPS=llvmobject \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
