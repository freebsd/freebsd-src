# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-extract

SRCDIR=	tools/llvm-extract
SRCS=	llvm-extract.cpp

LIBDEPS=llvmirreader \
	llvmasmparser \
	llvmbitwriter \
	llvmipo \
	llvmvectorize \
	llvmscalaropts \
	llvmprofiledata \
	llvmobject \
	llvmmcparser \
	llvmbitreader \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
