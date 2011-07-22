# $FreeBSD$

LIB=	llvmia64codegen

SRCDIR=	lib/Target/IA64
SRCS=	\
	IA64AsmPrinter.cpp \
	IA64DAGToDAGISel.cpp \
	IA64FrameLowering.cpp \
	IA64InstrInfo.cpp \
	IA64TargetLowering.cpp \
	IA64MCInstLower.cpp \
	IA64RegisterInfo.cpp \
	IA64SelectionDAGInfo.cpp \
	IA64Subtarget.cpp \
	IA64TargetMachine.cpp

TGHDRS=	\
	Intrinsics \
	IA64GenCallingConv \
	IA64GenDAGISel \
	IA64GenInstrInfo \
	IA64GenRegisterInfo \
	IA64GenSubtargetInfo

.include "../clang.lib.mk"
