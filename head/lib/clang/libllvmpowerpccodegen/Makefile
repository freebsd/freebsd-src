# $FreeBSD$

LIB=	llvmpowerpccodegen

SRCDIR=	lib/Target/PowerPC
SRCS=	PPCAsmBackend.cpp \
	PPCAsmPrinter.cpp \
	PPCBranchSelector.cpp \
	PPCCodeEmitter.cpp \
	PPCFrameLowering.cpp \
	PPCHazardRecognizers.cpp \
	PPCISelDAGToDAG.cpp \
	PPCISelLowering.cpp \
	PPCInstrInfo.cpp \
	PPCJITInfo.cpp \
	PPCMCAsmInfo.cpp \
	PPCMCCodeEmitter.cpp \
	PPCMCInstLower.cpp \
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
	PPCGenMCCodeEmitter \
	PPCGenRegisterInfo \
	PPCGenRegisterInfo.h \
	PPCGenRegisterNames \
	PPCGenSubtarget

.include "../clang.lib.mk"
