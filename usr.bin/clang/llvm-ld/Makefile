# $FreeBSD: head/usr.bin/clang/llvm-ld/Makefile 231057 2012-02-05 23:56:22Z dim $

PROG_CXX=llvm-ld

SRCDIR=	tools/llvm-ld
SRCS=	Optimize.cpp \
	llvm-ld.cpp

TGHDRS=	Intrinsics
LIBDEPS=llvmbitwriter \
	llvmlinker \
	llvmarchive \
	llvmbitreader \
	llvmipo \
	llvmvectorize \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
