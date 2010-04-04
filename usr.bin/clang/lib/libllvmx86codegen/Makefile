# $FreeBSD$

LIB=	llvmx86codegen

SRCDIR=	lib/Target/X86
SRCS=	SSEDomainFix.cpp X86AsmBackend.cpp X86COFFMachineModuleInfo.cpp \
	X86CodeEmitter.cpp X86ELFWriterInfo.cpp \
	X86FastISel.cpp X86FloatingPoint.cpp X86FloatingPointRegKill.cpp \
	X86ISelDAGToDAG.cpp X86ISelLowering.cpp X86InstrInfo.cpp \
	X86JITInfo.cpp X86MCAsmInfo.cpp X86MCCodeEmitter.cpp \
	X86RegisterInfo.cpp X86Subtarget.cpp \
	X86TargetMachine.cpp X86TargetObjectFile.cpp

TGHDRS=	Intrinsics X86GenCallingConv X86GenDAGISel X86GenFastISel \
	X86GenInstrInfo X86GenInstrNames X86GenRegisterInfo.h \
	X86GenRegisterInfo X86GenRegisterNames X86GenSubtarget

.include "../../clang.lib.mk"
