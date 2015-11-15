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

.include "../clang.prog.mk"

DPADD+=	${LIBZ}
LDADD+=	-lz
