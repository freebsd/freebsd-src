# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbPluginProcessElfCore

CFLAGS+=-I${.CURDIR}/../../../contrib/llvm/tools/lldb/source/Plugins/Process/POSIX
CFLAGS+=-I${.CURDIR}/../../../contrib/llvm/tools/lldb/source/Plugins/Process/Utility

SRCDIR=	tools/lldb/source/Plugins/Process/elf-core
SRCS=	ProcessElfCore.cpp \
	ThreadElfCore.cpp \
	RegisterContextPOSIXCore_mips64.cpp \
	RegisterContextPOSIXCore_x86_64.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
