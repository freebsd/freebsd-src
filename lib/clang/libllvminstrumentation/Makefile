# $FreeBSD$

.include <src.opts.mk>

LIB=	llvminstrumentation

SRCDIR=	lib/Transforms/Instrumentation
SRCS=	AddressSanitizer.cpp \
	BoundsChecking.cpp \
	DataFlowSanitizer.cpp \
	GCOVProfiling.cpp \
	MemorySanitizer.cpp \
	InstrProfiling.cpp \
	SafeStack.cpp \
	SanitizerCoverage.cpp \
	ThreadSanitizer.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	Instrumentation.cpp
.endif

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
