# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-diff

SRCDIR=	tools/llvm-diff
SRCS=	llvm-diff.cpp \
	DiffConsumer.cpp \
	DiffLog.cpp \
	DifferenceEngine.cpp

LIBDEPS=llvmbitreader \
	llvmasmparser \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
