# $FreeBSD$

.include <bsd.own.mk>

.if !make(install)
.if !defined(EARLY_BUILD)
.if defined(MK_CLANG_FULL) && ${MK_CLANG_FULL} != "no"
_libclangstaticanalyzer= \
	libclangstaticanalyzercheckers \
	libclangstaticanalyzercore \
	libclangstaticanalyzerfrontend
_libclangarcmigrate= \
	libclangarcmigrate
.endif # MK_CLANG_FULL
.if (defined(MK_CLANG_FULL) && ${MK_CLANG_FULL} != "no") || \
    (defined(MK_LLDB) && ${MK_LLDB} != "no")
_libclangrewriter= \
	libclangrewritecore \
	libclangrewritefrontend
.endif # (MK_CLANG_FULL || MK_LLDB)
.endif # !EARLY_BUILD

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
	${_libclangrewriter} \
	libclangsema \
	libclangserialization \
	${_libclangstaticanalyzer} \
	\
	libllvmanalysis \
	libllvmarchive \
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
	libllvmmcparser \
	libllvmobjcarcopts \
	libllvmobject \
	libllvmscalaropts \
	libllvmselectiondag \
	libllvmsupport \
	libllvmtablegen \
	libllvmtarget \
	libllvmtransformutils \
	libllvmvectorize \
	\
	libllvmarmasmparser \
	libllvmarmcodegen \
	libllvmarmdesc \
	libllvmarmdisassembler \
	libllvmarminfo \
	libllvmarminstprinter \
	libllvmmipsasmparser \
	libllvmmipscodegen \
	libllvmmipsdesc \
	libllvmmipsdisassembler \
	libllvmmipsinfo \
	libllvmmipsinstprinter \
	libllvmpowerpccodegen \
	libllvmpowerpcdesc \
	libllvmpowerpcinfo \
	libllvmpowerpcinstprinter \
	libllvmx86asmparser \
	libllvmx86codegen \
	libllvmx86desc \
	libllvmx86disassembler \
	libllvmx86info \
	libllvmx86instprinter \
	libllvmx86utils

.if ${MK_CLANG_EXTRAS} != "no"
SUBDIR+=libllvmdebuginfo
.endif # MK_CLANG_EXTRAS
.if ${MK_CLANG_EXTRAS} != "no" || ${MK_LLDB} != "no"
SUBDIR+=libllvmexecutionengine \
	libllvminterpreter \
	libllvmjit \
	libllvmmcdisassembler \
	libllvmmcjit \
	libllvmruntimedyld
.endif # MK_CLANG_EXTRAS | LLDB

.if !defined(EARLY_BUILD) && ${MK_LLDB} != "no"
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
	liblldbInterpreter \
	liblldbSymbol \
	liblldbTarget \
	liblldbUtility \
	\
	liblldbPluginABISysV_x86_64 \
	liblldbPluginCXXItaniumABI \
	liblldbPluginDisassemblerLLVM \
	liblldbPluginDynamicLoaderStatic \
	liblldbPluginDynamicLoaderPosixDYLD \
	liblldbPluginInstructionARM \
	liblldbPluginObjectContainerBSDArchive \
	liblldbPluginObjectFileELF \
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
.endif # !EARLY_BUILD && MK_LLDB

.endif # !make(install)

SUBDIR+= include

.include <bsd.subdir.mk>
