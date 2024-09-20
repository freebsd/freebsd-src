.include <src.opts.mk>

PROG_CXX=	llvm-objcopy

SRCDIR=		llvm/tools/llvm-objcopy
SRCS+=		ObjcopyOptions.cpp
SRCS+=		llvm-objcopy-driver.cpp
SRCS+=		llvm-objcopy.cpp

.include "${SRCTOP}/lib/clang/llvm.pre.mk"

CFLAGS+=	-I${.OBJDIR}
CFLAGS+=	-I${LLVM_BASE}/${SRCDIR}

.for hdr in BitcodeStripOpts InstallNameToolOpts ObjcopyOpts StripOpts
${hdr}.inc: ${LLVM_BASE}/${SRCDIR}/${hdr}.td
	${LLVM_TBLGEN} -gen-opt-parser-defs \
	    -I ${LLVM_SRCS}/tools/llvm-objcopy -I ${LLVM_SRCS}/include \
	    -d ${.TARGET}.d -o ${.TARGET} ${LLVM_BASE}/${SRCDIR}/${hdr}.td
TGHDRS+=	${hdr}.inc
.endfor

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

LINKS=		${BINDIR}/llvm-objcopy ${BINDIR}/llvm-strip

.if ${MK_LLVM_BINUTILS} != "no"
LINKS+=		${BINDIR}/llvm-objcopy ${BINDIR}/objcopy \
		${BINDIR}/llvm-strip ${BINDIR}/strip
MLINKS=		llvm-objcopy.1 objcopy.1 \
		llvm-objcopy.1 strip.1
.endif

.include "../llvm.prog.mk"
