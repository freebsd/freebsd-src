# $FreeBSD$

LIB=	clangstaticanalyzercheckers

SRCDIR=	tools/clang/lib/StaticAnalyzer/Checkers
SRCS=	AdjustedReturnValueChecker.cpp \
	AnalyzerStatsChecker.cpp \
	ArrayBoundChecker.cpp \
	ArrayBoundCheckerV2.cpp \
	AttrNonNullChecker.cpp \
	BasicObjCFoundationChecks.cpp \
	BuiltinFunctionChecker.cpp \
	CStringChecker.cpp \
	CallAndMessageChecker.cpp \
	CastSizeChecker.cpp \
	CastToStructChecker.cpp \
	CheckObjCDealloc.cpp \
	CheckObjCInstMethSignature.cpp \
	CheckSecuritySyntaxOnly.cpp \
	CheckSizeofPointer.cpp \
	ChrootChecker.cpp \
	ClangSACheckerProvider.cpp \
	DeadStoresChecker.cpp \
	DebugCheckers.cpp \
	DereferenceChecker.cpp \
	DivZeroChecker.cpp \
	FixedAddressChecker.cpp \
	IdempotentOperationChecker.cpp \
	IteratorsChecker.cpp \
	LLVMConventionsChecker.cpp \
	MacOSXAPIChecker.cpp \
	MallocChecker.cpp \
	NSAutoreleasePoolChecker.cpp \
	NSErrorChecker.cpp \
	NoReturnFunctionChecker.cpp \
	OSAtomicChecker.cpp \
	ObjCAtSyncChecker.cpp \
	ObjCSelfInitChecker.cpp \
	ObjCUnusedIVarsChecker.cpp \
	PointerArithChecker.cpp \
	PointerSubChecker.cpp \
	PthreadLockChecker.cpp \
	ReturnPointerRangeChecker.cpp \
	ReturnUndefChecker.cpp \
	StackAddrEscapeChecker.cpp \
	StreamChecker.cpp \
	UndefBranchChecker.cpp \
	UndefCapturedBlockVarChecker.cpp \
	UndefResultChecker.cpp \
	UndefinedArraySubscriptChecker.cpp \
	UndefinedAssignmentChecker.cpp \
	UnixAPIChecker.cpp \
	UnreachableCodeChecker.cpp \
	VLASizeChecker.cpp

TGHDRS=	AttrList \
	Attrs \
	Checkers \
	DeclNodes \
	DiagnosticCommonKinds \
	StmtNodes

.include "../clang.lib.mk"
