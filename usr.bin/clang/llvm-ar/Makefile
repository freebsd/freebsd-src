.include <src.opts.mk>

PACKAGE=	toolchain
PROG_CXX=	llvm-ar
MAN=		llvm-ar.1 llvm-ranlib.1

SRCDIR=		llvm/tools/llvm-ar
SRCS+=		llvm-ar-driver.cpp
SRCS+=		llvm-ar.cpp

LINKS+=		${BINDIR}/llvm-ar ${BINDIR}/llvm-ranlib

.if ${MK_LLVM_BINUTILS} != "no"
SYMLINKS+=	llvm-ar ${BINDIR}/ar
SYMLINKS+=	llvm-ranlib ${BINDIR}/ranlib
MLINKS+=	llvm-ar.1 ar.1 llvm-ar.1 ranlib.1
.endif

.include "../llvm.prog.mk"
