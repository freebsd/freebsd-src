# $FreeBSD$

LIB=	clangfrontend

SRCDIR=	tools/clang/lib/Frontend
SRCS=	ASTConsumers.cpp \
	ASTMerge.cpp \
	ASTUnit.cpp \
	CacheTokens.cpp \
	CompilerInstance.cpp \
	CompilerInvocation.cpp \
	CreateInvocationFromCommandLine.cpp \
	DependencyFile.cpp \
	FrontendAction.cpp \
	FrontendActions.cpp \
	FrontendOptions.cpp \
	HeaderIncludeGen.cpp \
	InitHeaderSearch.cpp \
	InitPreprocessor.cpp \
	LangStandards.cpp \
	LogDiagnosticPrinter.cpp \
	MultiplexConsumer.cpp \
	PrintPreprocessedOutput.cpp \
	TextDiagnosticBuffer.cpp \
	TextDiagnosticPrinter.cpp \
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
	Options \
	StmtNodes

.include "../clang.lib.mk"
