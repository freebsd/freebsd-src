# $FreeBSD$

.include <src.opts.mk>

LIB=	clanganalysis

SRCDIR=	tools/clang/lib/Analysis
SRCS=	AnalysisDeclContext.cpp \
	BodyFarm.cpp \
	CFG.cpp \
	CFGReachabilityAnalysis.cpp \
	CFGStmtMap.cpp \
	CallGraph.cpp \
	CocoaConventions.cpp \
	CodeInjector.cpp \
	Consumed.cpp \
	Dominators.cpp \
	FormatString.cpp \
	LiveVariables.cpp \
	ObjCNoReturn.cpp \
	PostOrderCFGView.cpp \
	PrintfFormatString.cpp \
	ProgramPoint.cpp \
	PseudoConstantAnalysis.cpp \
	ReachableCode.cpp \
	ScanfFormatString.cpp \
	ThreadSafety.cpp \
	ThreadSafetyCommon.cpp \
	ThreadSafetyTIL.cpp \
	UninitializedValues.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	ThreadSafetyLogical.cpp
.endif

TGHDRS=	AttrList \
	AttrVisitor \
	Attrs \
	CommentCommandList \
	CommentNodes \
	DeclNodes \
	DiagnosticAnalysisKinds \
	DiagnosticCommonKinds \
	StmtNodes

.include "../clang.lib.mk"
