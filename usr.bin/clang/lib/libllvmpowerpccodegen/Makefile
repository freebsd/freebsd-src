# $FreeBSD$

LIB=	llvmpowerpccodegen

SRCDIR=	lib/Target/PowerPC
SRCS=	PPCBranchSelector.cpp PPCCodeEmitter.cpp \
	PPCHazardRecognizers.cpp PPCISelDAGToDAG.cpp \
	PPCISelLowering.cpp PPCInstrInfo.cpp PPCJITInfo.cpp \
	PPCMachOWriterInfo.cpp PPCPredicates.cpp PPCRegisterInfo.cpp \
	PPCSubtarget.cpp PPCTargetAsmInfo.cpp PPCTargetMachine.cpp

TGHDRS=	Intrinsics PPCGenCallingConv PPCGenCodeEmitter PPCGenDAGISel \
	PPCGenInstrInfo PPCGenInstrNames PPCGenRegisterInfo.h \
	PPCGenRegisterInfo PPCGenRegisterNames PPCGenSubtarget

MAKEOBJ=

.include "../../clang.lib.mk"
