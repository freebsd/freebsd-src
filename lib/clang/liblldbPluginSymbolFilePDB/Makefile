# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbPluginSymbolFilePDB

SRCDIR=	tools/lldb/source/Plugins/SymbolFile/PDB
SRCS=	PDBASTParser.cpp \
	SymbolFilePDB.cpp

TGHDRS=	AttrList \
	Attrs \
	CommentCommandList \
	DeclNodes \
	DiagnosticCommonKinds \
	StmtNodes

.include "../lldb.lib.mk"
