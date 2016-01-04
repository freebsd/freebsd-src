# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbPluginLanguageObjC

SRCDIR=	tools/lldb/source/Plugins/Language/ObjC/
SRCS=	CF.cpp \
	Cocoa.cpp \
	CoreMedia.cpp \
	NSArray.cpp \
	NSDictionary.cpp \
	NSError.cpp \
	NSException.cpp \
	NSIndexPath.cpp \
	NSSet.cpp \
	NSString.cpp \
	ObjCLanguage.cpp

TGHDRS=	AttrList \
	Attrs \
	CommentCommandList \
	DeclNodes \
	DiagnosticCommonKinds \
	StmtNodes

.include "../lldb.lib.mk"
