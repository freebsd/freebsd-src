# $FreeBSD$

LIB=	llvmmipsdesc

SRCDIR=	lib/Target/Mips/MCTargetDesc
SRCS=	MipsAsmBackend.cpp \
	MipsMCAsmInfo.cpp \
	MipsMCCodeEmitter.cpp \
	MipsMCTargetDesc.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	MipsGenInstrInfo \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
