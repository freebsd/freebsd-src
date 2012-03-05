# $FreeBSD: head/usr.bin/clang/llvm-extract/Makefile 231057 2012-02-05 23:56:22Z dim $

PROG_CXX=llvm-extract

SRCDIR=	tools/llvm-extract
SRCS=	llvm-extract.cpp

LIBDEPS=llvmasmparser \
	llvmbitwriter \
	llvmbitreader \
	llvmipo \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
