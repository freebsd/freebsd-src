# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmaarch64desc

SRCDIR=	lib/Target/AArch64/MCTargetDesc
SRCS=	AArch64AsmBackend.cpp \
	AArch64ELFObjectWriter.cpp \
	AArch64ELFStreamer.cpp \
	AArch64MCAsmInfo.cpp \
	AArch64MCCodeEmitter.cpp \
	AArch64MCExpr.cpp \
	AArch64MCTargetDesc.cpp \
	AArch64MachObjectWriter.cpp \
	AArch64TargetStreamer.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	AArch64GenInstrInfo \
	AArch64GenMCCodeEmitter \
	AArch64GenRegisterInfo \
	AArch64GenSubtargetInfo \
	Attributes

.include "../clang.lib.mk"
