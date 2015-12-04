# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-symbolizer

SRCDIR=	tools/llvm-symbolizer
SRCS=	llvm-symbolizer.cpp \
	LLVMSymbolize.cpp

LIBDEPS=llvmdebuginfopdb \
	llvmdebuginfodwarf \
	llvmobject \
	llvmmcparser \
	llvmmc \
	llvmbitreader \
	llvmcore \
	llvmsupport

LIBADD=	z

.include "../clang.prog.mk"
