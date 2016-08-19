# $FreeBSD$

.include <src.opts.mk>

LIB=	llvminstrumentation

SRCDIR=	lib/Transforms/Instrumentation
SRCS=	AddressSanitizer.cpp \
	BoundsChecking.cpp \
	DataFlowSanitizer.cpp \
	EfficiencySanitizer.cpp \
	GCOVProfiling.cpp \
	IndirectCallPromotion.cpp \
	InstrProfiling.cpp \
	Instrumentation.cpp \
	MemorySanitizer.cpp \
	PGOInstrumentation.cpp \
	SanitizerCoverage.cpp \
	ThreadSanitizer.cpp

TGHDRS=	Attributes \
	Intrinsics

.include "../clang.lib.mk"
