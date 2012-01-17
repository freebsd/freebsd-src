# $FreeBSD$

LIB=	llvmia64desc

SRCDIR=	lib/Target/IA64/MCTargetDesc
SRCS=	IA64MCAsmInfo.cpp \
	IA64MCTargetDesc.cpp

CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	IA64GenInstrInfo \
	IA64GenRegisterInfo \
	IA64GenSubtargetInfo

.include "../clang.lib.mk"
