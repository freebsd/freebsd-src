# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmx86codegen

SRCDIR=	lib/Target/X86
SRCS=	X86AsmPrinter.cpp \
	X86CallFrameOptimization.cpp \
	X86ExpandPseudo.cpp \
	X86FastISel.cpp \
	X86FixupBWInsts.cpp \
	X86FixupLEAs.cpp \
	X86FixupSetCC.cpp \
	X86FloatingPoint.cpp \
	X86FrameLowering.cpp \
	X86ISelDAGToDAG.cpp \
	X86ISelLowering.cpp \
	X86InstrInfo.cpp \
	X86MCInstLower.cpp \
	X86MachineFunctionInfo.cpp \
	X86OptimizeLEAs.cpp \
	X86PadShortFunction.cpp \
	X86RegisterInfo.cpp \
	X86SelectionDAGInfo.cpp \
	X86ShuffleDecodeConstantPool.cpp \
	X86Subtarget.cpp \
	X86TargetMachine.cpp \
	X86TargetObjectFile.cpp \
	X86TargetTransformInfo.cpp \
	X86VZeroUpper.cpp \
	X86WinAllocaExpander.cpp \
	X86WinEHState.cpp

TGHDRS=	Attributes \
	Intrinsics \
	X86GenCallingConv \
	X86GenDAGISel \
	X86GenFastISel \
	X86GenInstrInfo \
	X86GenRegisterInfo \
	X86GenSubtargetInfo

.include "../clang.lib.mk"
