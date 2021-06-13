# $FreeBSD$

PROG_CXX=	llvm-symbolizer
MAN=		llvm-symbolizer.1 llvm-addr2line.1

SRCDIR=		llvm/tools/llvm-symbolizer
SRCS+=		llvm-symbolizer.cpp

.include "${SRCTOP}/lib/clang/llvm.pre.mk"

CFLAGS+=	-I${.OBJDIR}

INCFILE=	Opts.inc
TDFILE=		${LLVM_SRCS}/tools/llvm-symbolizer/Opts.td
GENOPT=		-gen-opt-parser-defs
${INCFILE}: ${TDFILE}
	${LLVM_TBLGEN} ${GENOPT} -I ${LLVM_SRCS}/include -d ${.TARGET:C/$/.d/} \
	    -o ${.TARGET} ${TDFILE}
TGHDRS+=	${INCFILE}

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

LIBADD+=	z

LINKS+=		${BINDIR}/llvm-symbolizer ${BINDIR}/llvm-addr2line

.include "../llvm.prog.mk"
