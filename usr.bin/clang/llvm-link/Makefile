# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-link

SRCDIR=	tools/llvm-link
SRCS=	llvm-link.cpp

LIBDEPS=llvmirreader \
	llvmasmparser \
	llvmbitwriter \
	llvmbitreader \
	llvmlinker \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
