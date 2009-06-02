# $FreeBSD$

LIB=	llvmmipscodegen

SRCDIR=	lib/Target/Mips
SRCS=	MipsDelaySlotFiller.cpp MipsISelDAGToDAG.cpp \
	MipsISelLowering.cpp MipsInstrInfo.cpp MipsRegisterInfo.cpp \
	MipsSubtarget.cpp MipsTargetAsmInfo.cpp MipsTargetMachine.cpp

TGHDRS=	Intrinsics MipsGenAsmWriter MipsGenCallingConv MipsGenDAGISel \
	MipsGenInstrInfo MipsGenInstrNames MipsGenRegisterInfo.h \
	MipsGenRegisterInfo MipsGenRegisterNames MipsGenSubtarget

MAKEOBJ=

.include "../../clang.lib.mk"
