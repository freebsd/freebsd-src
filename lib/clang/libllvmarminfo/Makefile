# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmarminfo

SRCDIR=	lib/Target/ARM/TargetInfo
INCDIR=	lib/Target/ARM
SRCS=	ARMTargetInfo.cpp

TGHDRS=	ARMGenInstrInfo \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo \
	Attributes

.include "../clang.lib.mk"
