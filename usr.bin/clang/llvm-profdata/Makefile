# $FreeBSD$

PROG_CXX=	llvm-profdata

SRCDIR=		llvm/tools/llvm-profdata
SRCS+=		llvm-profdata.cpp

CFLAGS.llvm-profdata.cpp+=	-Dllvm_profdata_main=main

.include "../llvm.prog.mk"
