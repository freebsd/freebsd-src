# $FreeBSD$

LIB=	llvmarmcodegen

SRCDIR=	lib/Target/ARM
SRCS=	ARMAsmBackend.cpp \
	ARMAsmPrinter.cpp \
	ARMBaseInstrInfo.cpp \
	ARMBaseRegisterInfo.cpp \
	ARMCodeEmitter.cpp \
	ARMConstantIslandPass.cpp \
	ARMConstantPoolValue.cpp \
	ARMELFWriterInfo.cpp \
	ARMExpandPseudoInsts.cpp \
	ARMFastISel.cpp \
	ARMFrameLowering.cpp \
	ARMGlobalMerge.cpp \
	ARMHazardRecognizer.cpp \
	ARMISelDAGToDAG.cpp \
	ARMISelLowering.cpp \
	ARMInstrInfo.cpp \
	ARMJITInfo.cpp \
	ARMMCCodeEmitter.cpp \
	ARMMCExpr.cpp \
	ARMLoadStoreOptimizer.cpp \
	ARMMCAsmInfo.cpp \
	ARMMCInstLower.cpp \
	ARMRegisterInfo.cpp \
	ARMSelectionDAGInfo.cpp \
	ARMSubtarget.cpp \
	ARMTargetMachine.cpp \
	ARMTargetObjectFile.cpp \
	MLxExpansionPass.cpp \
	NEONMoveFix.cpp \
	Thumb1FrameLowering.cpp \
	Thumb1InstrInfo.cpp \
	Thumb1RegisterInfo.cpp \
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
	ARMGenMCCodeEmitter \
	ARMGenRegisterInfo \
	ARMGenRegisterInfo.h \
	ARMGenRegisterNames \
	ARMGenSubtarget \
	Intrinsics

.include "../clang.lib.mk"
