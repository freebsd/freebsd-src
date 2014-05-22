# $FreeBSD$

CLANG_SRCS=	${LLVM_SRCS}/tools/clang

CFLAGS+=	-I${LLVM_SRCS}/include -I${CLANG_SRCS}/include \
		-I${LLVM_SRCS}/${SRCDIR} ${INCDIR:C/^/-I${LLVM_SRCS}\//} -I. \
		-I${LLVM_SRCS}/../../lib/clang/include \
		-DLLVM_ON_UNIX -DLLVM_ON_FREEBSD \
		-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS #-DNDEBUG

.if !defined(EARLY_BUILD) && defined(MK_CLANG_FULL) && ${MK_CLANG_FULL} != "no"
CFLAGS+=	-DCLANG_ENABLE_ARCMT \
		-DCLANG_ENABLE_REWRITER \
		-DCLANG_ENABLE_STATIC_ANALYZER
.endif # !EARLY_BUILD && MK_CLANG_FULL

# LLVM is not strict aliasing safe as of 12/31/2011
CFLAGS+= -fno-strict-aliasing

TARGET_ARCH?=	${MACHINE_ARCH}
BUILD_ARCH?=	${MACHINE_ARCH}

.if (${TARGET_ARCH} == "arm" || ${TARGET_ARCH} == "armv6") && \
    ${MK_ARM_EABI} != "no"
TARGET_ABI=	gnueabi
.else
TARGET_ABI=	unknown
.endif

TARGET_TRIPLE?=	${TARGET_ARCH:C/amd64/x86_64/}-${TARGET_ABI}-freebsd9.3
BUILD_TRIPLE?=	${BUILD_ARCH:C/amd64/x86_64/}-unknown-freebsd9.3
CFLAGS+=	-DLLVM_DEFAULT_TARGET_TRIPLE=\"${TARGET_TRIPLE}\" \
		-DLLVM_HOST_TRIPLE=\"${BUILD_TRIPLE}\" \
		-DDEFAULT_SYSROOT=\"${TOOLS_PREFIX}\"
CXXFLAGS+=	-fno-exceptions -fno-rtti

.PATH:	${LLVM_SRCS}/${SRCDIR}

TBLGEN?=	tblgen
CLANG_TBLGEN?=	clang-tblgen

Intrinsics.inc.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${TBLGEN} -gen-intrinsic \
	    -I ${LLVM_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
.for arch in \
	ARM/ARM Mips/Mips PowerPC/PPC Sparc/Sparc X86/X86
. for hdr in \
	AsmMatcher/-gen-asm-matcher \
	AsmWriter1/-gen-asm-writer,-asmwriternum=1 \
	AsmWriter/-gen-asm-writer \
	CallingConv/-gen-callingconv \
	CodeEmitter/-gen-emitter \
	DAGISel/-gen-dag-isel \
	DisassemblerTables/-gen-disassembler \
	FastISel/-gen-fast-isel \
	InstrInfo/-gen-instr-info \
	MCCodeEmitter/-gen-emitter,-mc-emitter \
	MCPseudoLowering/-gen-pseudo-lowering \
	RegisterInfo/-gen-register-info \
	SubtargetInfo/-gen-subtarget
${arch:T}Gen${hdr:H:C/$/.inc.h/}: ${LLVM_SRCS}/lib/Target/${arch:H}/${arch:T}.td
	${TBLGEN} ${hdr:T:C/,/ /g} \
	    -I ${LLVM_SRCS}/include -I ${LLVM_SRCS}/lib/Target/${arch:H} \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${LLVM_SRCS}/lib/Target/${arch:H}/${arch:T}.td
. endfor
.endfor

Attrs.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-classes \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrDump.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-dump \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrIdentifierArg.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-identifier-arg-list \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrImpl.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-impl \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrLateParsed.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-late-parsed-list \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrList.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-list \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrParsedAttrImpl.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-parsed-attr-impl \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrParsedAttrKinds.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-parsed-attr-kinds \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrParsedAttrList.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-parsed-attr-list \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrPCHRead.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-pch-read \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrPCHWrite.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-pch-write \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrSpellings.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-spelling-list \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrSpellingListIndex.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-spelling-index \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrTemplateInstantiate.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-template-instantiate \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

AttrTypeArg.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -gen-clang-attr-type-arg-list \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${.ALLSRC}

CommentCommandInfo.inc.h: ${CLANG_SRCS}/include/clang/AST/CommentCommands.td
	${CLANG_TBLGEN} -gen-clang-comment-command-info \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

CommentCommandList.inc.h: ${CLANG_SRCS}/include/clang/AST/CommentCommands.td
	${CLANG_TBLGEN} -gen-clang-comment-command-list \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

CommentHTMLNamedCharacterReferences.inc.h: \
	${CLANG_SRCS}/include/clang/AST/CommentHTMLNamedCharacterReferences.td
	${CLANG_TBLGEN} -gen-clang-comment-html-named-character-references \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

CommentHTMLTags.inc.h: ${CLANG_SRCS}/include/clang/AST/CommentHTMLTags.td
	${CLANG_TBLGEN} -gen-clang-comment-html-tags \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

CommentHTMLTagsProperties.inc.h: \
	${CLANG_SRCS}/include/clang/AST/CommentHTMLTags.td
	${CLANG_TBLGEN} -gen-clang-comment-html-tags-properties \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

CommentNodes.inc.h: ${CLANG_SRCS}/include/clang/Basic/CommentNodes.td
	${CLANG_TBLGEN} -gen-clang-comment-nodes \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

DeclNodes.inc.h: ${CLANG_SRCS}/include/clang/Basic/DeclNodes.td
	${CLANG_TBLGEN} -gen-clang-decl-nodes \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

StmtNodes.inc.h: ${CLANG_SRCS}/include/clang/Basic/StmtNodes.td
	${CLANG_TBLGEN} -gen-clang-stmt-nodes \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

arm_neon.h: ${CLANG_SRCS}/include/clang/Basic/arm_neon.td
	${CLANG_TBLGEN} -gen-arm-neon \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

arm_neon.inc.h: ${CLANG_SRCS}/include/clang/Basic/arm_neon.td
	${CLANG_TBLGEN} -gen-arm-neon-sema \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

DiagnosticGroups.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${CLANG_TBLGEN} -gen-clang-diag-groups \
	    -I ${CLANG_SRCS}/include/clang/Basic -d ${.TARGET:C/\.h$/.d/} \
	    -o ${.TARGET} ${.ALLSRC}

DiagnosticIndexName.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${CLANG_TBLGEN} -gen-clang-diags-index-name \
	    -I ${CLANG_SRCS}/include/clang/Basic -d ${.TARGET:C/\.h$/.d/} \
	    -o ${.TARGET} ${.ALLSRC}

.for hdr in AST Analysis Comment Common Driver Frontend Lex Parse Sema Serialization
Diagnostic${hdr}Kinds.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${CLANG_TBLGEN} -gen-clang-diags-defs -clang-component=${hdr} \
	    -I ${CLANG_SRCS}/include/clang/Basic -d ${.TARGET:C/\.h$/.d/} \
	    -o ${.TARGET} ${.ALLSRC}
.endfor

Options.inc.h: ${CLANG_SRCS}/include/clang/Driver/Options.td
	${TBLGEN} -gen-opt-parser-defs \
	    -I ${LLVM_SRCS}/include -I ${CLANG_SRCS}/include/clang/Driver \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

CC1AsOptions.inc.h: ${CLANG_SRCS}/include/clang/Driver/CC1AsOptions.td
	${TBLGEN} -gen-opt-parser-defs \
	    -I ${LLVM_SRCS}/include -I ${CLANG_SRCS}/include/clang/Driver \
	    -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} ${.ALLSRC}

Checkers.inc.h: ${CLANG_SRCS}/lib/StaticAnalyzer/Checkers/Checkers.td \
	    ${CLANG_SRCS}/include/clang/StaticAnalyzer/Checkers/CheckerBase.td
	${CLANG_TBLGEN} -gen-clang-sa-checkers \
	    -I ${CLANG_SRCS}/include -d ${.TARGET:C/\.h$/.d/} -o ${.TARGET} \
	    ${CLANG_SRCS}/lib/StaticAnalyzer/Checkers/Checkers.td

.for dep in ${TGHDRS:C/$/.inc.d/}
. sinclude "${dep}"
.endfor

SRCS+=		${TGHDRS:C/$/.inc.h/}
DPADD+=		${TGHDRS:C/$/.inc.h/}
CLEANFILES+=	${TGHDRS:C/$/.inc.h/} ${TGHDRS:C/$/.inc.d/}
