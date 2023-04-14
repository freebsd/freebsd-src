# $FreeBSD$

PROG_CXX=	opt

SRCDIR=		llvm/tools/opt
SRCS+=		AnalysisWrappers.cpp
SRCS+=		BreakpointPrinter.cpp
SRCS+=		NewPMDriver.cpp
SRCS+=		opt.cpp

LIBADD+=	z
LIBADD+=	zstd

.include "../llvm.prog.mk"
