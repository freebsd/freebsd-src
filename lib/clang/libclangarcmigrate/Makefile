# $FreeBSD$

.include <bsd.own.mk>

LIB=	clangarcmigrate

SRCDIR=	tools/clang/lib/ARCMigrate
SRCS=	ARCMT.cpp \
	ARCMTActions.cpp \
	FileRemapper.cpp \
	ObjCMT.cpp \
	PlistReporter.cpp \
	TransAPIUses.cpp \
	TransARCAssign.cpp \
	TransAutoreleasePool.cpp \
	TransBlockObjCVariable.cpp \
	TransEmptyStatementsAndDealloc.cpp \
	TransformActions.cpp \
	Transforms.cpp \
	TransGCAttrs.cpp \
	TransGCCalls.cpp \
	TransProperties.cpp \
	TransRetainReleaseDealloc.cpp \
	TransUnbridgedCasts.cpp \
	TransUnusedInitDelegate.cpp \
	TransZeroOutPropsInDealloc.cpp

TGHDRS=	AttrList \
	AttrParsedAttrList \
	Attrs \
	CommentNodes \
	DeclNodes \
	DiagnosticCommonKinds \
	DiagnosticGroups \
	DiagnosticSemaKinds \
	StmtNodes

.include "../clang.lib.mk"
