# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbPluginCPlusPlusLanguage

SRCDIR=	tools/lldb/source/Plugins/Language/CPlusPlus
SRCS=	BlockPointer.cpp \
	CPlusPlusLanguage.cpp \
	CxxStringTypes.cpp \
	LibCxx.cpp \
	LibCxxAtomic.cpp \
	LibCxxInitializerList.cpp \
	LibCxxList.cpp \
	LibCxxMap.cpp \
	LibCxxUnorderedMap.cpp \
	LibCxxVector.cpp \
	LibStdcpp.cpp

TGHDRS=	AttrList \
	Attrs \
	CommentCommandList \
	DeclNodes \
	DiagnosticCommonKinds \
	StmtNodes

.include "../lldb.lib.mk"
