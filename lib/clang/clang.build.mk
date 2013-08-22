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
CFLAGS+=	-fno-strict-aliasing

TARGET_ARCH?=	${MACHINE_ARCH}
BUILD_ARCH?=	${MACHINE_ARCH}

.if (${TARGET_ARCH} == "arm" || ${TARGET_ARCH} == "armv6") && \
    ${MK_ARM_EABI} != "no"
TARGET_ABI=	gnueabi
.else
TARGET_ABI=	unknown
.endif

TARGET_TRIPLE?=	${TARGET_ARCH:C/amd64/x86_64/}-${TARGET_ABI}-freebsd10.0
BUILD_TRIPLE?=	${BUILD_ARCH:C/amd64/x86_64/}-unknown-freebsd10.0
CFLAGS+=	-DLLVM_DEFAULT_TARGET_TRIPLE=\"${TARGET_TRIPLE}\" \
		-DLLVM_HOSTTRIPLE=\"${BUILD_TRIPLE}\" \
		-DDEFAULT_SYSROOT=\"${TOOLS_PREFIX}\"
CXXFLAGS+=	-fno-exceptions -fno-rtti

.PATH:	${LLVM_SRCS}/${SRCDIR}

TBLGEN?=	tblgen
CLANG_TBLGEN?=	clang-tblgen

Intrinsics.inc.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td \
		  ${LLVM_SRCS}/include/llvm/IR/IntrinsicsARM.td \
		  ${LLVM_SRCS}/include/llvm/IR/IntrinsicsHexagon.td \
		  ${LLVM_SRCS}/include/llvm/IR/IntrinsicsMips.td \
		  ${LLVM_SRCS}/include/llvm/IR/IntrinsicsNVVM.td \
		  ${LLVM_SRCS}/include/llvm/IR/IntrinsicsPowerPC.td \
		  ${LLVM_SRCS}/include/llvm/IR/IntrinsicsR600.td \
		  ${LLVM_SRCS}/include/llvm/IR/IntrinsicsX86.td \
		  ${LLVM_SRCS}/include/llvm/IR/IntrinsicsXCore.td
	${TBLGEN} -I ${LLVM_SRCS}/include \
	    -gen-intrinsic -o ${.TARGET} \
	    ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
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
	FastISel/-gen-fast-isel \
	InstrInfo/-gen-instr-info \
	MCCodeEmitter/-gen-emitter,-mc-emitter \
	MCPseudoLowering/-gen-pseudo-lowering \
	RegisterInfo/-gen-register-info \
	SubtargetInfo/-gen-subtarget
${arch:T}Gen${hdr:H:C/$/.inc.h/}: ${LLVM_SRCS}/lib/Target/${arch:H}/${arch:T}.td
	${TBLGEN} -I ${LLVM_SRCS}/include -I ${LLVM_SRCS}/lib/Target/${arch:H} \
	    ${hdr:T:C/,/ /g} -o ${.TARGET} \
	    ${LLVM_SRCS}/lib/Target/${arch:H}/${arch:T}.td
. endfor
.endfor

Attrs.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-classes -o ${.TARGET} ${.ALLSRC}

AttrDump.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-dump -o ${.TARGET} ${.ALLSRC}

AttrImpl.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-impl -o ${.TARGET} ${.ALLSRC}
AttrLateParsed.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-late-parsed-list -o ${.TARGET} ${.ALLSRC}

AttrList.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-list -o ${.TARGET} ${.ALLSRC}

AttrParsedAttrKinds.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-parsed-attr-kinds -o ${.TARGET} ${.ALLSRC}

AttrParsedAttrList.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-parsed-attr-list -o ${.TARGET} ${.ALLSRC}

AttrPCHRead.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-pch-read -o ${.TARGET} ${.ALLSRC}

AttrPCHWrite.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-pch-write -o ${.TARGET} ${.ALLSRC}

AttrSpellings.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-spelling-list -o ${.TARGET} ${.ALLSRC}

AttrSpellingListIndex.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-spelling-index -o ${.TARGET} ${.ALLSRC}

