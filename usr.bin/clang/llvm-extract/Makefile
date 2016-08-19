# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-extract

SRCDIR=	tools/llvm-extract
SRCS=	llvm-extract.cpp

TGHDRS=	Attributes

LIBDEPS=llvmbitwriter \
	llvmipo \
	llvmirreader \
	llvmasmparser \
	llvminstrumentation \
	llvmlinker \
	llvmobject \
	llvmbitreader \
	llvmmcparser \
	llvmmc \
	llvmscalaropts \
	llvminstcombine \
	llvmvectorize \
	llvmtransformutils \
	llvmanalysis \
	llvmprofiledata \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
