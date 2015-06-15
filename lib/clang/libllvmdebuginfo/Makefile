# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmdebuginfo

SRCDIR=	lib/DebugInfo
SRCS=	DIContext.cpp \
	DWARFAbbreviationDeclaration.cpp \
	DWARFCompileUnit.cpp \
	DWARFContext.cpp \
	DWARFDebugAbbrev.cpp \
	DWARFDebugArangeSet.cpp \
	DWARFDebugAranges.cpp \
	DWARFDebugFrame.cpp \
	DWARFDebugInfoEntry.cpp \
	DWARFDebugLine.cpp \
	DWARFDebugLoc.cpp \
	DWARFDebugRangeList.cpp \
	DWARFFormValue.cpp \
	DWARFTypeUnit.cpp \
	DWARFUnit.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	DWARFAcceleratorTable.cpp \
	SyntaxHighlighting.cpp
.endif

.include "../clang.lib.mk"
