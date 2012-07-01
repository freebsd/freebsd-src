# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmmc

SRCDIR=	lib/MC
SRCS=	ELFObjectWriter.cpp \
	MCAsmBackend.cpp \
	MCAsmInfo.cpp \
	MCAsmInfoCOFF.cpp \
	MCAsmInfoDarwin.cpp \
	MCAsmStreamer.cpp \
	MCAssembler.cpp \
	MCCodeGenInfo.cpp \
	MCCodeEmitter.cpp \
	MCContext.cpp \
	MCDwarf.cpp \
	MCELF.cpp \
	MCELFObjectTargetWriter.cpp \
	MCELFStreamer.cpp \
	MCExpr.cpp \
	MCInst.cpp \
	MCInstPrinter.cpp \
	MCInstrAnalysis.cpp \
	MCMachOStreamer.cpp \
	MCMachObjectTargetWriter.cpp \
	MCNullStreamer.cpp \
	MCObjectFileInfo.cpp \
	MCObjectStreamer.cpp \
	MCObjectWriter.cpp \
	MCPureStreamer.cpp \
	MCSection.cpp \
	MCSectionCOFF.cpp \
	MCSectionELF.cpp \
	MCSectionMachO.cpp \
	MCStreamer.cpp \
	MCSubtargetInfo.cpp \
	MCSymbol.cpp \
	MCTargetAsmLexer.cpp \
	MCWin64EH.cpp \
	MachObjectWriter.cpp \
	SubtargetFeature.cpp \
	WinCOFFObjectWriter.cpp \
	WinCOFFStreamer.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	MCDisassembler.cpp
.endif

.include "../clang.lib.mk"
