# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmsparccodegen

SRCDIR=	lib/Target/Sparc
SRCS=	DelaySlotFiller.cpp \
	SparcAsmPrinter.cpp \
	SparcFrameLowering.cpp \
	SparcISelDAGToDAG.cpp \
	SparcISelLowering.cpp \
	SparcInstrInfo.cpp \
	SparcMCInstLower.cpp \
	SparcMachineFunctionInfo.cpp \
	SparcRegisterInfo.cpp \
	SparcSubtarget.cpp \
	SparcTargetMachine.cpp \
	SparcTargetObjectFile.cpp

TGHDRS=	Attributes \
	Intrinsics \
	SparcGenAsmWriter \
	SparcGenCallingConv \
	SparcGenCodeEmitter \
	SparcGenDAGISel \
	SparcGenInstrInfo \
	SparcGenMCCodeEmitter \
	SparcGenRegisterInfo \
	SparcGenSubtargetInfo

.include "../clang.lib.mk"
