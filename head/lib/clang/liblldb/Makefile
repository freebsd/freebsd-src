# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldb

CFLAGS+=-I${.CURDIR}/../../../contrib/llvm/tools/lldb/source/Plugins/Process/POSIX

SRCDIR=	tools/lldb/source
SRCS=	lldb.cpp \
	lldb-log.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
