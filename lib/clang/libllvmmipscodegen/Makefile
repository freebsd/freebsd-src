# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmmipscodegen

SRCDIR=	lib/Target/Mips
SRCS=	Mips16FrameLowering.cpp \
	Mips16InstrInfo.cpp \
	Mips16RegisterInfo.cpp \
	MipsAnalyzeImmediate.cpp \
	MipsAsmPrinter.cpp \
	MipsCodeEmitter.cpp \
	MipsDelaySlotFiller.cpp \
	MipsFrameLowering.cpp \
	MipsISelDAGToDAG.cpp \
	MipsISelLowering.cpp \
	MipsInstrInfo.cpp \
	MipsJITInfo.cpp \
	MipsLongBranch.cpp \
	MipsMCInstLower.cpp \
	MipsMachineFunction.cpp \
	MipsRegisterInfo.cpp \
	MipsSEFrameLowering.cpp \
	MipsSEInstrInfo.cpp \
	MipsSERegisterInfo.cpp \
	MipsSelectionDAGInfo.cpp \
	MipsSubtarget.cpp \
	MipsTargetMachine.cpp \
	MipsTargetObjectFile.cpp

TGHDRS=	Intrinsics \
	MipsGenAsmWriter \
	MipsGenCallingConv \
	MipsGenCodeEmitter \
	MipsGenDAGISel \
	MipsGenInstrInfo \
	MipsGenMCPseudoLowering \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
