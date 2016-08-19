# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-as

SRCDIR=	tools/llvm-as
SRCS=	llvm-as.cpp

TGHDRS=	Attributes

LIBDEPS=llvmasmparser \
	llvmbitwriter \
	llvmanalysis \
	llvmprofiledata \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
