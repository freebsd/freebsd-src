# $FreeBSD$

.include <bsd.own.mk>

.if !make(install)
SUBDIR=	libclanganalysis \
	libclangarcmigrate \
	libclangast \
	libclangbasic \
	libclangcodegen \
	libclangdriver \
	libclangedit \
	libclangfrontend \
	libclangfrontendtool \
	libclanglex \
	libclangparse \
	libclangrewritecore \
	libclangrewritefrontend \
	libclangsema \
	libclangserialization \
	libclangstaticanalyzercheckers \
	libclangstaticanalyzercore \
	libclangstaticanalyzerfrontend \
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
	libllvmlinker \
	libllvmmc \
	libllvmmcparser \
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
SUBDIR+=libllvmdebuginfo \
	libllvmexecutionengine \
	libllvminterpreter \
	libllvmjit \
	libllvmmcdisassembler \
	libllvmmcjit \
	libllvmruntimedyld
.endif
.endif

SUBDIR+= include

.include <bsd.subdir.mk>
