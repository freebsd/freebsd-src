# $FreeBSD$

LIB=	clanganalysis

SRCDIR=	tools/clang/lib/Analysis
SRCS=	BasicConstraintManager.cpp BasicObjCFoundationChecks.cpp \
	BasicStore.cpp BasicValueFactory.cpp BugReporter.cpp \
	CFRefCount.cpp CheckDeadStores.cpp CheckNSError.cpp \
	CheckObjCDealloc.cpp CheckObjCInstMethSignature.cpp \
	CheckObjCUnusedIVars.cpp Environment.cpp ExplodedGraph.cpp \
	GRBlockCounter.cpp GRCoreEngine.cpp GRExprEngine.cpp \
	GRExprEngineInternalChecks.cpp GRSimpleVals.cpp GRState.cpp \
	GRTransferFuncs.cpp LiveVariables.cpp MemRegion.cpp \
	PathDiagnostic.cpp RangeConstraintManager.cpp RegionStore.cpp \
	SVals.cpp SimpleConstraintManager.cpp Store.cpp \
	SymbolManager.cpp UninitializedValues.cpp

TGHDRS=	DiagnosticAnalysisKinds DiagnosticCommonKinds

.include "../../clang.lib.mk"
