# $FreeBSD$

PROG_CXX=opt

SRCDIR=	tools/opt
SRCS=	AnalysisWrappers.cpp GraphPrinters.cpp PrintSCC.cpp opt.cpp
MAN=

TGHDRS=	Intrinsics
LIBDEPS=llvminstrumentation llvmscalaropts llvmbitreader llvmbitwriter \
	llvmtarget llvmipo llvmipa llvmtransformutils llvmanalysis \
	llvmcore llvmsupport llvmsystem

.include "../../clang.prog.mk"
