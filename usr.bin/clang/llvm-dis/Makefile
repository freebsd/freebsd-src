# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-dis

SRCDIR=	tools/llvm-dis
SRCS=	llvm-dis.cpp

TGHDRS=	Attributes \
	Intrinsics

LIBDEPS=llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
