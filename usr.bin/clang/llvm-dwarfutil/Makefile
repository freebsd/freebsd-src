PROG_CXX=	llvm-dwarfutil

SRCDIR=		llvm/tools/llvm-dwarfutil
SRCS+=		DebugInfoLinker.cpp
SRCS+=		llvm-dwarfutil.cpp

.include "${SRCTOP}/lib/clang/llvm.pre.mk"

CFLAGS+=	-I${.OBJDIR}

INCFILE=	Options.inc
TDFILE=		${LLVM_BASE}/${SRCDIR}/Options.td
GENOPT=		-gen-opt-parser-defs
${INCFILE}: ${TDFILE}
	${LLVM_TBLGEN} ${GENOPT} -I ${LLVM_SRCS}/include -d ${.TARGET:C/$/.d/} \
	    -o ${.TARGET} ${TDFILE}
TGHDRS+=	${INCFILE}

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

.include "../llvm.prog.mk"
