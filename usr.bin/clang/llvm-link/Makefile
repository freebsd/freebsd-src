# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-link

SRCDIR=	tools/llvm-link
SRCS=	llvm-link.cpp

TGHDRS=	Attributes

LIBDEPS=llvmobject \
	llvmmcparser \
	llvmmc \
	llvmirreader \
	llvmasmparser \
	llvmbitwriter \
	llvmbitreader \
	llvmlinker \
	llvmtransformutils \
	llvmanalysis \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
