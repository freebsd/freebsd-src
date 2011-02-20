# $FreeBSD$

LIB=	clanganalysis

SRCDIR=	tools/clang/lib/Analysis
SRCS=	AnalysisContext.cpp \
	CFG.cpp \
	CFGStmtMap.cpp \
	CocoaConventions.cpp \
	FormatString.cpp \
	LiveVariables.cpp \
	PrintfFormatString.cpp \
	PseudoConstantAnalysis.cpp \
	ReachableCode.cpp \
	ScanfFormatString.cpp \
	UninitializedValues.cpp \
	UninitializedValuesV2.cpp

TGHDRS=	AttrList \
	Attrs \
	DeclNodes \
	DiagnosticAnalysisKinds \
	DiagnosticCommonKinds \
	StmtNodes

.include "../clang.lib.mk"
