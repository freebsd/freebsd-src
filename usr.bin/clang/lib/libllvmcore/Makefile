# $FreeBSD$

LIB=	llvmcore

SRCDIR=	lib/VMCore
SRCS=	AsmWriter.cpp Attributes.cpp AutoUpgrade.cpp BasicBlock.cpp \
	ConstantFold.cpp Constants.cpp Core.cpp DebugLoc.cpp Dominators.cpp \
	Function.cpp Globals.cpp GVMaterializer.cpp InlineAsm.cpp Instruction.cpp \
	Instructions.cpp IntrinsicInst.cpp IRBuilder.cpp LLVMContext.cpp \
	LLVMContextImpl.cpp LeakDetector.cpp Metadata.cpp \
	Module.cpp Pass.cpp PassManager.cpp \
	PrintModulePass.cpp Type.cpp TypeSymbolTable.cpp Use.cpp \
	Value.cpp ValueSymbolTable.cpp ValueTypes.cpp Verifier.cpp

REQUIRES_RTTI=yo

TGHDRS=	Intrinsics

.include "../../clang.lib.mk"
