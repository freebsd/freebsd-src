# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmmipscodegen

SRCDIR=	lib/Target/Mips
SRCS=	Mips16FrameLowering.cpp \
	Mips16HardFloat.cpp \
	Mips16HardFloatInfo.cpp \
	Mips16ISelDAGToDAG.cpp \
	Mips16ISelLowering.cpp \
	Mips16InstrInfo.cpp \
	Mips16RegisterInfo.cpp \
	MipsAnalyzeImmediate.cpp \
	MipsAsmPrinter.cpp \
	MipsCCState.cpp \
	MipsConstantIslandPass.cpp \
	MipsDelaySlotFiller.cpp \
	MipsFastISel.cpp \
	MipsFrameLowering.cpp \
	MipsHazardSchedule.cpp \
	MipsISelDAGToDAG.cpp \
	MipsISelLowering.cpp \
	MipsInstrInfo.cpp \
	MipsLongBranch.cpp \
	MipsMCInstLower.cpp \
	MipsMachineFunction.cpp \
	MipsModuleISelDAGToDAG.cpp \
	MipsOptimizePICCall.cpp \
	MipsOs16.cpp \
	MipsRegisterInfo.cpp \
	MipsSEFrameLowering.cpp \
	MipsSEISelDAGToDAG.cpp \
	MipsSEISelLowering.cpp \
	MipsSEInstrInfo.cpp \
	MipsSERegisterInfo.cpp \
	MipsSubtarget.cpp \
	MipsTargetMachine.cpp \
	MipsTargetObjectFile.cpp

TGHDRS=	Attributes \
	Intrinsics \
	MipsGenAsmWriter \
	MipsGenCallingConv \
	MipsGenCodeEmitter \
	MipsGenDAGISel \
	MipsGenFastISel \
	MipsGenInstrInfo \
	MipsGenMCPseudoLowering \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
