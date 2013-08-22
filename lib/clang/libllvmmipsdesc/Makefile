# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmmipsdesc

SRCDIR=	lib/Target/Mips/MCTargetDesc
SRCS=	MipsAsmBackend.cpp \
	MipsDirectObjLower.cpp \
	MipsELFObjectWriter.cpp \
	MipsMCAsmInfo.cpp \
	MipsMCCodeEmitter.cpp \
	MipsMCTargetDesc.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	MipsGenInstrInfo \
	MipsGenMCCodeEmitter \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
