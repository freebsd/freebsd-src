.include <src.opts.mk>

PROG_CXX=	clang-scan-deps
MAN=

SRCDIR=		clang/tools/clang-scan-deps
SRCS+=		ClangScanDeps.cpp \
		clang-scan-deps-driver.cpp

.include "${SRCTOP}/lib/clang/clang.pre.mk"

CFLAGS+=	-I${.OBJDIR}
TDFILE=		Opts.td
INCFILE=	${TDFILE:.td=.inc}
GENOPT=		-gen-opt-parser-defs

${INCFILE}: ${TDFILE}
	${LLVM_TBLGEN} ${GENOPT} -I ${LLVM_SRCS}/include -d ${.TARGET:C/$/.d/} \
	    -o ${.TARGET} ${.ALLSRC}
TGHDRS+=	${INCFILE}

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

.include "../clang.prog.mk"
