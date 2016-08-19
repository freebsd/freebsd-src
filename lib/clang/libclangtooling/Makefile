# $FreeBSD$

.include <src.opts.mk>

LIB=	clangtooling

SRCDIR=	tools/clang/lib/Tooling
SRCS=	ArgumentsAdjusters.cpp \
	CommonOptionsParser.cpp \
	CompilationDatabase.cpp \
	FileMatchTrie.cpp \
	JSONCompilationDatabase.cpp \
	Refactoring.cpp \
	RefactoringCallbacks.cpp \
	Tooling.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	FixIt.cpp
.endif

TGHDRS=	AttrList \
	Attrs \
	CommentCommandList \
	DeclNodes \
	DiagnosticCommonKinds \
	DiagnosticDriverKinds \
	DiagnosticFrontendKinds \
	StmtNodes

.include "../clang.lib.mk"
