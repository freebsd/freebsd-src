# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-ar

SRCDIR=	tools/llvm-ar
SRCS=	llvm-ar.cpp

LIBDEPS=llvmarchive \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
