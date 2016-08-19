# $FreeBSD$

.include <bsd.own.mk>

LIB=	clangindex

SRCDIR=	tools/clang/lib/Index
SRCS=	CodegenNameGenerator.cpp \
	CommentToXML.cpp \
	IndexBody.cpp \
	IndexDecl.cpp \
	IndexSymbol.cpp \
	IndexTypeSourceInfo.cpp \
	IndexingAction.cpp \
	IndexingContext.cpp \
	USRGeneration.cpp

TGHDRS=	AttrList \
	AttrVisitor \
	Attrs \
	CommentCommandList \
	CommentNodes \
	DeclNodes \
	DiagnosticCommonKinds \
	StmtNodes

.include "../clang.lib.mk"
