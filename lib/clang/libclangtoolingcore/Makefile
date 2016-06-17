# $FreeBSD$

.include <bsd.own.mk>

LIB=	clangtoolingcore

SRCDIR=	tools/clang/lib/Tooling/Core
CFLAGS+= -I${LLVM_SRCS}/tools
SRCS=	Lookup.cpp \
	Replacement.cpp

TGHDRS=	AttrList \
	Attrs \
	CommentCommandList \
	DeclNodes \
	DiagnosticCommonKinds \
	StmtNodes

.include "../clang.lib.mk"
