# $FreeBSD$

PROG_CXX=	llvm-mc
MAN=

SRCDIR=		llvm/tools/llvm-mc
SRCS+=		Disassembler.cpp
SRCS+=		llvm-mc.cpp

LIBADD+=	z
LIBADD+=	zstd

.include "../llvm.prog.mk"
