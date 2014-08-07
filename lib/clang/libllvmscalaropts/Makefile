# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmscalaropts

SRCDIR=	lib/Transforms/Scalar
SRCS=	ADCE.cpp \
	CodeGenPrepare.cpp \
	ConstantProp.cpp \
	CorrelatedValuePropagation.cpp \
	DCE.cpp \
	DeadStoreElimination.cpp \
	EarlyCSE.cpp \
	FlattenCFGPass.cpp \
	GVN.cpp \
	GlobalMerge.cpp \
	IndVarSimplify.cpp \
	JumpThreading.cpp \
	LICM.cpp \
	LoopDeletion.cpp \
	LoopIdiomRecognize.cpp \
	LoopRerollPass.cpp \
	LoopRotation.cpp \
	LoopStrengthReduce.cpp \
	LoopUnrollPass.cpp \
	LoopUnswitch.cpp \
	LowerAtomic.cpp \
	MemCpyOptimizer.cpp \
	PartiallyInlineLibCalls.cpp \
	Reassociate.cpp \
	Reg2Mem.cpp \
	SCCP.cpp \
	SROA.cpp \
	SampleProfile.cpp \
	ScalarReplAggregates.cpp \
	SimplifyCFGPass.cpp \
	Sink.cpp \
	StructurizeCFG.cpp \
	TailRecursionElimination.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	LoopInstSimplify.cpp \
	Scalar.cpp
.endif

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
