# $FreeBSD$

LIB=	llvmarmcodegen

SRCDIR=	lib/Target/ARM
SRCS=	ARMCodeEmitter.cpp ARMConstantIslandPass.cpp \
	ARMConstantPoolValue.cpp ARMISelDAGToDAG.cpp \
	ARMISelLowering.cpp ARMInstrInfo.cpp ARMJITInfo.cpp \
	ARMLoadStoreOptimizer.cpp ARMRegisterInfo.cpp  \
	ARMSubtarget.cpp ARMTargetAsmInfo.cpp ARMTargetMachine.cpp

TGHDRS=	ARMGenCallingConv ARMGenCodeEmitter ARMGenDAGISel \
	ARMGenInstrInfo ARMGenInstrNames ARMGenRegisterInfo.h \
	ARMGenRegisterInfo ARMGenRegisterNames ARMGenSubtarget \
	Intrinsics

MAKEOBJ=

.include "../../clang.lib.mk"
