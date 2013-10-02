# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbPluginInstructionARM

SRCDIR=	tools/lldb/source/Plugins/Instruction/ARM
SRCS=	EmulateInstructionARM.cpp \
	EmulationStateARM.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
