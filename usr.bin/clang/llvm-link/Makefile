# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-link

SRCDIR=	tools/llvm-link
SRCS=	llvm-link.cpp

TGHDRS=	Attributes

LIBDEPS=llvmbitwriter \
	llvmirreader \
	llvmlinker \
	llvmobject \
	llvmtransformutils \
	llvmasmparser \
	llvmanalysis \
	llvmprofiledata \
	llvmbitreader \
	llvmcore \
	llvmmcparser \
	llvmmc \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
