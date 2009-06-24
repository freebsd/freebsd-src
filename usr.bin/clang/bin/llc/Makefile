# $FreeBSD$

PROG_CXX=llc

SRCDIR=	tools/llc
SRCS=	llc.cpp
MAN=

LIBDEPS=llvmselectiondag llvmasmprinter llvmcodegen llvmscalaropts \
	llvmipa llvmtransformutils llvmanalysis llvmbitreader \
	llvmtarget llvmcore llvmsupport llvmsystem \
	llvmcbackend
TGHDRS=	Intrinsics

.include "../../clang.prog.mk"
