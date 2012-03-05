# $FreeBSD: head/usr.bin/clang/opt/Makefile 231057 2012-02-05 23:56:22Z dim $

PROG_CXX=opt

SRCDIR=	tools/opt
SRCS=	AnalysisWrappers.cpp \
	GraphPrinters.cpp \
	PrintSCC.cpp \
	opt.cpp

TGHDRS=	Intrinsics
LIBDEPS=llvmipo \
	llvmscalaropts \
	llvminstcombine \
	llvminstrumentation \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmasmparser \
	llvmbitwriter \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
