# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-profdata

SRCDIR=	tools/llvm-profdata
SRCS=	llvm-profdata.cpp

TGHDRS=	Attributes

LIBDEPS=llvmprofiledata \
	llvmobject \
	llvmmcparser \
	llvmmc \
	llvmbitreader \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
