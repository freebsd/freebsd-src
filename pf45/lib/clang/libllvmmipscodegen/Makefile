# $FreeBSD$

LIB=	llvmmipscodegen

SRCDIR=	lib/Target/Mips
SRCS=	MipsAsmPrinter.cpp \
	MipsDelaySlotFiller.cpp \
	MipsExpandPseudo.cpp \
	MipsFrameLowering.cpp \
	MipsISelDAGToDAG.cpp \
	MipsISelLowering.cpp \
	MipsInstrInfo.cpp \
	MipsMCAsmInfo.cpp \
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
	MipsGenInstrNames \
	MipsGenRegisterInfo \
	MipsGenRegisterInfo.h \
	MipsGenRegisterNames \
	MipsGenSubtarget

.include "../clang.lib.mk"
