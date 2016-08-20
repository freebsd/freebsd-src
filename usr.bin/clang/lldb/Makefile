# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=lldb

LLDB_SRCS=${.CURDIR}/../../../contrib/llvm/tools/lldb

CFLAGS+=	-I${LLDB_SRCS}/include
CXXFLAGS+=	-std=c++11
.if ${MACHINE_CPUARCH} == "arm"
CFLAGS+=	-mlong-calls
.endif

SRCDIR=	tools/lldb/tools/driver
SRCS=	Driver.cpp \
	Platform.cpp

# Man page directory
.PATH: ${LLDB_SRCS}/docs

LIBADD+=	edit panel ncursesw execinfo z

LLDB_LIBS=\
	lldb \
	lldbAPI \
	lldbBreakpoint \
	lldbCommands \
	lldbDataFormatters \
	lldbHostCommon \
	lldbHostFreeBSD \
	lldbHostPOSIX \
	lldbCore \
	lldbExpression \
	lldbInitialization \
	lldbInterpreter \
	lldbSymbol \
	lldbTarget \
	lldbUtility \
	\
	lldbPluginDisassemblerLLVM \
	lldbPluginSymbolFileDWARF \
	lldbPluginSymbolFilePDB \
	lldbPluginSymbolFileSymtab \
	lldbPluginDynamicLoaderStatic \
	lldbPluginDynamicLoaderPosixDYLD \
	lldbPluginCPlusPlusLanguage \
	lldbPluginObjCLanguage \
	lldbPluginObjectFileELF \
	lldbPluginObjectFileJIT \
	lldbPluginSymbolVendorELF \
	lldbPluginProcessGDBRemote \
	lldbPluginProcessUtility \
	lldbPluginPlatformGDB \
	lldbPluginPlatformFreeBSD \
	lldbPluginObjectContainerBSDArchive \
	lldbPluginUnwindAssemblyInstEmulation \
	lldbPluginUnwindAssemblyX86 \
	lldbPluginCXXItaniumABI \
	lldbPluginABISysV_arm \
	lldbPluginABISysV_arm64 \
	lldbPluginABISysV_i386 \
	lldbPluginABISysV_x86_64 \
	lldbPluginABISysV_ppc \
	lldbPluginABISysV_ppc64 \
	lldbPluginABISysV_mips \
	lldbPluginABISysV_mips64 \
	lldbPluginInstructionARM \
	lldbPluginInstructionARM64 \
	lldbPluginInstructionMIPS \
	lldbPluginInstructionMIPS64 \
	lldbPluginMemoryHistoryASan \
	lldbPluginInstrumentationRuntimeAddressSanitizer \
	lldbPluginProcessElfCore \
	lldbPluginJITLoaderGDB \
	lldbPluginExpressionParserClang \
	lldbPluginExpressionParserGo \
	lldbPluginProcessFreeBSD \
	lldbPluginProcessPOSIX \
	lldbPluginScriptInterpreterNone

LDADD+=	-Wl,--start-group
.for lib in ${LLDB_LIBS}
DPADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
LDADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
.endfor
LDADD+=	-Wl,--end-group

# Clang and LLVM libraries
LIBDEPS=\
	clangcodegen \
	clangrewritefrontend \
	llvmaarch64codegen \
	llvmaarch64asmparser \
	llvmaarch64disassembler \
	llvmarmcodegen \
	llvmarmasmparser \
	llvmarmdisassembler \
	llvmmipscodegen \
	llvmmipsasmparser \
	llvmmipsdesc \
	llvmmipsdisassembler \
	llvmpowerpccodegen \
	llvmpowerpcasmparser \
	llvmpowerpcdesc \
	llvmpowerpcdisassembler \
	llvmsparccodegen \
	llvmsparcasmparser \
	llvmsparcdesc \
	llvmsparcdisassembler \
	llvmx86codegen \
	llvmx86asmparser \
	llvmx86desc \
	llvmx86disassembler \
	llvminterpreter \
	llvmipo \
	llvmselectiondag \
	llvmmcjit \
	llvmexecutionengine \
	llvmruntimedyld \
	llvmcoverage \
	llvmglobalisel \
	llvmaarch64desc \
	llvmaarch64asmprinter \
	llvmaarch64info \
	llvmaarch64utils \
	llvmarmdesc \
	llvmarmasmprinter \
	llvmarminfo \
	llvmmipsasmprinter \
	llvmmipsinfo \
	llvmpowerpcasmprinter \
	llvmpowerpcinfo \
	llvmsparcasmprinter \
	llvmsparcinfo \
	llvmasmprinter \
	llvmx86asmprinter \
	llvmx86utils \
	llvmx86info \
	llvmmcdisassembler \
	llvmcodegen \
	llvmdebuginfopdb \
	llvmdebuginfocodeview \
	llvmbitwriter \
	llvmvectorize \
	llvmscalaropts \
	llvmtarget \
	llvmobject \
	llvminstrumentation \
	llvmirreader \
	llvmasmparser \
	llvminstcombine \
	llvmlinker \
	llvmobjcarcopts \
	llvmtransformutils \
	llvmanalysis \
	clangfrontend \
	clangdriver \
	llvmoption \
	clangparse \
	llvmmcparser \
	clangserialization \
	llvmbitreader \
	clangsema \
	clanganalysis \
	clangedit \
	clangast \
	llvmprofiledata \
	clangrewrite \
	clanglex \
	clangbasic \
	llvmmc \
	llvmcore \
	llvmsupport

LIBADD+=	pthread

.include "../clang.prog.mk"
