# $FreeBSD$

.include <src.opts.mk>

.if !make(install)
.if ${MK_CLANG_FULL} != "no"
_libclangstaticanalyzer= \
	libclangstaticanalyzercheckers \
	libclangstaticanalyzercore \
	libclangstaticanalyzerfrontend
_libclangarcmigrate= \
	libclangarcmigrate
.endif # MK_CLANG_FULL

SUBDIR=	libclanganalysis \
	${_libclangarcmigrate} \
	libclangast \
	libclangbasic \
	libclangcodegen \
	libclangdriver \
	libclangedit \
	libclangfrontend \
	libclangfrontendtool \
	libclanglex \
	libclangparse \
	libclangrewritefrontend \
	libclangrewrite \
	libclangsema \
	libclangserialization \
	${_libclangstaticanalyzer} \
	\
	libllvmanalysis \
	libllvmasmparser \
	libllvmasmprinter \
	libllvmbitreader \
	libllvmbitwriter \
	libllvmcodegen \
	libllvmcore \
	libllvminstcombine \
	libllvminstrumentation \
	libllvmipa \
	libllvmipo \
	libllvmirreader \
	libllvmlinker \
	libllvmmc \
	libllvmmcdisassembler \
	libllvmmcparser \
	libllvmobjcarcopts \
	libllvmobject \
	libllvmoption \
	libllvmprofiledata \
	libllvmscalaropts \
	libllvmselectiondag \
	libllvmsupport \
	libllvmtablegen \
	libllvmtarget \
	libllvmtransformutils \
	libllvmvectorize \
	\
	libllvmaarch64asmparser \
	libllvmaarch64codegen \
	libllvmaarch64desc \
	libllvmaarch64info \
	libllvmaarch64instprinter \
	libllvmaarch64utils \
	libllvmarmasmparser \
	libllvmarmcodegen \
	libllvmarmdesc \
	libllvmarmdisassembler \
	libllvmarminfo \
	libllvmarminstprinter \
	libllvmmipsasmparser \
	libllvmmipscodegen \
	libllvmmipsdesc \
	libllvmmipsinfo \
	libllvmmipsinstprinter \
	libllvmpowerpcasmparser \
	libllvmpowerpccodegen \
	libllvmpowerpcdesc \
	libllvmpowerpcdisassembler \
	libllvmpowerpcinfo \
	libllvmpowerpcinstprinter \
	libllvmsparcasmparser \
	libllvmsparccodegen \
	libllvmsparcdesc \
	libllvmsparcinfo \
	libllvmsparcinstprinter \
	libllvmx86asmparser \
	libllvmx86codegen \
	libllvmx86desc \
	libllvmx86info \
	libllvmx86instprinter \
	libllvmx86utils

.if ${MK_CLANG_EXTRAS} != "no"
SUBDIR+=libllvmdebuginfodwarf \
	libllvmdebuginfopdb \
	libllvmlibdriver \
	libllvmlto \
	libllvmmirparser \
	libllvmorcjit \
	libllvmpasses
.endif # MK_CLANG_EXTRAS
.if ${MK_CLANG_EXTRAS} != "no" || ${MK_LLDB} != "no"
SUBDIR+=libllvmaarch64disassembler \
	libllvmexecutionengine \
	libllvminterpreter \
	libllvmmcjit \
	libllvmmipsdisassembler \
	libllvmruntimedyld \
	libllvmsparcdisassembler \
	libllvmx86disassembler
.endif # MK_CLANG_EXTRAS | LLDB

.if ${MK_LLDB} != "no"
SUBDIR+=liblldb \
	\
	liblldbAPI \
	liblldbBreakpoint \
	liblldbCommands \
	liblldbCore \
	liblldbDataFormatters \
	liblldbExpression \
	liblldbHostCommon \
	liblldbHostFreeBSD \
	liblldbHostPOSIX \
	liblldbInitialization \
	liblldbInterpreter \
	liblldbSymbol \
	liblldbTarget \
	liblldbUtility \
	\
	liblldbPluginABISysV_arm \
	liblldbPluginABISysV_arm64 \
	liblldbPluginABISysV_i386 \
	liblldbPluginABISysV_mips \
	liblldbPluginABISysV_mips64 \
	liblldbPluginABISysV_ppc \
	liblldbPluginABISysV_ppc64 \
	liblldbPluginABISysV_x86_64 \
	liblldbPluginCXXItaniumABI \
	liblldbPluginDisassemblerLLVM \
	liblldbPluginDynamicLoaderStatic \
	liblldbPluginDynamicLoaderPosixDYLD \
	liblldbPluginInstructionARM \
	liblldbPluginInstructionARM64 \
	liblldbPluginInstructionMIPS \
	liblldbPluginInstructionMIPS64 \
	liblldbPluginInstrumentationRuntimeAddressSanitizer \
	liblldbPluginJITLoaderGDB \
	liblldbPluginMemoryHistoryASan \
	liblldbPluginObjectContainerBSDArchive \
	liblldbPluginObjectFileELF \
	liblldbPluginObjectFileJIT \
	liblldbPluginPlatformFreeBSD \
	liblldbPluginPlatformGDB \
	liblldbPluginProcessElfCore \
	liblldbPluginProcessFreeBSD \
	liblldbPluginProcessGDBRemote \
	liblldbPluginProcessPOSIX \
	liblldbPluginProcessUtility \
	liblldbPluginSymbolFileDWARF \
	liblldbPluginSymbolFileSymtab \
	liblldbPluginSymbolVendorELF \
	liblldbPluginUnwindAssemblyInstEmulation \
	liblldbPluginUnwindAssemblyX86
.endif # MK_LLDB

.endif # !make(install)

SUBDIR+= include

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
