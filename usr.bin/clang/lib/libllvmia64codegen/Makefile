# $FreeBSD$

LIB=	llvmia64codegen

SRCDIR=	lib/Target/IA64
SRCS=	IA64Bundling.cpp IA64ISelDAGToDAG.cpp IA64ISelLowering.cpp \
	IA64InstrInfo.cpp IA64RegisterInfo.cpp IA64Subtarget.cpp \
	IA64TargetAsmInfo.cpp IA64TargetMachine.cpp

TGHDRS=	IA64GenDAGISel IA64GenInstrInfo IA64GenInstrNames \
	IA64GenRegisterInfo.h IA64GenRegisterInfo IA64GenRegisterNames \
	Intrinsics

MAKEOBJ=

.include "../../clang.lib.mk"
