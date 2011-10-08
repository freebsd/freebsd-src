# $FreeBSD$

LIB=	llvmx86codegen

SRCDIR=	lib/Target/X86
SRCS=	SSEDomainFix.cpp \
	X86AsmBackend.cpp \
	X86AsmPrinter.cpp \
	X86COFFMachineModuleInfo.cpp \
	X86CodeEmitter.cpp \
	X86ELFWriterInfo.cpp \
	X86FastISel.cpp \
	X86FloatingPoint.cpp \
	X86FrameLowering.cpp \
	X86ISelDAGToDAG.cpp \
	X86ISelLowering.cpp \
	X86InstrInfo.cpp \
	X86JITInfo.cpp \
	X86MCCodeEmitter.cpp \
	X86MCInstLower.cpp \
	X86MachObjectWriter.cpp \
	X86RegisterInfo.cpp \
	X86SelectionDAGInfo.cpp \
	X86Subtarget.cpp \
	X86TargetMachine.cpp \
	X86TargetObjectFile.cpp

TGHDRS=	Intrinsics \
	X86GenCallingConv \
	X86GenDAGISel \
	X86GenFastISel \
	X86GenInstrInfo \
	X86GenRegisterInfo \
	X86GenSubtargetInfo

.include "../clang.lib.mk"
