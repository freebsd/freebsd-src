# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-symbolizer

SRCDIR=	tools/llvm-symbolizer
SRCS=	llvm-symbolizer.cpp

LIBDEPS=llvmsymbolize \
	llvmdebuginfodwarf \
	llvmdebuginfopdb \
	llvmdebuginfocodeview \
	llvmobject \
	llvmbitreader \
	llvmcore \
	llvmmcparser \
	llvmmc \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
