# $FreeBSD$

LIB=	llvmpowerpccodegen

SRCDIR=	lib/Target/PowerPC
SRCS=	PPCBranchSelector.cpp \
	PPCCodeEmitter.cpp \
	PPCHazardRecognizers.cpp \
	PPCISelDAGToDAG.cpp \
	PPCISelLowering.cpp \
	PPCInstrInfo.cpp \
	PPCJITInfo.cpp \
	PPCMCAsmInfo.cpp \
	PPCPredicates.cpp \
	PPCRegisterInfo.cpp \
	PPCSelectionDAGInfo.cpp \
	PPCSubtarget.cpp \
	PPCTargetMachine.cpp

TGHDRS=	Intrinsics \
	PPCGenCallingConv \
	PPCGenCodeEmitter \
	PPCGenDAGISel \
	PPCGenInstrInfo \
	PPCGenInstrNames \
	PPCGenRegisterInfo \
	PPCGenRegisterInfo.h \
	PPCGenRegisterNames \
	PPCGenSubtarget

.include "../clang.lib.mk"
