# $FreeBSD$

LIB=	clanganalysis

SRCDIR=	tools/clang/lib/Analysis
SRCS=	AnalysisContext.cpp ArrayBoundChecker.cpp AttrNonNullChecker.cpp \
	BadCallChecker.cpp BasicConstraintManager.cpp \
	BasicObjCFoundationChecks.cpp BasicStore.cpp BasicValueFactory.cpp \
	BugReporter.cpp BugReporterVisitors.cpp CastToStructChecker.cpp \
	CFG.cpp CFRefCount.cpp CallGraph.cpp CallInliner.cpp \
	CheckDeadStores.cpp \
	CheckObjCDealloc.cpp CheckObjCInstMethSignature.cpp \
	CheckObjCUnusedIVars.cpp CheckSecuritySyntaxOnly.cpp \
	CheckSizeofPointer.cpp \
	DereferenceChecker.cpp DivZeroChecker.cpp Environment.cpp \
	ExplodedGraph.cpp FixedAddressChecker.cpp GRBlockCounter.cpp \
	GRCoreEngine.cpp GRExprEngine.cpp \
	GRExprEngineExperimentalChecks.cpp GRExprEngineInternalChecks.cpp \
	GRState.cpp LiveVariables.cpp MallocChecker.cpp \
	ManagerRegistry.cpp MemRegion.cpp NSAutoreleasePoolChecker.cpp \
	NSErrorChecker.cpp PathDiagnostic.cpp PointerArithChecker.cpp \
	PointerSubChecker.cpp PthreadLockChecker.cpp \
	ReturnPointerRangeChecker.cpp RangeConstraintManager.cpp RegionStore.cpp \
	ReturnStackAddressChecker.cpp ReturnUndefChecker.cpp SVals.cpp \
	SValuator.cpp \
	SimpleConstraintManager.cpp SimpleSValuator.cpp Store.cpp \
	SymbolManager.cpp UndefinedArgChecker.cpp \
	UndefinedArraySubscriptChecker.cpp \
	UndefinedAssignmentChecker.cpp UninitializedValues.cpp \
	ValueManager.cpp VLASizeChecker.cpp

TGHDRS=	DiagnosticAnalysisKinds DiagnosticCommonKinds

.include "../../clang.lib.mk"
