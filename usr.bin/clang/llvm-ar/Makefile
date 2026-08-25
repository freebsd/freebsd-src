.include <src.opts.mk>

PROG_CXX=	llvm-ar
MAN=		llvm-ar.1 llvm-ranlib.1

SRCDIR=		llvm/tools/llvm-ar
SRCS+=		llvm-ar-driver.cpp
SRCS+=		llvm-ar.cpp

LINKS+=		${BINDIR}/llvm-ar ${BINDIR}/llvm-ranlib

.include "../llvm.prog.mk"
