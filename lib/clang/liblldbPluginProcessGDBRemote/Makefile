# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbPluginProcessGDBRemote

SRCDIR=	tools/lldb/source/Plugins/Process/gdb-remote
SRCS=	GDBRemoteCommunication.cpp \
	GDBRemoteCommunicationClient.cpp \
	GDBRemoteCommunicationServer.cpp \
	GDBRemoteCommunicationServerCommon.cpp \
	GDBRemoteCommunicationServerLLGS.cpp \
	GDBRemoteCommunicationServerPlatform.cpp \
	GDBRemoteRegisterContext.cpp \
	ProcessGDBRemote.cpp \
	ProcessGDBRemoteLog.cpp \
	ThreadGDBRemote.cpp

TGHDRS=	AttrList \
	Attrs \
	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
