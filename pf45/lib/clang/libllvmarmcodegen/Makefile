# $FreeBSD$

LIB=	llvmarmcodegen

SRCDIR=	lib/Target/ARM
SRCS=	ARMAsmPrinter.cpp \
	ARMBaseInstrInfo.cpp \
	ARMBaseRegisterInfo.cpp \
	ARMCodeEmitter.cpp \
	ARMConstantIslandPass.cpp \
	ARMConstantPoolValue.cpp \
	ARMExpandPseudoInsts.cpp \
	ARMFastISel.cpp \
	ARMGlobalMerge.cpp \
	ARMISelDAGToDAG.cpp \
	ARMISelLowering.cpp \
	ARMInstrInfo.cpp \
	ARMJITInfo.cpp \
	ARMLoadStoreOptimizer.cpp \
	ARMMCAsmInfo.cpp \
	ARMMCInstLower.cpp \
	ARMRegisterInfo.cpp \
	ARMSelectionDAGInfo.cpp \
	ARMSubtarget.cpp \
	ARMTargetMachine.cpp \
	ARMTargetObjectFile.cpp \
	NEONMoveFix.cpp \
	NEONPreAllocPass.cpp \
	Thumb1InstrInfo.cpp \
	Thumb1RegisterInfo.cpp \
	Thumb2HazardRecognizer.cpp \
	Thumb2ITBlockPass.cpp \
	Thumb2InstrInfo.cpp \
	Thumb2RegisterInfo.cpp \
	Thumb2SizeReduction.cpp

TGHDRS=	ARMGenAsmWriter \
	ARMGenCallingConv \
	ARMGenCodeEmitter \
	ARMGenDAGISel \
	ARMGenFastISel \
	ARMGenInstrInfo \
	ARMGenInstrNames \
	ARMGenRegisterInfo \
	ARMGenRegisterInfo.h \
	ARMGenRegisterNames \
	ARMGenSubtarget \
	Intrinsics

.include "../clang.lib.mk"
