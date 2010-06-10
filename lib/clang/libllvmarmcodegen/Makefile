# $FreeBSD$

LIB=	llvmarmcodegen

SRCDIR=	lib/Target/ARM
SRCS=	ARMBaseInstrInfo.cpp ARMBaseRegisterInfo.cpp \
	ARMCodeEmitter.cpp ARMConstantIslandPass.cpp \
	ARMConstantPoolValue.cpp ARMExpandPseudoInsts.cpp \
	ARMISelDAGToDAG.cpp ARMISelLowering.cpp ARMInstrInfo.cpp \
	ARMJITInfo.cpp ARMLoadStoreOptimizer.cpp \
	ARMLoadStoreOptimizer.cpp ARMMCAsmInfo.cpp ARMRegisterInfo.cpp \
	ARMSelectionDAGInfo.cpp ARMSubtarget.cpp ARMTargetMachine.cpp \
	ARMTargetObjectFile.cpp NEONMoveFix.cpp NEONPreAllocPass.cpp \
	Thumb1InstrInfo.cpp Thumb1RegisterInfo.cpp \
	Thumb2ITBlockPass.cpp Thumb2InstrInfo.cpp \
	Thumb2RegisterInfo.cpp Thumb2SizeReduction.cpp

TGHDRS=	ARMGenCallingConv ARMGenCodeEmitter ARMGenDAGISel \
	ARMGenInstrInfo ARMGenInstrNames ARMGenRegisterInfo.h \
	ARMGenRegisterInfo ARMGenRegisterNames ARMGenSubtarget \
	Intrinsics

.include "../clang.lib.mk"
