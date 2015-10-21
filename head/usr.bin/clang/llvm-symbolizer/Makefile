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

.include "../clang.prog.mk"

DPADD+= ${LIBZ}
LDADD+= -lz
