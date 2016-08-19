# $FreeBSD$

.include <src.opts.mk>

LIB=	clangstaticanalyzercheckers

SRCDIR=	tools/clang/lib/StaticAnalyzer/Checkers
SRCS=	AllocationDiagnostics.cpp \
	AnalyzerStatsChecker.cpp \
	ArrayBoundChecker.cpp \
	ArrayBoundCheckerV2.cpp \
	BasicObjCFoundationChecks.cpp \
	BoolAssignmentChecker.cpp \
	BuiltinFunctionChecker.cpp \
	CStringChecker.cpp \
	CStringSyntaxChecker.cpp \
	CallAndMessageChecker.cpp \
	CastSizeChecker.cpp \
	CastToStructChecker.cpp \
	CheckObjCDealloc.cpp \
	CheckObjCInstMethSignature.cpp \
	CheckSecuritySyntaxOnly.cpp \
	CheckSizeofPointer.cpp \
	ChrootChecker.cpp \
	ClangCheckers.cpp \
	DeadStoresChecker.cpp \
	DebugCheckers.cpp \
	DereferenceChecker.cpp \
	DirectIvarAssignment.cpp \
	DivZeroChecker.cpp \
	DynamicTypeChecker.cpp \
	DynamicTypePropagation.cpp \
	ExprInspectionChecker.cpp \
	FixedAddressChecker.cpp \
	GenericTaintChecker.cpp \
	IdenticalExprChecker.cpp \
	IvarInvalidationChecker.cpp \
	LLVMConventionsChecker.cpp \
	LocalizationChecker.cpp \
	MPI-Checker/MPIBugReporter.cpp \
	MPI-Checker/MPIChecker.cpp \
	MPI-Checker/MPIFunctionClassifier.cpp \
	MacOSKeychainAPIChecker.cpp \
	MacOSXAPIChecker.cpp \
	MallocChecker.cpp \
	MallocOverflowSecurityChecker.cpp \
	MallocSizeofChecker.cpp \
	NSAutoreleasePoolChecker.cpp \
	NSErrorChecker.cpp \
	NoReturnFunctionChecker.cpp \
	NonNullParamChecker.cpp \
	NullabilityChecker.cpp \
	ObjCAtSyncChecker.cpp \
	ObjCContainersASTChecker.cpp \
	ObjCContainersChecker.cpp \
	ObjCMissingSuperCallChecker.cpp \
	ObjCSelfInitChecker.cpp \
	ObjCSuperDeallocChecker.cpp \
	ObjCUnusedIVarsChecker.cpp \
	PaddingChecker.cpp \
	PointerArithChecker.cpp \
	PointerSubChecker.cpp \
	PthreadLockChecker.cpp \
	RetainCountChecker.cpp \
	ReturnPointerRangeChecker.cpp \
	ReturnUndefChecker.cpp \
	SimpleStreamChecker.cpp \
	StackAddrEscapeChecker.cpp \
	StreamChecker.cpp \
	TaintTesterChecker.cpp \
	TestAfterDivZeroChecker.cpp \
	TraversalChecker.cpp \
	UndefBranchChecker.cpp \
	UndefCapturedBlockVarChecker.cpp \
	UndefResultChecker.cpp \
	UndefinedArraySubscriptChecker.cpp \
	UndefinedAssignmentChecker.cpp \
	UnixAPIChecker.cpp \
	UnreachableCodeChecker.cpp \
	VLASizeChecker.cpp \
	VforkChecker.cpp \
	VirtualCallChecker.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	CheckerDocumentation.cpp
.endif

TGHDRS=	AttrList \
	AttrVisitor \
	Attrs \
	Checkers \
	CommentCommandList \
	CommentNodes \
	DeclNodes \
	DiagnosticCommonKinds \
	StmtNodes

.include "../clang.lib.mk"
