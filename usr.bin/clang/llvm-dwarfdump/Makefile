# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-dwarfdump

SRCDIR=	tools/llvm-dwarfdump
SRCS=	llvm-dwarfdump.cpp

LIBDEPS=llvmdebuginfodwarf \
	llvmobject \
	llvmmcparser \
	llvmmc \
	llvmbitreader \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
