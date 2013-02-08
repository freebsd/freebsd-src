# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmanalysis

SRCDIR=	lib/Analysis
SRCS=	AliasAnalysis.cpp \
	AliasAnalysisCounter.cpp \
	AliasAnalysisEvaluator.cpp \
	AliasDebugger.cpp \
	AliasSetTracker.cpp \
	Analysis.cpp \
	BasicAliasAnalysis.cpp \
	BlockFrequencyInfo.cpp \
	BranchProbabilityInfo.cpp \
	CFGPrinter.cpp \
	CaptureTracking.cpp \
	CodeMetrics.cpp \
	ConstantFolding.cpp \
	CostModel.cpp \
	DbgInfoPrinter.cpp \
	DependenceAnalysis.cpp \
	DomPrinter.cpp \
	DominanceFrontier.cpp \
	IVUsers.cpp \
	InlineCost.cpp \
	InstCount.cpp \
	InstructionSimplify.cpp \
	Interval.cpp \
	IntervalPartition.cpp \
	LazyValueInfo.cpp \
	LibCallAliasAnalysis.cpp \
	LibCallSemantics.cpp \
	Lint.cpp \
	Loads.cpp \
	LoopInfo.cpp \
	LoopPass.cpp \
	MemDepPrinter.cpp \
	MemoryBuiltins.cpp \
	MemoryDependenceAnalysis.cpp \
	ModuleDebugInfoPrinter.cpp \
	NoAliasAnalysis.cpp \
	PHITransAddr.cpp \
	PathNumbering.cpp \
	PathProfileInfo.cpp \
	PathProfileVerifier.cpp \
	PostDominators.cpp \
	ProfileEstimatorPass.cpp \
	ProfileInfo.cpp \
	ProfileInfoLoader.cpp \
	ProfileInfoLoaderPass.cpp \
	ProfileVerifierPass.cpp \
	ProfileDataLoader.cpp \
	ProfileDataLoaderPass.cpp \
	RegionInfo.cpp \
	RegionPass.cpp \
	RegionPrinter.cpp \
	ScalarEvolution.cpp \
	ScalarEvolutionAliasAnalysis.cpp \
	ScalarEvolutionExpander.cpp \
	ScalarEvolutionNormalization.cpp \
	SparsePropagation.cpp \
	Trace.cpp \
	TypeBasedAliasAnalysis.cpp \
	ValueTracking.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	BlockFrequencyInfo.cpp \
	LibCallSemantics.cpp \
	PathNumbering.cpp \
	PathProfileInfo.cpp \
	PathProfileVerifier.cpp
.endif

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
