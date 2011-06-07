# $FreeBSD$

PROG_CXX=tblgen

SRCDIR=	utils/TableGen
SRCS=	ARMDecoderEmitter.cpp \
	AsmMatcherEmitter.cpp \
	AsmWriterEmitter.cpp \
	AsmWriterInst.cpp \
	CallingConvEmitter.cpp \
	ClangASTNodesEmitter.cpp \
	ClangAttrEmitter.cpp \
	ClangDiagnosticsEmitter.cpp \
	ClangSACheckersEmitter.cpp \
	CodeEmitterGen.cpp \
	CodeGenDAGPatterns.cpp \
	CodeGenInstruction.cpp \
	CodeGenTarget.cpp \
	DAGISelEmitter.cpp \
	DAGISelMatcher.cpp \
	DAGISelMatcherEmitter.cpp \
	DAGISelMatcherGen.cpp \
	DAGISelMatcherOpt.cpp \
	DisassemblerEmitter.cpp \
	EDEmitter.cpp \
	FastISelEmitter.cpp \
	FixedLenDecoderEmitter.cpp \
	InstrEnumEmitter.cpp \
	InstrInfoEmitter.cpp \
	IntrinsicEmitter.cpp \
	LLVMCConfigurationEmitter.cpp \
	NeonEmitter.cpp \
	OptParserEmitter.cpp \
	Record.cpp \
	RegisterInfoEmitter.cpp \
	StringMatcher.cpp \
	SubtargetEmitter.cpp \
	TGLexer.cpp \
	TGParser.cpp \
	TGValueTypes.cpp \
	TableGen.cpp \
	TableGenBackend.cpp \
	X86DisassemblerTables.cpp \
	X86RecognizableInstr.cpp
LLVM_REQUIRES_EH=

LIBDEPS=llvmsupport

.include "../clang.prog.mk"
