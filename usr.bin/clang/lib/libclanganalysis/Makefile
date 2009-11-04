# $FreeBSD$

LIB=	clanganalysis

SRCDIR=	tools/clang/lib/Analysis
SRCS=	AnalysisContext.cpp AnalysisManager.cpp AttrNonNullChecker.cpp \
	BadCallChecker.cpp BasicConstraintManager.cpp \
	BasicObjCFoundationChecks.cpp BasicStore.cpp BasicValueFactory.cpp \
	BugReporter.cpp BugReporterVisitors.cpp CFG.cpp CFRefCount.cpp \
	CallGraph.cpp CallInliner.cpp  CheckDeadStores.cpp \
	CheckObjCDealloc.cpp CheckObjCInstMethSignature.cpp \
	CheckObjCUnusedIVars.cpp CheckSecuritySyntaxOnly.cpp DereferenceChecker.cpp DivZeroChecker.cpp Environment.cpp \
	ExplodedGraph.cpp GRBlockCounter.cpp GRCoreEngine.cpp GRExprEngine.cpp \
	GRExprEngineInternalChecks.cpp GRState.cpp LiveVariables.cpp \
	MemRegion.cpp NSAutoreleasePoolChecker.cpp NSErrorChecker.cpp PathDiagnostic.cpp \
	RangeConstraintManager.cpp RegionStore.cpp SVals.cpp SValuator.cpp \
	SimpleConstraintManager.cpp SimpleSValuator.cpp Store.cpp \
	SymbolManager.cpp UndefinedArgChecker.cpp \
	UndefinedAssignmentChecker.cpp UninitializedValues.cpp \
	ValueManager.cpp VLASizeChecker.cpp

TGHDRS=	DiagnosticAnalysisKinds DiagnosticCommonKinds

.include "../../clang.lib.mk"
