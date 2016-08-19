# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-profdata

SRCDIR=	tools/llvm-profdata
SRCS=	llvm-profdata.cpp

TGHDRS=	Attributes

LIBDEPS=llvmprofiledata \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
