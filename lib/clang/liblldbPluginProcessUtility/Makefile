# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbPluginProcessUtility

SRCDIR=	tools/lldb/source/Plugins/Process/Utility
SRCS=	DynamicRegisterInfo.cpp \
	InferiorCallPOSIX.cpp \
	RegisterContextDarwin_arm.cpp \
	RegisterContextDarwin_i386.cpp \
	RegisterContextDarwin_x86_64.cpp \
	RegisterContextDummy.cpp \
	RegisterContextLLDB.cpp \
	RegisterContextMach_arm.cpp \
	RegisterContextMach_i386.cpp \
	RegisterContextMach_x86_64.cpp \
	RegisterContextMacOSXFrameBackchain.cpp \
	RegisterContextMemory.cpp \
	RegisterContextThreadMemory.cpp \
	StopInfoMachException.cpp \
	ThreadMemory.cpp \
	UnwindLLDB.cpp \
	UnwindMacOSXFrameBackchain.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
