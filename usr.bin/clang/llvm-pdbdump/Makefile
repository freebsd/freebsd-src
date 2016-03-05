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
	LinePrinter.cpp \
	TypeDumper.cpp \
	TypedefDumper.cpp \
	VariableDumper.cpp \
	llvm-pdbdump.cpp

LIBDEPS=llvmdebuginfopdb \
	llvmobject \
	llvmmcparser \
	llvmmc \
	llvmbitreader \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
