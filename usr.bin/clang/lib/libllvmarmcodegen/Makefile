# $FreeBSD$

LIB=	llvmarmcodegen

SRCDIR=	lib/Target/ARM
SRCS=	ARMCodeEmitter.cpp ARMConstantIslandPass.cpp \
	ARMConstantPoolValue.cpp ARMISelDAGToDAG.cpp \
	ARMISelLowering.cpp ARMInstrInfo.cpp ARMJITInfo.cpp \
	ARMLoadStoreOptimizer.cpp ARMRegisterInfo.cpp  \
	ARMSubtarget.cpp ARMTargetAsmInfo.cpp ARMTargetMachine.cpp \
	ThumbInstrInfo.cpp

TGHDRS=	ARMGenCallingConv ARMGenCodeEmitter ARMGenDAGISel \
	ARMGenInstrInfo ARMGenInstrNames ARMGenRegisterInfo.h \
	ARMGenRegisterInfo ARMGenRegisterNames ARMGenSubtarget \
	Intrinsics

.include "../../clang.lib.mk"
