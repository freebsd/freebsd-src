# $FreeBSD$

LIB=	clangfrontend

SRCDIR=	tools/clang/lib/Frontend
SRCS=	ASTConsumers.cpp ASTMerge.cpp ASTUnit.cpp AnalysisConsumer.cpp \
	BoostConAction.cpp CacheTokens.cpp CodeGenAction.cpp \
	CompilerInstance.cpp CompilerInvocation.cpp DeclXML.cpp \
	DependencyFile.cpp DiagChecker.cpp DocumentXML.cpp \
	FixItRewriter.cpp FrontendAction.cpp FrontendActions.cpp \
	FrontendOptions.cpp GeneratePCH.cpp HTMLDiagnostics.cpp \
	HTMLPrint.cpp InitHeaderSearch.cpp InitPreprocessor.cpp \
	LangStandards.cpp PCHReader.cpp PCHReaderDecl.cpp \
	PCHReaderStmt.cpp PCHWriter.cpp PCHWriterDecl.cpp \
	PCHWriterStmt.cpp PlistDiagnostics.cpp \
	PrintParserCallbacks.cpp PrintPreprocessedOutput.cpp \
	RewriteMacros.cpp RewriteObjC.cpp RewriteTest.cpp StmtXML.cpp \
	TextDiagnosticBuffer.cpp TextDiagnosticPrinter.cpp TypeXML.cpp \
	VerifyDiagnosticsClient.cpp Warnings.cpp

TGHDRS=	CC1Options DiagnosticASTKinds DiagnosticCommonKinds \
	DiagnosticDriverKinds DiagnosticFrontendKinds \
	DiagnosticLexKinds DiagnosticSemaKinds StmtNodes

.include "../clang.lib.mk"
