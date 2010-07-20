# $FreeBSD$

LIB=	clanganalysis

SRCDIR=	tools/clang/lib/Analysis
SRCS=	AnalysisContext.cpp \
	CFG.cpp \
	LiveVariables.cpp \
	PrintfFormatString.cpp \
	ReachableCode.cpp \
	UninitializedValues.cpp

TGHDRS=	AttrList \
	Attrs \
	DeclNodes \
	DiagnosticAnalysisKinds \
	DiagnosticCommonKinds \
	StmtNodes

.include "../clang.lib.mk"
