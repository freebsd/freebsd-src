# $FreeBSD$

.include <src.opts.mk>

LIB=	clangdynamicastmatchers

SRCDIR=	tools/clang/lib/ASTMatchers/Dynamic
SRCS=	Diagnostics.cpp \
	Registry.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	Parser.cpp \
	VariantValue.cpp
.endif

TGHDRS=	AttrList \
	Attrs \
	CommentCommandList \
	DeclNodes \
	DiagnosticCommonKinds \
	StmtNodes

.include "../clang.lib.mk"
