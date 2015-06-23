# $FreeBSD$

.include <bsd.own.mk>

LIB=	clangbasic

SRCDIR=	tools/clang/lib/Basic
SRCS=	Attributes.cpp \
	Builtins.cpp \
	CharInfo.cpp \
	Diagnostic.cpp \
	DiagnosticIDs.cpp \
	DiagnosticOptions.cpp \
	FileManager.cpp \
	FileSystemStatCache.cpp \
	IdentifierTable.cpp \
	LangOptions.cpp \
	Module.cpp \
	ObjCRuntime.cpp \
	OpenMPKinds.cpp \
	OperatorPrecedence.cpp \
	SanitizerBlacklist.cpp \
	Sanitizers.cpp \
	SourceLocation.cpp \
	SourceManager.cpp \
	TargetInfo.cpp \
	Targets.cpp \
	TokenKinds.cpp \
	Version.cpp \
	VersionTuple.cpp \
	VirtualFileSystem.cpp \
	Warnings.cpp

TGHDRS=	AttrHasAttributeImpl \
	DiagnosticAnalysisKinds \
	DiagnosticASTKinds \
	DiagnosticCommentKinds \
	DiagnosticCommonKinds \
	DiagnosticDriverKinds \
	DiagnosticFrontendKinds \
	DiagnosticGroups \
	DiagnosticIndexName \
	DiagnosticLexKinds \
	DiagnosticParseKinds \
	DiagnosticSemaKinds \
	DiagnosticSerializationKinds \
	arm_neon

.include "../clang.lib.mk"
