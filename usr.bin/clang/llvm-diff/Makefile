# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-diff

SRCDIR=	tools/llvm-diff
SRCS=	llvm-diff.cpp \
	DiffConsumer.cpp \
	DiffLog.cpp \
	DifferenceEngine.cpp

TGHDRS=	Attributes

LIBDEPS=llvmirreader \
	llvmbitreader \
	llvmasmparser \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
