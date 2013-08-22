# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-bcanalyzer

SRCDIR=	tools/llvm-bcanalyzer
SRCS=	llvm-bcanalyzer.cpp
LLVM_REQUIRES_EH=

LIBDEPS=llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
