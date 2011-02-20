# $FreeBSD$

LIB=	llvmscalaropts

SRCDIR=	lib/Transforms/Scalar
SRCS=	ADCE.cpp \
	BasicBlockPlacement.cpp \
	CodeGenPrepare.cpp \
	ConstantProp.cpp \
	CorrelatedValuePropagation.cpp \
	DCE.cpp \
	DeadStoreElimination.cpp \
	EarlyCSE.cpp \
	GEPSplitter.cpp \
	GVN.cpp \
	IndVarSimplify.cpp \
	JumpThreading.cpp \
	LICM.cpp \
	LoopDeletion.cpp \
	LoopIdiomRecognize.cpp \
	LoopRotation.cpp \
	LoopStrengthReduce.cpp \
	LoopUnrollPass.cpp \
	LoopUnswitch.cpp \
	MemCpyOptimizer.cpp \
	Reassociate.cpp \
	SCCP.cpp \
	ScalarReplAggregates.cpp \
	SimplifyCFGPass.cpp \
	SimplifyHalfPowrLibCalls.cpp \
	SimplifyLibCalls.cpp \
	Sink.cpp \
	TailDuplication.cpp \
	TailRecursionElimination.cpp

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
