# $FreeBSD$

PROG_CXX=tblgen

SRCDIR=	utils/TableGen
SRCS=	AsmMatcherEmitter.cpp AsmWriterEmitter.cpp CallingConvEmitter.cpp \
	ClangDiagnosticsEmitter.cpp CodeEmitterGen.cpp \
	CodeGenDAGPatterns.cpp CodeGenInstruction.cpp \
	CodeGenTarget.cpp DAGISelEmitter.cpp FastISelEmitter.cpp \
	InstrEnumEmitter.cpp InstrInfoEmitter.cpp IntrinsicEmitter.cpp \
	LLVMCConfigurationEmitter.cpp OptParserEmitter.cpp Record.cpp \
	RegisterInfoEmitter.cpp SubtargetEmitter.cpp TGLexer.cpp \
	TGParser.cpp TGValueTypes.cpp TableGen.cpp TableGenBackend.cpp

NO_MAN=
LIBDEPS=llvmsupport llvmsystem

.include "../../clang.prog.mk"
