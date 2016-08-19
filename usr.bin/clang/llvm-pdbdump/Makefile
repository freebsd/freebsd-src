# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=llvm-pdbdump

SRCDIR=	tools/llvm-pdbdump
SRCS=	BuiltinDumper.cpp \
	ClassDefinitionDumper.cpp \
	CompilandDumper.cpp \
	EnumDumper.cpp \
	ExternalSymbolDumper.cpp \
	FunctionDumper.cpp \
	LLVMOutputStyle.cpp \
	LinePrinter.cpp \
	PdbYaml.cpp \
	TypeDumper.cpp \
	TypedefDumper.cpp \
	VariableDumper.cpp \
	YAMLOutputStyle.cpp \
	llvm-pdbdump.cpp

LIBDEPS=llvmdebuginfopdb \
	llvmobject \
	llvmdebuginfocodeview \
	llvmbitreader \
	llvmcore \
	llvmmcparser \
	llvmmc \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
