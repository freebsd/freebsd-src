# $FreeBSD$

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
