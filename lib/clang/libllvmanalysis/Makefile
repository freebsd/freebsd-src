# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmanalysis

SRCDIR=	lib/Analysis
SRCS=	AliasAnalysis.cpp \
	AliasAnalysisEvaluator.cpp \
	AliasAnalysisSummary.cpp \
	AliasSetTracker.cpp \
	AssumptionCache.cpp \
	BasicAliasAnalysis.cpp \
	BlockFrequencyInfo.cpp \
	BlockFrequencyInfoImpl.cpp \
	BranchProbabilityInfo.cpp \
	CFG.cpp \
	CFGPrinter.cpp \
	CFLAndersAliasAnalysis.cpp \
	CFLSteensAliasAnalysis.cpp \
	CallGraph.cpp \
	CallGraphSCCPass.cpp \
	CallPrinter.cpp \
	CaptureTracking.cpp \
	CodeMetrics.cpp \
	ConstantFolding.cpp \
	CostModel.cpp \
	Delinearization.cpp \
	DemandedBits.cpp \
	DependenceAnalysis.cpp \
	DivergenceAnalysis.cpp \
	DomPrinter.cpp \
	DominanceFrontier.cpp \
	EHPersonalities.cpp \
	CGSCCPassManager.cpp \
	GlobalsModRef.cpp \
	IVUsers.cpp \
	IndirectCallPromotionAnalysis.cpp \
	InlineCost.cpp \
	InstCount.cpp \
	InstructionSimplify.cpp \
	Interval.cpp \
	IntervalPartition.cpp \
	IteratedDominanceFrontier.cpp \
	LazyBlockFrequencyInfo.cpp \
	LazyCallGraph.cpp \
	LazyValueInfo.cpp \
	Lint.cpp \
	Loads.cpp \
	LoopAccessAnalysis.cpp \
	LoopInfo.cpp \
	LoopPass.cpp \
	LoopPassManager.cpp \
	LoopUnrollAnalyzer.cpp \
	MemDepPrinter.cpp \
	MemDerefPrinter.cpp \
	MemoryBuiltins.cpp \
	MemoryDependenceAnalysis.cpp \
	MemoryLocation.cpp \
	ModuleDebugInfoPrinter.cpp \
	ModuleSummaryAnalysis.cpp \
	ObjCARCAliasAnalysis.cpp \
	ObjCARCAnalysisUtils.cpp \
	ObjCARCInstKind.cpp \
	OptimizationDiagnosticInfo.cpp \
	OrderedBasicBlock.cpp \
	PHITransAddr.cpp \
	PostDominators.cpp \
	ProfileSummaryInfo.cpp \
	PtrUseVisitor.cpp \
	RegionInfo.cpp \
	RegionPass.cpp \
	RegionPrinter.cpp \
	ScalarEvolution.cpp \
	ScalarEvolutionAliasAnalysis.cpp \
	ScalarEvolutionExpander.cpp \
	ScalarEvolutionNormalization.cpp \
	ScopedNoAliasAA.cpp \
	SparsePropagation.cpp \
	TargetLibraryInfo.cpp \
	TargetTransformInfo.cpp \
	TypeBasedAliasAnalysis.cpp \
	TypeMetadataUtils.cpp \
	ValueTracking.cpp \
	VectorUtils.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	Analysis.cpp \
	Trace.cpp
.endif
	
TGHDRS=	Attributes \
	Intrinsics

.include "../clang.lib.mk"
