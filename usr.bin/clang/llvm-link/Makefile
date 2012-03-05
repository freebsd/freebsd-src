# $FreeBSD: head/usr.bin/clang/llvm-link/Makefile 231057 2012-02-05 23:56:22Z dim $

PROG_CXX=llvm-link

SRCDIR=	tools/llvm-link
SRCS=	llvm-link.cpp

LIBDEPS=llvmasmparser \
	llvmbitwriter \
	llvmlinker \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmarchive \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
