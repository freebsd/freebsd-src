# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-extract

SRCDIR=	tools/llvm-extract
SRCS=	llvm-extract.cpp

TGHDRS=	Attributes

LIBDEPS=llvmbitwriter \
	llvmipo \
	llvmvectorize \
	llvmscalaropts \
	llvmprofiledata \
	llvmobject \
	llvmmcparser \
	llvmmc \
	llvmlinker \
	llvmirreader \
	llvmbitreader \
	llvmasmparser \
	llvminstcombine \
	llvmtransformutils \
	llvmanalysis \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
