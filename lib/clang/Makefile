# $FreeBSD$

.include <bsd.own.mk>

.if !make(install)
.if !defined(EARLY_BUILD) && defined(MK_CLANG_FULL) && ${MK_CLANG_FULL} != "no"
_libclangstaticanalyzer= \
	libclangstaticanalyzercheckers \
	libclangstaticanalyzercore \
	libclangstaticanalyzerfrontend
_libclangarcmigrate= \
	libclangarcmigrate
_libclangrewriter= \
	libclangrewritecore \
	libclangrewritefrontend
.endif # !EARLY_BUILD && MK_CLANG_FULL

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
	libllvmoption \
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
	libllvmpowerpcasmparser \
	libllvmpowerpccodegen \
	libllvmpowerpcdesc \
	libllvmpowerpcinfo \
	libllvmpowerpcinstprinter \
	libllvmsparcasmparser \
	libllvmsparccodegen \
	libllvmsparcdesc \
	libllvmsparcdisassembler \
	libllvmsparcinfo \
	libllvmsparcinstprinter \
	libllvmx86asmparser \
	libllvmx86codegen \
	libllvmx86desc \
	libllvmx86disassembler \
	libllvmx86info \
	libllvmx86instprinter \
	libllvmx86utils

.if ${MK_CLANG_EXTRAS} != "no"
SUBDIR+=libllvmdebuginfo \
	libllvmexecutionengine \
	libllvminterpreter \
	libllvmjit \
	libllvmmcdisassembler \
	libllvmmcjit \
	libllvmruntimedyld
.endif # MK_CLANG_EXTRAS
.endif # !make(install)

SUBDIR+= include

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
