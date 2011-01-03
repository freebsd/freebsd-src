# $FreeBSD$

LIB=	clangfrontend

SRCDIR=	tools/clang/lib/Frontend
SRCS=	ASTConsumers.cpp \
	ASTMerge.cpp \
	ASTUnit.cpp \
	BoostConAction.cpp \
	CacheTokens.cpp \
	CompilerInstance.cpp \
	CompilerInvocation.cpp \
	DeclXML.cpp \
	DependencyFile.cpp \
	DiagChecker.cpp \
	DocumentXML.cpp \
	FrontendAction.cpp \
	FrontendActions.cpp \
	FrontendOptions.cpp \
	InitHeaderSearch.cpp \
	InitPreprocessor.cpp \
	LangStandards.cpp \
	PrintPreprocessedOutput.cpp \
	StmtXML.cpp \
	TextDiagnosticBuffer.cpp \
	TextDiagnosticPrinter.cpp \
	TypeXML.cpp \
	VerifyDiagnosticsClient.cpp \
	Warnings.cpp

TGHDRS=	AttrList \
	Attrs \
	CC1Options \
	DeclNodes \
	DiagnosticASTKinds \
	DiagnosticCommonKinds \
	DiagnosticDriverKinds \
	DiagnosticFrontendKinds \
	DiagnosticLexKinds \
	DiagnosticSemaKinds \
	StmtNodes

.include "../clang.lib.mk"
