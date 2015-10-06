# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-dis

SRCDIR=	tools/llvm-dis
SRCS=	llvm-dis.cpp

TGHDRS=	Intrinsics
LIBDEPS=llvmanalysis \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
