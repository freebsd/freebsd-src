# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbHostFreeBSD

SRCDIR=	tools/lldb/source/Host/freebsd
SRCS=	Host.cpp \
	HostInfoFreeBSD.cpp \
	HostThreadFreeBSD.cpp \
	ThisThread.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
