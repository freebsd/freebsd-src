# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmtransformutils

SRCDIR=	lib/Transforms/Utils
SRCS=	AddrModeMatcher.cpp \
	BasicBlockUtils.cpp \
	BreakCriticalEdges.cpp \
	BuildLibCalls.cpp \
	CloneFunction.cpp \
	CloneModule.cpp \
	CmpInstAnalysis.cpp \
	CodeExtractor.cpp \
	DemoteRegToStack.cpp \
	InlineFunction.cpp \
	InstructionNamer.cpp \
	LCSSA.cpp \
	Local.cpp \
	LoopSimplify.cpp \
	LoopUnroll.cpp \
	LoopUnrollRuntime.cpp \
	LowerExpectIntrinsic.cpp \
	LowerInvoke.cpp \
	LowerSwitch.cpp \
	Mem2Reg.cpp \
	ModuleUtils.cpp \
	PromoteMemoryToRegister.cpp \
	SSAUpdater.cpp \
	SimplifyCFG.cpp \
	SimplifyIndVar.cpp \
	SimplifyInstructions.cpp \
	UnifyFunctionExitNodes.cpp \
	ValueMapper.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	SimplifyInstructions.cpp \
	Utils.cpp
.endif

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
