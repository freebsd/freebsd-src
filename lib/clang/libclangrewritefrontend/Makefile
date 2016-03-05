# $FreeBSD$

.include <src.opts.mk>

LIB=	clangrewritefrontend

SRCDIR=	tools/clang/lib/Frontend/Rewrite
SRCS=	FixItRewriter.cpp \
	FrontendActions.cpp \
	HTMLPrint.cpp \
	InclusionRewriter.cpp \
	RewriteMacros.cpp \
	RewriteTest.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	RewriteModernObjC.cpp \
	RewriteObjC.cpp
.endif

TGHDRS=	AttrList \
	AttrParsedAttrList \
	Attrs \
	CommentCommandList \
	DeclNodes \
	DiagnosticCommonKinds \
	DiagnosticFrontendKinds \
	StmtNodes

.include "../clang.lib.mk"
