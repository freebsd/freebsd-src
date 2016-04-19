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
	\
	lldbAPI \
	lldbBreakpoint \
	lldbCommands \
	lldbCore \
	lldbDataFormatters \
	lldbExpression \
	lldbHostFreeBSD \
	lldbHostCommon \
	lldbHostPOSIX \
	lldbInitialization \
	lldbInterpreter \
	lldbSymbol \
	lldbTarget \
	lldbUtility \
	\
	lldbPluginABISysV_arm \
	lldbPluginABISysV_arm64 \
	lldbPluginABISysV_i386 \
	lldbPluginABISysV_mips \
	lldbPluginABISysV_mips64 \
	lldbPluginABISysV_ppc \
	lldbPluginABISysV_ppc64 \
	lldbPluginABISysV_x86_64 \
	lldbPluginCXXItaniumABI \
	lldbPluginDisassemblerLLVM \
	lldbPluginExpressionParserClang \
	lldbPluginExpressionParserGo \
	lldbPluginInstructionARM \
	lldbPluginInstructionARM64 \
	lldbPluginInstructionMIPS \
	lldbPluginInstructionMIPS64 \
	lldbPluginInstrumentationRuntimeAddressSanitizer \
	lldbPluginJITLoaderGDB \
	lldbPluginLanguageCPlusPlus \
	lldbPluginLanguageObjC \
	lldbPluginSymbolFileDWARF \
	lldbPluginSymbolFileSymtab \
	lldbPluginDynamicLoaderStatic \
	lldbPluginDynamicLoaderPosixDYLD \
	lldbPluginMemoryHistoryASan \
	lldbPluginObjectContainerBSDArchive \
	lldbPluginObjectFileELF \
	lldbPluginObjectFileJIT \
	lldbPluginScriptInterpreterNone \
	lldbPluginSymbolVendorELF \
	lldbPluginPlatformFreeBSD \
	lldbPluginPlatformGDB \
	lldbPluginProcessElfCore \
	lldbPluginProcessFreeBSD \
	lldbPluginProcessGDBRemote \
	lldbPluginProcessUtility \
	lldbPluginProcessPOSIX \
	lldbPluginProcessFreeBSD \
	lldbPluginUnwindAssemblyInstEmulation \
	lldbPluginUnwindAssemblyX86

LDADD+=	-Wl,--start-group
.for lib in ${LLDB_LIBS}
DPADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
LDADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
.endfor
LDADD+=	-Wl,--end-group

# Clang and LLVM libraries
LIBDEPS=\
	clangfrontend \
	clangdriver \
	clangserialization \
	clangcodegen \
	clangparse \
	clangsema \
	clanganalysis \
	clangedit \
	clangast \
	clanglex \
	clangbasic \
	\
	llvmoption \
	llvmobjcarcopts \
	llvmlinker \
	llvmmcjit \
	llvmruntimedyld \
	llvmexecutionengine \
	llvmirreader \
	llvmipo \
	llvmvectorize \
	llvmbitwriter \
	llvmasmparser \
	llvmaarch64disassembler \
	llvmaarch64codegen \
	llvmaarch64asmparser \
	llvmaarch64desc \
	llvmaarch64info \
	llvmaarch64asmprinter \
	llvmaarch64utils \
	llvmarmdisassembler \
	llvmarmcodegen \
	llvmarmasmparser \
	llvmarmdesc \
	llvmarminfo \
	llvmarmasmprinter \
	llvmmipsdisassembler \
	llvmmipscodegen \
	llvmmipsasmparser \
	llvmmipsdesc \
	llvmmipsinfo \
	llvmmipsasmprinter \
	llvmpowerpcdisassembler \
	llvmpowerpccodegen \
	llvmpowerpcasmparser \
	llvmpowerpcdesc \
	llvmpowerpcinfo \
	llvmpowerpcasmprinter \
	llvmsparcdisassembler \
	llvmsparccodegen \
	llvmsparcasmparser \
	llvmsparcdesc \
	llvmsparcinfo \
	llvmsparcasmprinter \
	llvmx86disassembler \
	llvmx86asmparser \
	llvmx86codegen \
	llvmselectiondag \
	llvmasmprinter \
	llvmcodegen \
	llvmtarget \
	llvmscalaropts \
	llvmprofiledata \
	llvminstcombine \
	llvmtransformutils \
	llvmanalysis \
	llvminstrumentation \
	llvmx86desc \
	llvmobject \
	llvmmcparser \
	llvmbitreader \
	llvmmcdisassembler \
	llvmx86info \
	llvmx86asmprinter \
	llvmmc \
	llvmx86utils \
	llvmcore \
	llvmsupport

LIBADD+=	pthread

.include "../clang.prog.mk"
