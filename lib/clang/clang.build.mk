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

TBLGEN?=tblgen
CLANG_TBLGEN?=clang-tblgen
TBLINC+=-I ${LLVM_SRCS}/include -I ${LLVM_SRCS}/lib/Target

Intrinsics.inc.h: ${LLVM_SRCS}/include/llvm/Intrinsics.td
	${TBLGEN} -I ${LLVM_SRCS}/lib/VMCore ${TBLINC} -gen-intrinsic \
	    -o ${.TARGET} ${LLVM_SRCS}/include/llvm/Intrinsics.td
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
	InstrInfo/-gen-instr-info \
	MCCodeEmitter/-gen-emitter,-mc-emitter \
	MCPseudoLowering/-gen-pseudo-lowering \
	RegisterInfo/-gen-register-info \
	SubtargetInfo/-gen-subtarget
${arch:T}Gen${hdr:H:C/$/.inc.h/}: ${LLVM_SRCS}/lib/Target/${arch:H}/${arch:T}.td
	${TBLGEN} -I ${LLVM_SRCS}/lib/Target/${arch:H} ${TBLINC} \
	    ${hdr:T:C/,/ /g} -o ${.TARGET} \
	    ${LLVM_SRCS}/lib/Target/${arch:H}/${arch:T}.td
. endfor
.endfor

Attrs.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/AST ${TBLINC} \
	    -gen-clang-attr-classes -o ${.TARGET} \
	    -I ${CLANG_SRCS}/include ${.ALLSRC}

AttrImpl.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/AST ${TBLINC} \
	    -gen-clang-attr-impl -o ${.TARGET} \
	    -I ${CLANG_SRCS}/include ${.ALLSRC}

AttrLateParsed.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Basic ${TBLINC} \
	    -gen-clang-attr-late-parsed-list -o ${.TARGET} \
	    -I ${CLANG_SRCS}/include ${.ALLSRC}

AttrList.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Basic ${TBLINC} \
	    -gen-clang-attr-list -o ${.TARGET} \
	    -I ${CLANG_SRCS}/include ${.ALLSRC}

AttrPCHRead.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Serialization \
	    ${TBLINC} -gen-clang-attr-pch-read -o ${.TARGET} \
	    -I ${CLANG_SRCS}/include ${.ALLSRC}

AttrPCHWrite.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Serialization \
	    ${TBLINC} -gen-clang-attr-pch-write -o ${.TARGET} \
	    -I ${CLANG_SRCS}/include ${.ALLSRC}

AttrSpellings.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Lex ${TBLINC} \
	    -gen-clang-attr-spelling-list -o ${.TARGET} \
	    -I ${CLANG_SRCS}/include ${.ALLSRC}

DeclNodes.inc.h: ${CLANG_SRCS}/include/clang/Basic/DeclNodes.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/AST ${TBLINC} \
	    -gen-clang-decl-nodes -o ${.TARGET} ${.ALLSRC}

StmtNodes.inc.h: ${CLANG_SRCS}/include/clang/Basic/StmtNodes.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/AST ${TBLINC} \
	    -gen-clang-stmt-nodes -o ${.TARGET} ${.ALLSRC}

arm_neon.inc.h: ${CLANG_SRCS}/include/clang/Basic/arm_neon.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Basic ${TBLINC} \
	    -gen-arm-neon-sema -o ${.TARGET} ${.ALLSRC}

DiagnosticGroups.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Basic ${TBLINC} \
	    -gen-clang-diag-groups -o ${.TARGET} ${.ALLSRC}

DiagnosticIndexName.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Basic ${TBLINC} \
	    -gen-clang-diags-index-name -o ${.TARGET} ${.ALLSRC}

.for hdr in AST Analysis Common Driver Frontend Lex Parse Sema
Diagnostic${hdr}Kinds.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Basic ${TBLINC} \
	    -gen-clang-diags-defs -clang-component=${hdr} \
	    -o ${.TARGET} ${.ALLSRC}
.endfor

Options.inc.h: ${CLANG_SRCS}/include/clang/Driver/Options.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Driver ${TBLINC} \
	    -gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

CC1Options.inc.h: ${CLANG_SRCS}/include/clang/Driver/CC1Options.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Driver ${TBLINC} \
	    -gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

CC1AsOptions.inc.h: ${CLANG_SRCS}/include/clang/Driver/CC1AsOptions.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Driver ${TBLINC} \
	    -gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

Checkers.inc.h: ${CLANG_SRCS}/lib/StaticAnalyzer/Checkers/Checkers.td \
	    ${CLANG_SRCS}/include/clang/StaticAnalyzer/Checkers/CheckerBase.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/lib/StaticAnalyzer/Checkers \
	    ${TBLINC} -gen-clang-sa-checkers -o ${.TARGET} \
	    -I ${CLANG_SRCS}/include \
	    ${CLANG_SRCS}/lib/StaticAnalyzer/Checkers/Checkers.td

SRCS+=		${TGHDRS:C/$/.inc.h/}
DPADD+=		${TGHDRS:C/$/.inc.h/}
CLEANFILES+=	${TGHDRS:C/$/.inc.h/}
