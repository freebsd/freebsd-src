# $FreeBSD$

PROG_CXX=tblgen

SRCDIR=	utils/TableGen
SRCS=	ARMDecoderEmitter.cpp AsmMatcherEmitter.cpp \
	AsmWriterEmitter.cpp AsmWriterInst.cpp CallingConvEmitter.cpp \
	ClangDiagnosticsEmitter.cpp CodeEmitterGen.cpp \
	CodeGenDAGPatterns.cpp CodeGenInstruction.cpp \
	CodeGenTarget.cpp DAGISelEmitter.cpp DAGISelMatcher.cpp \
	DAGISelMatcherEmitter.cpp DAGISelMatcherGen.cpp \
	DAGISelMatcherOpt.cpp DisassemblerEmitter.cpp EDEmitter.cpp \
	FastISelEmitter.cpp InstrEnumEmitter.cpp InstrInfoEmitter.cpp \
	IntrinsicEmitter.cpp LLVMCConfigurationEmitter.cpp \
	OptParserEmitter.cpp Record.cpp RegisterInfoEmitter.cpp \
	SubtargetEmitter.cpp TGLexer.cpp TGParser.cpp TGValueTypes.cpp \
	TableGen.cpp TableGenBackend.cpp X86DisassemblerTables.cpp \
	X86RecognizableInstr.cpp
MAN=

LIBDEPS=llvmsupport llvmsystem
REQUIRES_RTTI=yo

.include "../../clang.prog.mk"
