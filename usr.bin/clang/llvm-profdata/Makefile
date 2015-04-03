# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-profdata

SRCDIR=	tools/llvm-profdata
SRCS=	llvm-profdata.cpp

LIBDEPS=llvmprofiledata \
	llvmobject \
	llvmmcparser \
	llvmmc \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
