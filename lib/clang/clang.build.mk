# $FreeBSD$

CLANG_SRCS=${LLVM_SRCS}/tools/clang

CFLAGS+=-I${LLVM_SRCS}/include -I${CLANG_SRCS}/include \
	-I${LLVM_SRCS}/${SRCDIR} ${INCDIR:C/^/-I${LLVM_SRCS}\//} -I. \
	-I${LLVM_SRCS}/../../lib/clang/include \
	-DLLVM_ON_UNIX -DLLVM_ON_FREEBSD \
	-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS #-DNDEBUG

TARGET_ARCH?=	${MACHINE_ARCH}
# XXX: 8.0, to keep __FreeBSD_cc_version happy
CFLAGS+=-DLLVM_HOSTTRIPLE=\"${TARGET_ARCH}-undermydesk-freebsd9.0\" \
	-DCLANG_VENDOR=\"clang\ r104832\ 20100610\ [FreeBSD]\\n\"

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
	FastISel/-gen-fast-isel \
	InstrInfo/-gen-instr-desc \
	InstrNames/-gen-instr-enums \
	RegisterInfo.h/-gen-register-desc-header \
	RegisterInfo/-gen-register-desc \
	RegisterNames/-gen-register-enums \
	Subtarget/-gen-subtarget
${arch:T}Gen${hdr:H:C/$/.inc.h/}: ${LLVM_SRCS}/lib/Target/${arch:H}/${arch:T}.td
	${TBLGEN} ${hdr:T:C/,/ /g} \
		${LLVM_SRCS}/lib/Target/${arch:H}/${arch:T}.td > ${.TARGET}
. endfor
.endfor

DiagnosticGroups.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_SRCS}/include/clang/Basic \
		-gen-clang-diag-groups \
		${CLANG_SRCS}/include/clang/Basic/Diagnostic.td > ${.TARGET}
.for hdr in AST Analysis Common Driver Frontend Lex Parse Sema
Diagnostic${hdr}Kinds.inc.h: ${CLANG_SRCS}/include/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_SRCS}/include/clang/Basic \
		-gen-clang-diags-defs -clang-component=${hdr} \
		${CLANG_SRCS}/include/clang/Basic/Diagnostic.td > ${.TARGET}
.endfor
CC1AsOptions.inc.h: ${CLANG_SRCS}/include/clang/Driver/CC1AsOptions.td
	${TBLGEN} -I${CLANG_SRCS}/include/clang/Driver \
	   -gen-opt-parser-defs \
	   ${CLANG_SRCS}/include/clang/Driver/CC1AsOptions.td > ${.TARGET}
CC1Options.inc.h: ${CLANG_SRCS}/include/clang/Driver/CC1Options.td
	${TBLGEN} -I${CLANG_SRCS}/include/clang/Driver \
	   -gen-opt-parser-defs \
	   ${CLANG_SRCS}/include/clang/Driver/CC1Options.td > ${.TARGET}
Options.inc.h: ${CLANG_SRCS}/include/clang/Driver/Options.td
	${TBLGEN} -I${CLANG_SRCS}/include/clang/Driver \
	   -gen-opt-parser-defs \
	   ${CLANG_SRCS}/include/clang/Driver/Options.td > ${.TARGET}
StmtNodes.inc.h: ${CLANG_SRCS}/include/clang/AST/StmtNodes.td
	${TBLGEN} -I${CLANG_SRCS}/include/clang/AST \
		-gen-clang-stmt-nodes \
		${CLANG_SRCS}/include/clang/AST/StmtNodes.td > ${.TARGET}

SRCS+=		${TGHDRS:C/$/.inc.h/}
DPADD+=		${TGHDRS:C/$/.inc.h/}
CLEANFILES+=	${TGHDRS:C/$/.inc.h/}
