# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmmc

SRCDIR=	lib/MC
SRCS=	ConstantPools.cpp \
	ELFObjectWriter.cpp \
	MCAsmBackend.cpp \
	MCAsmInfo.cpp \
	MCAsmInfoCOFF.cpp \
	MCAsmInfoDarwin.cpp \
	MCAsmInfoELF.cpp \
	MCAsmStreamer.cpp \
	MCAssembler.cpp \
	MCCodeEmitter.cpp \
	MCCodeGenInfo.cpp \
	MCContext.cpp \
	MCDwarf.cpp \
	MCELF.cpp \
	MCELFObjectTargetWriter.cpp \
	MCELFStreamer.cpp \
	MCExpr.cpp \
	MCExternalSymbolizer.cpp \
	MCInst.cpp \
	MCInstPrinter.cpp \
	MCInstrAnalysis.cpp \
	MCLabel.cpp \
	MCLinkerOptimizationHint.cpp \
	MCMachOStreamer.cpp \
	MCMachObjectTargetWriter.cpp \
	MCNullStreamer.cpp \
	MCObjectFileInfo.cpp \
	MCObjectStreamer.cpp \
	MCObjectWriter.cpp \
	MCRegisterInfo.cpp \
	MCRelocationInfo.cpp \
	MCSection.cpp \
	MCSectionCOFF.cpp \
	MCSectionELF.cpp \
	MCSectionMachO.cpp \
	MCStreamer.cpp \
	MCSubtargetInfo.cpp \
	MCSymbol.cpp \
	MCSymbolizer.cpp \
	MCTargetOptions.cpp \
	MCValue.cpp \
	MCWin64EH.cpp \
	MachObjectWriter.cpp \
	StringTableBuilder.cpp \
	SubtargetFeature.cpp \
	WinCOFFObjectWriter.cpp \
	WinCOFFStreamer.cpp \
	YAML.cpp

.if ${MK_CLANG_EXTRAS} != "no" || ${MK_LLDB} != "no"
SRCS+=	MCDisassembler.cpp
.endif

.include "../clang.lib.mk"
