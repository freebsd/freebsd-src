# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=llvm-diff

SRCDIR=	tools/llvm-diff
SRCS=	DiffConsumer.cpp \
	DiffLog.cpp \
	DifferenceEngine.cpp \
	llvm-diff.cpp

TGHDRS=	Attributes

LIBDEPS=llvmirreader \
	llvmasmparser \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
