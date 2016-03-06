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
	libllvmaarch64asmprinter \
	libllvmaarch64codegen \
	libllvmaarch64desc \
	libllvmaarch64info \
	libllvmaarch64utils \
	libllvmarmasmparser \
	libllvmarmasmprinter \
	libllvmarmcodegen \
	libllvmarmdesc \
	libllvmarminfo \
	libllvmmipsasmparser \
	libllvmmipsasmprinter \
	libllvmmipscodegen \
	libllvmmipsdesc \
	libllvmmipsinfo \
	libllvmpowerpcasmparser \
	libllvmpowerpcasmprinter \
	libllvmpowerpccodegen \
	libllvmpowerpcdesc \
	libllvmpowerpcinfo \
	libllvmsparcasmparser \
	libllvmsparcasmprinter \
	libllvmsparccodegen \
	libllvmsparcdesc \
	libllvmsparcinfo \
	libllvmx86asmparser \
	libllvmx86asmprinter \
	libllvmx86codegen \
	libllvmx86desc \
	libllvmx86info \
	libllvmx86utils

.if ${MK_CLANG_EXTRAS} != "no"
SUBDIR+=libllvmdebuginfodwarf \
	libllvmdebuginfopdb \
	libllvmlibdriver \
	libllvmlto \
	libllvmmirparser \
	libllvmorcjit \
	libllvmpasses \
	libllvmsymbolize
.endif # MK_CLANG_EXTRAS
	
.if ${MK_CLANG_EXTRAS} != "no" || ${MK_LLDB} != "no"
SUBDIR+=libllvmexecutionengine \
	libllvmaarch64disassembler \
	libllvmarmdisassembler \
	libllvminterpreter \
	libllvmmcjit \
	libllvmmipsdisassembler \
	libllvmpowerpcdisassembler \
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
	liblldbPluginExpressionParserClang \
	liblldbPluginExpressionParserGo \
	liblldbPluginInstructionARM \
	liblldbPluginInstructionARM64 \
	liblldbPluginInstructionMIPS \
	liblldbPluginInstructionMIPS64 \
	liblldbPluginInstrumentationRuntimeAddressSanitizer \
	liblldbPluginJITLoaderGDB \
	liblldbPluginLanguageCPlusPlus \
	liblldbPluginLanguageObjC \
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
	liblldbPluginScriptInterpreterNone \
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
