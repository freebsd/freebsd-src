# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-prof

SRCDIR=	tools/llvm-prof
SRCS=	llvm-prof.cpp

LIBDEPS=llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
