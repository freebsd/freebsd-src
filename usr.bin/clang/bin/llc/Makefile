# $FreeBSD$

PROG_CXX=llc

SRCDIR=	tools/llc
SRCS=	llc.cpp
MAN=

LIBDEPS=llvmselectiondag llvmasmprinter llvmcodegen llvmscalaropts \
	llvmipa llvmtransformutils llvmanalysis llvmbitreader \
	llvmtarget llvmcore llvmsupport llvmsystem
TGHDRS=	Intrinsics
OBJDEPS=llvmcbackend

.include "../../clang.prog.mk"
