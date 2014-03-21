# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmarmdesc

SRCDIR=	lib/Target/ARM/MCTargetDesc
SRCS=	ARMAsmBackend.cpp \
	ARMELFObjectWriter.cpp \
	ARMELFStreamer.cpp \
	ARMMCAsmInfo.cpp \
	ARMMCCodeEmitter.cpp \
	ARMMCExpr.cpp \
	ARMMCTargetDesc.cpp \
	ARMMachORelocationInfo.cpp \
	ARMMachObjectWriter.cpp \
	ARMUnwindOpAsm.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	ARMGenInstrInfo \
	ARMGenMCCodeEmitter \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo

.include "../clang.lib.mk"
