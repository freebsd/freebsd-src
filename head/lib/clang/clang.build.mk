# $FreeBSD$

CLANG_SRCS=${LLVM_SRCS}/tools/clang

CFLAGS+=-I${LLVM_SRCS}/include -I${CLANG_SRCS}/include \
	-I${LLVM_SRCS}/${SRCDIR} ${INCDIR:C/^/-I${LLVM_SRCS}\//} -I. \
	-I${LLVM_SRCS}/../../lib/clang/include \
	-DLLVM_ON_UNIX -DLLVM_ON_FREEBSD \
	-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS #-DNDEBUG

# Correct for gcc miscompilation when compiling on PPC with -O2
.if ${MACHINE_ARCH} == "powerpc"
CFLAGS+= -O1
.endif

TARGET_ARCH?=	${MACHINE_ARCH}
# XXX: 8.0, to keep __FreeBSD_cc_version happy
CFLAGS+=-DLLVM_HOSTTRIPLE=\"${TARGET_ARCH:C/amd64/x86_64/}-unknown-freebsd9.0\"

.ifndef LLVM_REQUIRES_EH
CXXFLAGS+=-fno-exceptions
.else
# If the library or program requires EH, it also requires RTTI.
LLVM_REQUIRES_RTTI=
.endif

.ifndef LLVM_REQUIRES_RTTI
CXXFLAGS+=-fno-rtti
.endif

.ifdef TOOLS_PREFIX
CFLAGS+=-DCLANG_PREFIX=\"${TOOLS_PREFIX}\"
.endif

.PATH:	${LLVM_SRCS}/${SRCDIR}

TBLGEN=tblgen ${CFLAGS:M-I*}

Intrinsics.inc.h: ${LLVM_SRCS}/include/llvm/Intrinsics.td
	${TBLGEN} -gen-intrinsic \
	    ${LLVM_SRCS}/include/llvm/Intrinsics.td > ${.TARGET}
.for arch in \
	ARM/ARM Mips/Mips PowerPC/PPC X86/X86
. for hdr in \
	AsmMatcher/-gen-asm-matcher \
	AsmWriter1/-gen-asm-writer,-asmwriternum=1 \
	AsmWriter/-gen-asm-writer \
	CallingConv/-gen-callingconv \
	CodeEmitter/-gen-emitter \
	DAGISel/-gen-dag-isel \
	DisassemblerTables/-gen-disassembler \
	EDInfo/-gen-enhanced-disassembly-info \
	FastISel/-gen-fast-isel \
	InstrInfo/-gen-instr-desc \
	InstrNames/-gen-instr-enums \
	MCCodeEmitter/-gen-emitter,-mc-emitter \
	RegisterInfo.h/-gen-register-desc-header \
	RegisterInfo/-gen-register-desc \
	RegisterNames/-gen-register-enums \
	Subtarget/-gen-subtarget
${arch:T}Gen${hdr:H:C/$/.inc.h/}: ${LLVM_SRCS}/lib/Target/${arch:H}/${arch:T}.td
	${TBLGEN} ${hdr:T:C/,/ /g} \
	    ${LLVM_SRCS}/lib/Target/${arch:H}/${arch:T}.td > ${.TARGET}
. endfor
.endfor

ARMGenDecoderTables.inc.h: ${LLVM_SRCS}/lib/Target/ARM/ARM.td
	${TBLGEN} -gen-arm-decoder ${.ALLSRC} > ${.TARGET}

Attrs.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${TBLGEN} -gen-clang-attr-classes ${.ALLSRC} > ${.TARGET}

AttrImpl.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${TBLGEN} -gen-clang-attr-impl ${.ALLSRC} > ${.TARGET}

AttrList.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${TBLGEN} -gen-clang-attr-list ${.ALLSRC} > ${.TARGET}

AttrPCHRead.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${TBLGEN} -gen-clang-attr-pch-read ${.ALLSRC} > ${.TARGET}

AttrPCHWrite.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${TBLGEN} -gen-clang-attr-pch-write ${.ALLSRC} > ${.TARGET}

AttrSpellings.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${TBLGEN} -gen-clang-attr-spelling-list ${.ALLSRC} > ${.TARGET}

DeclNodes.inc.h: ${CLANG_SRCS}/include/clang/Basic/DeclNodes.td
	${TBLGEN} -gen-clang-decl-nodes ${.ALLSRC} > ${.TARGET}

StmtNodes.inc.h: ${CLANG_SRCS}/include/clang/Basic/StmtNodes.td
	${TBLGEN} -gen-clang-stmt-nodes ${.ALLSRC} > ${.TARGET}

arm_neon.inc.h: ${CLANG_SRCS}/include/clang/Basic/arm_neon.td
	${TBLGEN} -gen-arm-neon-sema ${.ALLSRC} > ${.TARGET}

DiagnosticGroups.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${TBLGEN} -gen-clang-diag-groups -I${CLANG_SRCS}/include/clang/Basic \
	    ${.ALLSRC} > ${.TARGET}

DiagnosticIndexName.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${TBLGEN} -gen-clang-diags-index-name \
	    -I${CLANG_SRCS}/include/clang/Basic ${.ALLSRC} > ${.TARGET}

.for hdr in AST Analysis Common Driver Frontend Lex Parse Sema
Diagnostic${hdr}Kinds.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${TBLGEN} -gen-clang-diags-defs -clang-component=${hdr} \
	    -I${CLANG_SRCS}/include/clang/Basic ${.ALLSRC} > ${.TARGET}
.endfor

Options.inc.h: ${CLANG_SRCS}/include/clang/Driver/Options.td
	${TBLGEN} -gen-opt-parser-defs -I${CLANG_SRCS}/include/clang/Driver \
	    ${.ALLSRC} > ${.TARGET}

CC1Options.inc.h: ${CLANG_SRCS}/include/clang/Driver/CC1Options.td
	${TBLGEN} -gen-opt-parser-defs -I${CLANG_SRCS}/include/clang/Driver \
	    ${.ALLSRC} > ${.TARGET}

CC1AsOptions.inc.h: ${CLANG_SRCS}/include/clang/Driver/CC1AsOptions.td
	${TBLGEN} -gen-opt-parser-defs -I${CLANG_SRCS}/include/clang/Driver \
	    ${.ALLSRC} > ${.TARGET}

Checkers.inc.h: ${CLANG_SRCS}/lib/StaticAnalyzer/Checkers/Checkers.td \
	    ${CLANG_SRCS}/include/clang/StaticAnalyzer/Checkers/CheckerBase.td
	${TBLGEN} -gen-clang-sa-checkers -I${CLANG_SRCS}/include \
	    ${CLANG_SRCS}/lib/StaticAnalyzer/Checkers/Checkers.td > ${.TARGET}

SRCS+=		${TGHDRS:C/$/.inc.h/}
DPADD+=		${TGHDRS:C/$/.inc.h/}
CLEANFILES+=	${TGHDRS:C/$/.inc.h/}
