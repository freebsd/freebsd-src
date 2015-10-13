# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmarmcodegen

SRCDIR=	lib/Target/ARM
SRCS=	A15SDOptimizer.cpp \
	ARMAsmPrinter.cpp \
	ARMBaseInstrInfo.cpp \
	ARMBaseRegisterInfo.cpp \
	ARMConstantIslandPass.cpp \
	ARMConstantPoolValue.cpp \
	ARMExpandPseudoInsts.cpp \
	ARMFastISel.cpp \
	ARMFrameLowering.cpp \
	ARMHazardRecognizer.cpp \
	ARMISelDAGToDAG.cpp \
	ARMISelLowering.cpp \
	ARMInstrInfo.cpp \
	ARMLoadStoreOptimizer.cpp \
	ARMMCInstLower.cpp \
	ARMMachineFunctionInfo.cpp \
	ARMOptimizeBarriersPass.cpp \
	ARMRegisterInfo.cpp \
	ARMSelectionDAGInfo.cpp \
	ARMSubtarget.cpp \
	ARMTargetMachine.cpp \
	ARMTargetObjectFile.cpp \
	ARMTargetTransformInfo.cpp \
	MLxExpansionPass.cpp \
	Thumb1FrameLowering.cpp \
	Thumb1InstrInfo.cpp \
	Thumb2ITBlockPass.cpp \
	Thumb2InstrInfo.cpp \
	Thumb2SizeReduction.cpp \
	ThumbRegisterInfo.cpp

TGHDRS=	ARMGenAsmWriter \
	ARMGenCallingConv \
	ARMGenCodeEmitter \
	ARMGenDAGISel \
	ARMGenFastISel \
	ARMGenInstrInfo \
	ARMGenMCCodeEmitter \
	ARMGenMCPseudoLowering \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo \
	Intrinsics

.include "../clang.lib.mk"
