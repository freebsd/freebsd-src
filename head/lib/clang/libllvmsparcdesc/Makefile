# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmsparcdesc

SRCDIR=	lib/Target/Sparc/MCTargetDesc
SRCS=	SparcAsmBackend.cpp \
	SparcELFObjectWriter.cpp \
	SparcMCAsmInfo.cpp \
	SparcMCCodeEmitter.cpp \
	SparcMCExpr.cpp \
	SparcMCTargetDesc.cpp \
	SparcTargetStreamer.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	SparcGenInstrInfo \
	SparcGenMCCodeEmitter \
	SparcGenRegisterInfo \
	SparcGenSubtargetInfo

.include "../clang.lib.mk"
