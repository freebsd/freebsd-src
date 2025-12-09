PROG_CXX=	llvm-tblgen
MAN=

SRCDIR=		llvm/utils/TableGen
SRCS+=		AsmMatcherEmitter.cpp
SRCS+=		AsmWriterEmitter.cpp
SRCS+=		Basic/ARMTargetDefEmitter.cpp
SRCS+=		Basic/Attributes.cpp
SRCS+=		Basic/CodeGenIntrinsics.cpp
SRCS+=		Basic/DirectiveEmitter.cpp
SRCS+=		Basic/IntrinsicEmitter.cpp
SRCS+=		Basic/RISCVTargetDefEmitter.cpp
SRCS+=		Basic/RuntimeLibcallsEmitter.cpp
SRCS+=		Basic/SDNodeProperties.cpp
SRCS+=		Basic/TableGen.cpp
SRCS+=		Basic/TargetFeaturesEmitter.cpp
SRCS+=		Basic/VTEmitter.cpp
SRCS+=		CTagsEmitter.cpp
SRCS+=		CallingConvEmitter.cpp
SRCS+=		CodeEmitterGen.cpp
SRCS+=		CodeGenMapTable.cpp
SRCS+=		Common/AsmWriterInst.cpp
SRCS+=		Common/CodeGenDAGPatterns.cpp
SRCS+=		Common/CodeGenHwModes.cpp
SRCS+=		Common/CodeGenInstAlias.cpp
SRCS+=		Common/CodeGenInstruction.cpp
SRCS+=		Common/CodeGenRegisters.cpp
SRCS+=		Common/CodeGenSchedule.cpp
SRCS+=		Common/CodeGenTarget.cpp
SRCS+=		Common/DAGISelMatcher.cpp
SRCS+=		Common/GlobalISel/CodeExpander.cpp
SRCS+=		Common/GlobalISel/CombinerUtils.cpp
SRCS+=		Common/GlobalISel/CXXPredicates.cpp
SRCS+=		Common/GlobalISel/GlobalISelMatchTable.cpp
SRCS+=		Common/GlobalISel/GlobalISelMatchTableExecutorEmitter.cpp
SRCS+=		Common/GlobalISel/PatternParser.cpp
SRCS+=		Common/GlobalISel/Patterns.cpp
SRCS+=		Common/InfoByHwMode.cpp
SRCS+=		Common/OptEmitter.cpp
SRCS+=		Common/PredicateExpander.cpp
SRCS+=		Common/SubtargetFeatureInfo.cpp
SRCS+=		Common/Types.cpp
SRCS+=		Common/Utils.cpp
SRCS+=		Common/VarLenCodeEmitterGen.cpp
SRCS+=		CompressInstEmitter.cpp
SRCS+=		DAGISelEmitter.cpp
SRCS+=		DAGISelMatcherEmitter.cpp
SRCS+=		DAGISelMatcherGen.cpp
SRCS+=		DAGISelMatcherOpt.cpp
SRCS+=		DFAEmitter.cpp
SRCS+=		DFAPacketizerEmitter.cpp
SRCS+=		DXILEmitter.cpp
SRCS+=		DecoderEmitter.cpp
SRCS+=		DisassemblerEmitter.cpp
SRCS+=		ExegesisEmitter.cpp
SRCS+=		FastISelEmitter.cpp
SRCS+=		GlobalISelCombinerEmitter.cpp
SRCS+=		GlobalISelEmitter.cpp
SRCS+=		InstrDocsEmitter.cpp
SRCS+=		InstrInfoEmitter.cpp
SRCS+=		MacroFusionPredicatorEmitter.cpp
SRCS+=		OptionParserEmitter.cpp
SRCS+=		PseudoLoweringEmitter.cpp
SRCS+=		RegisterBankEmitter.cpp
SRCS+=		RegisterInfoEmitter.cpp
SRCS+=		SDNodeInfoEmitter.cpp
SRCS+=		SearchableTableEmitter.cpp
SRCS+=		SubtargetEmitter.cpp
SRCS+=		WebAssemblyDisassemblerEmitter.cpp
SRCS+=		X86DisassemblerTables.cpp
SRCS+=		X86FoldTablesEmitter.cpp
SRCS+=		X86InstrMappingEmitter.cpp
SRCS+=		X86MnemonicTables.cpp
SRCS+=		X86ModRMFilters.cpp
SRCS+=		X86RecognizableInstr.cpp
SRCS+=		llvm-tblgen.cpp

CFLAGS+=	-I${LLVM_BASE}/${SRCDIR}
CFLAGS+=	-I${.OBJDIR}

.include "${SRCTOP}/lib/clang/llvm.pre.mk"

llvm/CodeGen/GenVT.inc: ${LLVM_SRCS}/include/llvm/CodeGen/ValueTypes.td
	${LLVM_MIN_TBLGEN} -gen-vt \
	    -I ${LLVM_SRCS}/include -d ${.TARGET}.d -o ${.TARGET} \
	    ${LLVM_SRCS}/include/llvm/CodeGen/ValueTypes.td
TGHDRS+=	llvm/CodeGen/GenVT.inc

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}
CLEANFILES+=	${GENSRCS} ${GENSRCS:C/$/.d/}

.include "../llvm.prog.mk"
