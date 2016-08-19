# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmdebuginfocodeview

SRCDIR=	lib/DebugInfo/CodeView
SRCS=	ByteStream.cpp \
	CVTypeVisitor.cpp \
	CodeViewError.cpp \
	FieldListRecordBuilder.cpp \
	Line.cpp \
	ListRecordBuilder.cpp \
	MemoryTypeTableBuilder.cpp \
	ModuleSubstream.cpp \
	ModuleSubstreamVisitor.cpp \
	RecordSerialization.cpp \
	StreamReader.cpp \
	StreamWriter.cpp \
	SymbolDumper.cpp \
	TypeDumper.cpp \
	TypeRecord.cpp \
	TypeRecordBuilder.cpp \
	TypeStreamMerger.cpp \
	TypeTableBuilder.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	EnumTables.cpp \
	MethodListRecordBuilder.cpp
.endif

.include "../clang.lib.mk"
