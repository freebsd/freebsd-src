# $FreeBSD$

LIB=	llvmanalysis

SRCDIR=	lib/Analysis
SRCS=	AliasAnalysis.cpp AliasAnalysisCounter.cpp \
	AliasAnalysisEvaluator.cpp AliasDebugger.cpp \
	AliasSetTracker.cpp Analysis.cpp BasicAliasAnalysis.cpp \
	CFGPrinter.cpp CaptureTracking.cpp ConstantFolding.cpp \
	DbgInfoPrinter.cpp DebugInfo.cpp IVUsers.cpp InlineCost.cpp \
	InstructionSimplify.cpp \
	InstCount.cpp Interval.cpp IntervalPartition.cpp LazyValueInfo.cpp \
	LibCallAliasAnalysis.cpp LibCallSemantics.cpp Lint.cpp LiveValues.cpp \
	LoopDependenceAnalysis.cpp LoopInfo.cpp LoopPass.cpp \
	MemoryBuiltins.cpp MemoryDependenceAnalysis.cpp PHITransAddr.cpp \
	PointerTracking.cpp \
	PostDominators.cpp ProfileEstimatorPass.cpp ProfileInfo.cpp \
	ProfileInfoLoader.cpp ProfileInfoLoaderPass.cpp \
	ProfileVerifierPass.cpp ScalarEvolution.cpp \
	ScalarEvolutionAliasAnalysis.cpp ScalarEvolutionExpander.cpp \
	ScalarEvolutionNormalization.cpp SparsePropagation.cpp Trace.cpp \
	ValueTracking.cpp

TGHDRS=	Intrinsics

.include "../../clang.lib.mk"
