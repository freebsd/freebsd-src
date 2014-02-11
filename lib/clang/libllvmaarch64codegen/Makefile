# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmaarch64codegen

SRCDIR=	lib/Target/AArch64
SRCS=	AArch64AsmPrinter.cpp \
	AArch64BranchFixupPass.cpp \
	AArch64FrameLowering.cpp \
	AArch64ISelDAGToDAG.cpp \
	AArch64ISelLowering.cpp \
	AArch64InstrInfo.cpp \
	AArch64MCInstLower.cpp \
	AArch64MachineFunctionInfo.cpp \
	AArch64RegisterInfo.cpp \
	AArch64SelectionDAGInfo.cpp \
	AArch64Subtarget.cpp \
	AArch64TargetMachine.cpp \
	AArch64TargetObjectFile.cpp

TGHDRS=	AArch64GenCallingConv \
	AArch64GenDAGISel \
	AArch64GenInstrInfo \
	AArch64GenMCPseudoLowering \
	AArch64GenRegisterInfo \
	AArch64GenSubtargetInfo

.include "../clang.lib.mk"
