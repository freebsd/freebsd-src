# $FreeBSD$

LIB=	llvmia64codegen

SRCDIR=	lib/Target/IA64
SRCS=	\
	IA64FrameLowering.cpp \
	IA64InstrInfo.cpp \
	IA64TargetLowering.cpp \
	IA64MCAsmInfo.cpp \
	IA64RegisterInfo.cpp \
	IA64SelectionDAGInfo.cpp \
	IA64Subtarget.cpp \
	IA64TargetMachine.cpp

TGHDRS=	\
	Intrinsics \
	IA64GenInstrInfo \
	IA64GenInstrNames \
	IA64GenRegisterInfo \
	IA64GenRegisterInfo.h \
	IA64GenRegisterNames \
	IA64GenSubtarget

.include "../clang.lib.mk"
