# $FreeBSD$

LIB=	llvmmipscodegen

SRCDIR=	lib/Target/Mips
SRCS=	MipsDelaySlotFiller.cpp \
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
	MipsGenCallingConv \
	MipsGenDAGISel \
	MipsGenInstrInfo \
	MipsGenInstrNames \
	MipsGenRegisterInfo \
	MipsGenRegisterInfo.h \
	MipsGenRegisterNames \
	MipsGenSubtarget

.include "../clang.lib.mk"
