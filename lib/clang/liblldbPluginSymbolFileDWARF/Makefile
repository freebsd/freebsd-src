# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbPluginSymbolFileDWARF

SRCDIR=	tools/lldb/source/Plugins/SymbolFile/DWARF
SRCS=	DIERef.cpp \
	DWARFASTParserClang.cpp \
	DWARFASTParserGo.cpp \
	DWARFAbbreviationDeclaration.cpp \
	DWARFAttribute.cpp \
	DWARFCompileUnit.cpp \
	DWARFDIE.cpp \
	DWARFDIECollection.cpp \
	DWARFDataExtractor.cpp \
	DWARFDebugAbbrev.cpp \
	DWARFDebugArangeSet.cpp \
	DWARFDebugAranges.cpp \
	DWARFDebugInfo.cpp \
	DWARFDebugInfoEntry.cpp \
	DWARFDebugLine.cpp \
	DWARFDebugMacinfo.cpp \
	DWARFDebugMacinfoEntry.cpp \
	DWARFDebugMacro.cpp \
	DWARFDebugPubnames.cpp \
	DWARFDebugPubnamesSet.cpp \
	DWARFDebugRanges.cpp \
	DWARFDeclContext.cpp \
	DWARFDefines.cpp \
	DWARFFormValue.cpp \
	HashedNameToDIE.cpp \
	LogChannelDWARF.cpp \
	NameToDIE.cpp \
	SymbolFileDWARF.cpp \
	SymbolFileDWARFDebugMap.cpp \
	SymbolFileDWARFDwo.cpp \
	UniqueDWARFASTType.cpp

TGHDRS=	AttrList \
	Attrs \
	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList \
	AttrParsedAttrList \
	DiagnosticFrontendKinds \
	Intrinsics

.include "../lldb.lib.mk"
