# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbPluginDynamicLoaderPosixDYLD

SRCDIR=	tools/lldb/source/Plugins/DynamicLoader/POSIX-DYLD
SRCS=	AuxVector.cpp \
	DYLDRendezvous.cpp \
	DynamicLoaderPOSIXDYLD.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
