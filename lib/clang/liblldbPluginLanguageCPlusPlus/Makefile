# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbPluginLanguageCPlusPlus

SRCDIR=	tools/lldb/source/Plugins/Language/CPlusPlus/
SRCS=	CPlusPlusLanguage.cpp \
	CxxStringTypes.cpp \
	LibCxx.cpp \
	LibCxxInitializerList.cpp \
	LibCxxList.cpp \
	LibCxxMap.cpp \
	LibCxxUnorderedMap.cpp \
	LibCxxVector.cpp \
	LibStdcpp.cpp

TGHDRS=	CommentCommandList \
	DeclNodes \
	DiagnosticCommonKinds \
	StmtNodes

.include "../lldb.lib.mk"
