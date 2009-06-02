# $FreeBSD$

LIB=	llvmanalysis

SRCDIR=	lib/Analysis
SRCS=	AliasAnalysis.cpp AliasAnalysisCounter.cpp \
	AliasAnalysisEvaluator.cpp AliasDebugger.cpp \
	AliasSetTracker.cpp Analysis.cpp BasicAliasAnalysis.cpp \
	CFGPrinter.cpp CaptureTracking.cpp ConstantFolding.cpp \
	DbgInfoPrinter.cpp DebugInfo.cpp InstCount.cpp Interval.cpp \
	IntervalPartition.cpp IVUsers.cpp LibCallAliasAnalysis.cpp \
	LibCallSemantics.cpp LiveValues.cpp LoopInfo.cpp LoopPass.cpp \
	LoopVR.cpp MemoryDependenceAnalysis.cpp PostDominators.cpp \
	ProfileInfo.cpp ProfileInfoLoader.cpp \
	ProfileInfoLoaderPass.cpp ScalarEvolution.cpp \
	ScalarEvolutionExpander.cpp SparsePropagation.cpp Trace.cpp \
	ValueTracking.cpp

TGHDRS=	Intrinsics

.include "../../clang.lib.mk"
