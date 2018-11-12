# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-symbolizer

SRCDIR=	tools/llvm-symbolizer
SRCS=	llvm-symbolizer.cpp \
	LLVMSymbolize.cpp

LIBDEPS=llvmdebuginfo \
	llvmobject \
	llvmmcparser \
	llvmmc \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"

DPADD+= ${LIBZ}
LDADD+= -lz
