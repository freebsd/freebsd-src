# $FreeBSD: head/usr.bin/clang/llvm-dis/Makefile 231057 2012-02-05 23:56:22Z dim $

PROG_CXX=llvm-dis

SRCDIR=	tools/llvm-dis
SRCS=	llvm-dis.cpp
LLVM_REQUIRES_EH=

TGHDRS=	Intrinsics
LIBDEPS=llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
