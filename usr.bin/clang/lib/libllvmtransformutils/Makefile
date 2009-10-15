# $FreeBSD$

LIB=	llvmtransformutils

SRCDIR=	lib/Transforms/Utils
SRCS=	AddrModeMatcher.cpp BasicBlockUtils.cpp BasicInliner.cpp \
	BreakCriticalEdges.cpp CloneFunction.cpp CloneLoop.cpp \
	CloneModule.cpp CodeExtractor.cpp \
	DemoteRegToStack.cpp InlineFunction.cpp \
	InstructionNamer.cpp LCSSA.cpp Local.cpp LoopSimplify.cpp \
	LowerAllocations.cpp LowerInvoke.cpp LowerSwitch.cpp \
	Mem2Reg.cpp PromoteMemoryToRegister.cpp SSAUpdater.cpp SSI.cpp \
	SimplifyCFG.cpp UnifyFunctionExitNodes.cpp UnrollLoop.cpp \
	ValueMapper.cpp

TGHDRS=	Intrinsics

.include "../../clang.lib.mk"
