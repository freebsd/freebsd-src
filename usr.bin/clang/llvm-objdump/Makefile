.include <src.opts.mk>

PROG_CXX=	llvm-objdump

SRCDIR=		llvm/tools/llvm-objdump
SRCS+=		COFFDump.cpp
SRCS+=		ELFDump.cpp
SRCS+=		MachODump.cpp
SRCS+=		OffloadDump.cpp
SRCS+=		SourcePrinter.cpp
SRCS+=		WasmDump.cpp
SRCS+=		XCOFFDump.cpp
SRCS+=		llvm-objdump-driver.cpp
SRCS+=		llvm-objdump.cpp

.include "${SRCTOP}/lib/clang/llvm.pre.mk"

CFLAGS+=	-I${.OBJDIR}

.for hdr in ObjdumpOpts OtoolOpts
${hdr}.inc: ${LLVM_BASE}/${SRCDIR}/${hdr}.td
	${LLVM_TBLGEN} -gen-opt-parser-defs \
	    -I ${LLVM_SRCS}/tools/llvm-objcopy -I ${LLVM_SRCS}/include \
	    -d ${.TARGET}.d -o ${.TARGET} ${LLVM_BASE}/${SRCDIR}/${hdr}.td
TGHDRS+=	${hdr}.inc
.endfor

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

LINKS=		${BINDIR}/llvm-objdump ${BINDIR}/objdump
MLINKS=		llvm-objdump.1 objdump.1

.include "../llvm.prog.mk"
