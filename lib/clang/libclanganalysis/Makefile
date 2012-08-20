# $FreeBSD$

LIB=	clanganalysis

SRCDIR=	tools/clang/lib/Analysis
SRCS=	AnalysisDeclContext.cpp \
	CallGraph.cpp \
	CFG.cpp \
	CFGReachabilityAnalysis.cpp \
	CFGStmtMap.cpp \
	CocoaConventions.cpp \
	Dominators.cpp \
	FormatString.cpp \
	LiveVariables.cpp \
	PostOrderCFGView.cpp \
	PrintfFormatString.cpp \
	ProgramPoint.cpp \
	PseudoConstantAnalysis.cpp \
	ReachableCode.cpp \
	ScanfFormatString.cpp \
	ThreadSafety.cpp \
	UninitializedValues.cpp

TGHDRS=	AttrList \
	Attrs \
	CommentNodes \
	DeclNodes \
	DiagnosticAnalysisKinds \
	DiagnosticCommonKinds \
	StmtNodes

.include "../clang.lib.mk"
