# $FreeBSD$

.include <bsd.own.mk>

LIB=	clangfrontend

SRCDIR=	tools/clang/lib/Frontend
SRCS=	ASTConsumers.cpp \
	ASTMerge.cpp \
	ASTUnit.cpp \
	CacheTokens.cpp \
	ChainedDiagnosticConsumer.cpp \
	ChainedIncludesSource.cpp \
	CompilerInstance.cpp \
	CompilerInvocation.cpp \
	CreateInvocationFromCommandLine.cpp \
	DependencyFile.cpp \
	DependencyGraph.cpp \
	DiagnosticRenderer.cpp \
	FrontendAction.cpp \
	FrontendActions.cpp \
	FrontendOptions.cpp \
	HeaderIncludeGen.cpp \
	InitHeaderSearch.cpp \
	InitPreprocessor.cpp \
	LangStandards.cpp \
	LayoutOverrideSource.cpp \
	LogDiagnosticPrinter.cpp \
	MultiplexConsumer.cpp \
	PrintPreprocessedOutput.cpp \
	SerializedDiagnosticPrinter.cpp \
	TextDiagnostic.cpp \
	TextDiagnosticBuffer.cpp \
	TextDiagnosticPrinter.cpp \
	VerifyDiagnosticConsumer.cpp \
	Warnings.cpp

TGHDRS=	AttrList \
	AttrParsedAttrList \
	Attrs \
	CommentCommandList \
	CommentNodes \
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
