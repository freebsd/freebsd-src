# $FreeBSD$

LIB=	llvmx86desc

SRCDIR=	lib/Target/X86/MCTargetDesc
SRCS=	X86AsmBackend.cpp \
	X86MachObjectWriter.cpp \
	X86MCAsmInfo.cpp \
	X86MCCodeEmitter.cpp \
	X86MCTargetDesc.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	X86GenInstrInfo \
	X86GenRegisterInfo \
	X86GenSubtargetInfo

.include "../clang.lib.mk"
