# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-bcanalyzer

SRCDIR=	tools/llvm-bcanalyzer
SRCS=	llvm-bcanalyzer.cpp

TGHDRS=	Attributes

LIBDEPS=llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
