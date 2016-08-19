# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-dwarfdump

SRCDIR=	tools/llvm-dwarfdump
SRCS=	llvm-dwarfdump.cpp

LIBDEPS=llvmdebuginfodwarf \
	llvmobject \
	llvmbitreader \
	llvmcore \
	llvmmcparser \
	llvmmc \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
