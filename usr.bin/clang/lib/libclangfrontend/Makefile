# $FreeBSD$

LIB=	clangfrontend

SRCDIR=	tools/clang/lib/Frontend
SRCS=	ASTConsumers.cpp ASTUnit.cpp AnalysisConsumer.cpp Backend.cpp \
	CacheTokens.cpp CompilerInstance.cpp CompilerInvocation.cpp \
	DeclXML.cpp DependencyFile.cpp DiagChecker.cpp \
	DocumentXML.cpp FixItRewriter.cpp FrontendAction.cpp \
	FrontendActions.cpp FrontendOptions.cpp GeneratePCH.cpp \
	HTMLDiagnostics.cpp HTMLPrint.cpp InitHeaderSearch.cpp \
	InitPreprocessor.cpp PCHReader.cpp \
	PCHReaderDecl.cpp PCHReaderStmt.cpp PCHWriter.cpp \
	PCHWriterDecl.cpp PCHWriterStmt.cpp PlistDiagnostics.cpp \
	PrintParserCallbacks.cpp PrintPreprocessedOutput.cpp \
	RewriteBlocks.cpp RewriteMacros.cpp RewriteObjC.cpp \
	RewriteTest.cpp StmtXML.cpp TextDiagnosticBuffer.cpp \
	TextDiagnosticPrinter.cpp TypeXML.cpp VerifyDiagnosticsClient.cpp \
	Warnings.cpp

TGHDRS=	DiagnosticCommonKinds DiagnosticFrontendKinds \
	DiagnosticLexKinds DiagnosticSemaKinds

.include "../../clang.lib.mk"
