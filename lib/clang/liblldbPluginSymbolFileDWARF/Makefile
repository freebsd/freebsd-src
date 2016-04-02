# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbPluginSymbolFileDWARF

SRCDIR=	tools/lldb/source/Plugins/SymbolFile/DWARF
SRCS=	DIERef.cpp \
	DWARFAbbreviationDeclaration.cpp \
	DWARFASTParserClang.cpp \
	DWARFASTParserGo.cpp \
	DWARFAttribute.cpp \
	DWARFCompileUnit.cpp \
	DWARFDataExtractor.cpp \
	DWARFDebugAbbrev.cpp \
	DWARFDebugAranges.cpp \
	DWARFDebugArangeSet.cpp \
	DWARFDebugInfo.cpp \
	DWARFDebugInfoEntry.cpp \
	DWARFDebugMacro.cpp \
	DWARFDebugLine.cpp \
	DWARFDebugMacinfo.cpp \
	DWARFDebugMacinfoEntry.cpp \
	DWARFDebugPubnames.cpp \
	DWARFDebugPubnamesSet.cpp \
	DWARFDebugRanges.cpp \
	DWARFDeclContext.cpp \
	DWARFDefines.cpp \
	DWARFDIE.cpp \
	DWARFDIECollection.cpp \
	DWARFFormValue.cpp \
	LogChannelDWARF.cpp \
	HashedNameToDIE.cpp \
	NameToDIE.cpp \
	SymbolFileDWARF.cpp \
	SymbolFileDWARFDwo.cpp \
	SymbolFileDWARFDebugMap.cpp \
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
