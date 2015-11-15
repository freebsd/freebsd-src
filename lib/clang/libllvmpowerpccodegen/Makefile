# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmpowerpccodegen

SRCDIR=	lib/Target/PowerPC
SRCS=	PPCAsmPrinter.cpp \
	PPCBranchSelector.cpp \
	PPCCTRLoops.cpp \
	PPCEarlyReturn.cpp \
 	PPCFastISel.cpp \
	PPCFrameLowering.cpp \
	PPCHazardRecognizers.cpp \
	PPCISelDAGToDAG.cpp \
	PPCISelLowering.cpp \
	PPCInstrInfo.cpp \
	PPCLoopDataPrefetch.cpp \
	PPCLoopPreIncPrep.cpp \
	PPCMCInstLower.cpp \
	PPCMachineFunctionInfo.cpp \
	PPCRegisterInfo.cpp \
	PPCSubtarget.cpp \
	PPCTLSDynamicCall.cpp \
	PPCTOCRegDeps.cpp \
	PPCTargetMachine.cpp \
	PPCTargetObjectFile.cpp \
	PPCTargetTransformInfo.cpp \
	PPCVSXCopy.cpp \
	PPCVSXFMAMutate.cpp \
	PPCVSXSwapRemoval.cpp

TGHDRS=	Intrinsics \
	PPCGenCallingConv \
	PPCGenCodeEmitter \
	PPCGenDAGISel \
	PPCGenFastISel \
	PPCGenInstrInfo \
	PPCGenMCCodeEmitter \
	PPCGenRegisterInfo \
	PPCGenSubtargetInfo

.include "../clang.lib.mk"
