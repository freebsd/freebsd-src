# $FreeBSD$

.include <bsd.own.mk>

LIB=	lldbSymbol

SRCDIR=	tools/lldb/source/Symbol
SRCS=	ArmUnwindInfo.cpp \
	Block.cpp \
	ClangASTContext.cpp \
	ClangASTImporter.cpp \
	ClangExternalASTSourceCallbacks.cpp \
	ClangExternalASTSourceCommon.cpp \
	ClangUtil.cpp \
	CompactUnwindInfo.cpp \
	CompileUnit.cpp \
	CompilerDecl.cpp \
	CompilerDeclContext.cpp \
	CompilerType.cpp \
	DWARFCallFrameInfo.cpp \
	DebugMacros.cpp \
	Declaration.cpp \
	FuncUnwinders.cpp \
	Function.cpp \
	GoASTContext.cpp \
	LineEntry.cpp \
	LineTable.cpp \
	ObjectFile.cpp \
	Symbol.cpp \
	SymbolContext.cpp \
	SymbolFile.cpp \
	SymbolVendor.cpp \
	Symtab.cpp \
	Type.cpp \
	TypeList.cpp \
	TypeMap.cpp \
	TypeSystem.cpp \
	UnwindPlan.cpp \
	UnwindTable.cpp \
	Variable.cpp \
	VariableList.cpp \
	VerifyDecl.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList \
	AttrList \
	Attrs

.include "../lldb.lib.mk"
