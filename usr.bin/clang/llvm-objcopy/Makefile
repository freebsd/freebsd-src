# $FreeBSD$

.include <src.opts.mk>

PROG_CXX=	llvm-objcopy

SRCDIR=		llvm/tools/llvm-objcopy
SRCS+=		COFF/COFFObjcopy.cpp
SRCS+=		COFF/Object.cpp
SRCS+=		COFF/Reader.cpp
SRCS+=		COFF/Writer.cpp
SRCS+=		ConfigManager.cpp
SRCS+=		ELF/ELFObjcopy.cpp
SRCS+=		ELF/Object.cpp
SRCS+=		MachO/MachOLayoutBuilder.cpp
SRCS+=		MachO/MachOObjcopy.cpp
SRCS+=		MachO/MachOReader.cpp
SRCS+=		MachO/MachOWriter.cpp
SRCS+=		MachO/Object.cpp
SRCS+=		llvm-objcopy.cpp
SRCS+=		wasm/Object.cpp
SRCS+=		wasm/Reader.cpp
SRCS+=		wasm/WasmObjcopy.cpp
SRCS+=		wasm/Writer.cpp

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

${TGHDRS}: ${LLVM_TBLGEN_BIN}

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

LIBADD+=	z

LINKS=		${BINDIR}/llvm-objcopy ${BINDIR}/llvm-strip

.if ${MK_LLVM_BINUTILS} != "no"
LINKS+=		${BINDIR}/llvm-objcopy ${BINDIR}/objcopy \
		${BINDIR}/llvm-strip ${BINDIR}/strip
MLINKS=		llvm-objcopy.1 objcopy.1
.endif

.include "../llvm.prog.mk"
