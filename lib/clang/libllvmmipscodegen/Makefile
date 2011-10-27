# $FreeBSD$

LIB=	llvmmipscodegen

SRCDIR=	lib/Target/Mips
SRCS=	MipsAsmPrinter.cpp \
	MipsCodeEmitter.cpp \
	MipsDelaySlotFiller.cpp \
	MipsEmitGPRestore.cpp \
	MipsExpandPseudo.cpp \
	MipsFrameLowering.cpp \
	MipsISelDAGToDAG.cpp \
	MipsISelLowering.cpp \
	MipsInstrInfo.cpp \
	MipsJITInfo.cpp \
	MipsMCInstLower.cpp \
	MipsMCSymbolRefExpr.cpp \
	MipsRegisterInfo.cpp \
	MipsSelectionDAGInfo.cpp \
	MipsSubtarget.cpp \
	MipsTargetMachine.cpp \
	MipsTargetObjectFile.cpp

TGHDRS=	Intrinsics \
	MipsGenAsmWriter \
	MipsGenCallingConv \
	MipsGenDAGISel \
	MipsGenInstrInfo \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