AttrTemplateInstantiate.inc.h: ${CLANG_SRCS}/include/clang/Basic/Attr.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-attr-template-instantiate -o ${.TARGET} ${.ALLSRC}

CommentCommandInfo.inc.h: ${CLANG_SRCS}/include/clang/AST/CommentCommands.td
	${CLANG_TBLGEN} \
	    -gen-clang-comment-command-info -o ${.TARGET} ${.ALLSRC}

CommentCommandList.inc.h: ${CLANG_SRCS}/include/clang/AST/CommentCommands.td
	${CLANG_TBLGEN} \
	    -gen-clang-comment-command-list -o ${.TARGET} ${.ALLSRC}

CommentHTMLNamedCharacterReferences.inc.h: \
	${CLANG_SRCS}/include/clang/AST/CommentHTMLNamedCharacterReferences.td
	${CLANG_TBLGEN} \
	    -gen-clang-comment-html-named-character-references -o ${.TARGET} \
	    ${.ALLSRC}

CommentHTMLTags.inc.h: ${CLANG_SRCS}/include/clang/AST/CommentHTMLTags.td
	${CLANG_TBLGEN} \
	    -gen-clang-comment-html-tags -o ${.TARGET} ${.ALLSRC}

CommentHTMLTagsProperties.inc.h: \
	${CLANG_SRCS}/include/clang/AST/CommentHTMLTags.td
	${CLANG_TBLGEN} \
	    -gen-clang-comment-html-tags-properties -o ${.TARGET} ${.ALLSRC}

CommentNodes.inc.h: ${CLANG_SRCS}/include/clang/Basic/CommentNodes.td
	${CLANG_TBLGEN} \
	    -gen-clang-comment-nodes -o ${.TARGET} ${.ALLSRC}

DeclNodes.inc.h: ${CLANG_SRCS}/include/clang/Basic/DeclNodes.td
	${CLANG_TBLGEN} \
	    -gen-clang-decl-nodes -o ${.TARGET} ${.ALLSRC}

StmtNodes.inc.h: ${CLANG_SRCS}/include/clang/Basic/StmtNodes.td
	${CLANG_TBLGEN} \
	    -gen-clang-stmt-nodes -o ${.TARGET} ${.ALLSRC}

arm_neon.inc.h: ${CLANG_SRCS}/include/clang/Basic/arm_neon.td
	${CLANG_TBLGEN} \
	    -gen-arm-neon-sema -o ${.TARGET} ${.ALLSRC}

DiagnosticGroups.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Basic \
	    -gen-clang-diag-groups -o ${.TARGET} ${.ALLSRC}

DiagnosticIndexName.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Basic \
	    -gen-clang-diags-index-name -o ${.TARGET} ${.ALLSRC}

.for hdr in AST Analysis Comment Common Driver Frontend Lex Parse Sema Serialization
Diagnostic${hdr}Kinds.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Basic \
	    -gen-clang-diags-defs -clang-component=${hdr} -o ${.TARGET} \
	    ${.ALLSRC}
.endfor

Options.inc.h: ${CLANG_SRCS}/include/clang/Driver/Options.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Driver \
	    -gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

CC1AsOptions.inc.h: ${CLANG_SRCS}/include/clang/Driver/CC1AsOptions.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include/clang/Driver \
	    -gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

Checkers.inc.h: ${CLANG_SRCS}/lib/StaticAnalyzer/Checkers/Checkers.td \
	    ${CLANG_SRCS}/include/clang/StaticAnalyzer/Checkers/CheckerBase.td
	${CLANG_TBLGEN} -I ${CLANG_SRCS}/include \
	    -gen-clang-sa-checkers -o ${.TARGET} \
	    ${CLANG_SRCS}/lib/StaticAnalyzer/Checkers/Checkers.td

SRCS+=		${TGHDRS:C/$/.inc.h/}
DPADD+=		${TGHDRS:C/$/.inc.h/}
CLEANFILES+=	${TGHDRS:C/$/.inc.h/}
