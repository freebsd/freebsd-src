# $FreeBSD$

MAN=
.include <src.opts.mk>

PROG_CXX=lld

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${LLD_SRCS}/include
CFLAGS+=-I${LLD_SRCS}/lib/ReaderWriter/ELF
CFLAGS+=-I${LLD_SRCS}/lib/ReaderWriter/ELF/X86_64

SRCDIR=	tools/lld
SRCS=	lib/Config/Version.cpp \
	lib/Core/DefinedAtom.cpp \
	lib/Core/Error.cpp \
	lib/Core/File.cpp \
	lib/Core/LinkingContext.cpp \
	lib/Core/Reader.cpp \
	lib/Core/Resolver.cpp \
	lib/Core/SymbolTable.cpp \
	lib/Core/Writer.cpp \
	lib/Driver/CoreDriver.cpp \
	lib/Driver/DarwinLdDriver.cpp \
	lib/Driver/Driver.cpp \
	lib/Driver/GnuLdDriver.cpp \
	lib/Driver/UniversalDriver.cpp \
	lib/Driver/WinLinkDriver.cpp \
	lib/Driver/WinLinkModuleDef.cpp \
	lib/ReaderWriter/CoreLinkingContext.cpp \
	lib/ReaderWriter/ELF/AArch64/AArch64LinkingContext.cpp \
	lib/ReaderWriter/ELF/AArch64/AArch64RelocationHandler.cpp \
	lib/ReaderWriter/ELF/AArch64/AArch64RelocationPass.cpp \
	lib/ReaderWriter/ELF/AArch64/AArch64TargetHandler.cpp \
	lib/ReaderWriter/ELF/ARM/ARMLinkingContext.cpp \
	lib/ReaderWriter/ELF/ARM/ARMRelocationHandler.cpp \
	lib/ReaderWriter/ELF/ARM/ARMRelocationPass.cpp \
	lib/ReaderWriter/ELF/ARM/ARMTargetHandler.cpp \
	lib/ReaderWriter/ELF/ELFLinkingContext.cpp \
	lib/ReaderWriter/ELF/Hexagon/HexagonLinkingContext.cpp \
	lib/ReaderWriter/ELF/Hexagon/HexagonRelocationHandler.cpp \
	lib/ReaderWriter/ELF/Hexagon/HexagonTargetHandler.cpp \
	lib/ReaderWriter/ELF/Mips/MipsCtorsOrderPass.cpp \
	lib/ReaderWriter/ELF/Mips/MipsELFFlagsMerger.cpp \
	lib/ReaderWriter/ELF/Mips/MipsLinkingContext.cpp \
	lib/ReaderWriter/ELF/Mips/MipsRelocationHandler.cpp \
	lib/ReaderWriter/ELF/Mips/MipsRelocationPass.cpp \
	lib/ReaderWriter/ELF/Mips/MipsTargetHandler.cpp \
	lib/ReaderWriter/ELF/Reader.cpp \
	lib/ReaderWriter/ELF/Writer.cpp \
	lib/ReaderWriter/ELF/X86/X86LinkingContext.cpp \
	lib/ReaderWriter/ELF/X86/X86RelocationHandler.cpp \
	lib/ReaderWriter/ELF/X86/X86TargetHandler.cpp \
	lib/ReaderWriter/ELF/X86_64/ExampleSubTarget/ExampleLinkingContext.cpp \
	lib/ReaderWriter/ELF/X86_64/ExampleSubTarget/ExampleTargetHandler.cpp \
	lib/ReaderWriter/ELF/X86_64/X86_64LinkingContext.cpp \
	lib/ReaderWriter/ELF/X86_64/X86_64RelocationHandler.cpp \
	lib/ReaderWriter/ELF/X86_64/X86_64RelocationPass.cpp \
	lib/ReaderWriter/ELF/X86_64/X86_64TargetHandler.cpp \
	lib/ReaderWriter/FileArchive.cpp \
	lib/ReaderWriter/LinkerScript.cpp \
	lib/ReaderWriter/MachO/ArchHandler.cpp \
	lib/ReaderWriter/MachO/ArchHandler_arm.cpp \
	lib/ReaderWriter/MachO/ArchHandler_arm64.cpp \
	lib/ReaderWriter/MachO/ArchHandler_x86.cpp \
	lib/ReaderWriter/MachO/ArchHandler_x86_64.cpp \
	lib/ReaderWriter/MachO/CompactUnwindPass.cpp \
	lib/ReaderWriter/MachO/GOTPass.cpp \
	lib/ReaderWriter/MachO/LayoutPass.cpp \
	lib/ReaderWriter/MachO/MachOLinkingContext.cpp \
	lib/ReaderWriter/MachO/MachONormalizedFileBinaryReader.cpp \
	lib/ReaderWriter/MachO/MachONormalizedFileBinaryWriter.cpp \
	lib/ReaderWriter/MachO/MachONormalizedFileFromAtoms.cpp \
	lib/ReaderWriter/MachO/MachONormalizedFileToAtoms.cpp \
	lib/ReaderWriter/MachO/MachONormalizedFileYAML.cpp \
	lib/ReaderWriter/MachO/ShimPass.cpp \
	lib/ReaderWriter/MachO/StubsPass.cpp \
	lib/ReaderWriter/MachO/WriterMachO.cpp \
	lib/ReaderWriter/Native/ReaderNative.cpp \
	lib/ReaderWriter/Native/WriterNative.cpp \
	lib/ReaderWriter/PECOFF/EdataPass.cpp \
	lib/ReaderWriter/PECOFF/IdataPass.cpp \
	lib/ReaderWriter/PECOFF/LinkerGeneratedSymbolFile.cpp \
	lib/ReaderWriter/PECOFF/LoadConfigPass.cpp \
	lib/ReaderWriter/PECOFF/PECOFFLinkingContext.cpp \
	lib/ReaderWriter/PECOFF/Pass.cpp \
	lib/ReaderWriter/PECOFF/ReaderCOFF.cpp \
	lib/ReaderWriter/PECOFF/ReaderImportHeader.cpp \
	lib/ReaderWriter/PECOFF/WriterImportLibrary.cpp \
	lib/ReaderWriter/PECOFF/WriterPECOFF.cpp \
	lib/ReaderWriter/YAML/ReaderWriterYAML.cpp \
	tools/lld/lld.cpp

.if ${MK_SHARED_TOOLCHAIN} == "no"
NO_SHARED?= yes
.endif

TGHDRS=	CoreOptions \
	DarwinLdOptions \
	GnuLdOptions \
	UniversalDriverOptions \
	WinLinkOptions

LIBDEPS=llvmoption \
	llvmobject \
	llvmbitreader \
	llvmcore \
	llvmmcparser \
	llvmmc \
	llvmsupport

.include "../clang.prog.mk"
