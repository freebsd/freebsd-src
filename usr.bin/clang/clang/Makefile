# $FreeBSD$

.include <src.opts.mk>

PROG_CXX=clang

SRCDIR=	tools/clang/tools/driver
SRCS=	cc1_main.cpp \
	cc1as_main.cpp \
	driver.cpp

.if ${MK_SHARED_TOOLCHAIN} == "no"
NO_SHARED?= yes

.if ${MACHINE_CPUARCH} == "arm"
CFLAGS+= -mlong-calls
.endif
.endif

LINKS=	${BINDIR}/clang ${BINDIR}/clang++ \
	${BINDIR}/clang ${BINDIR}/clang-cpp
MLINKS=	clang.1 clang++.1 \
	clang.1 clang-cpp.1
.if ${MK_CLANG_IS_CC} != "no"
SCRIPTS=CC.sh
SCRIPTSNAME=CC

LINKS+=	${BINDIR}/clang ${BINDIR}/cc \
	${BINDIR}/clang ${BINDIR}/c++ \
	${BINDIR}/clang ${BINDIR}/cpp
MLINKS+= clang.1 cc.1 \
	clang.1 c++.1 \
	clang.1 CC.1 \
	clang.1 cpp.1
.endif

TGHDRS=	Attributes \
	Intrinsics \
	DiagnosticCommonKinds \
	DiagnosticDriverKinds \
	DiagnosticFrontendKinds \
	DiagnosticLexKinds \
	DiagnosticSemaKinds \
	Options

.if ${MK_CLANG_FULL} != "no"
_clangstaticanalyzer= \
	clangstaticanalyzerfrontend \
	clangstaticanalyzercheckers \
	clangstaticanalyzercore
_clangarcmigrate= \
	clangarcmigrate
.endif # MK_CLANG_FULL

LIBDEPS=clangfrontendtool \
	clangfrontend \
	clangdriver \
	clangserialization \
	clangcodegen \
	clangparse \
	clangsema \
	clangrewritefrontend \
	clangrewrite \
	${_clangstaticanalyzer} \
	${_clangarcmigrate} \
	clanganalysis \
	clangedit \
	clangast \
	clanglex \
	clangbasic \
	llvmoption \
	llvmobjcarcopts \
	llvmipo \
	llvmvectorize \
	llvmlinker \
	llvmirreader \
	llvmasmparser \
	llvmaarch64codegen \
	llvmaarch64asmparser \
	llvmaarch64desc \
	llvmaarch64info \
	llvmaarch64asmprinter \
	llvmaarch64utils \
	llvmarmcodegen \
	llvmarmasmparser \
	llvmarmdesc \
	llvmarminfo \
	llvmarmasmprinter \
	llvmmipscodegen \
	llvmmipsasmparser \
	llvmmipsdesc \
	llvmmipsinfo \
	llvmmipsasmprinter \
	llvmpowerpccodegen \
	llvmpowerpcasmparser \
	llvmpowerpcdesc \
	llvmpowerpcinfo \
	llvmpowerpcasmprinter \
	llvmsparccodegen \
	llvmsparcasmparser \
	llvmsparcdesc \
	llvmsparcinfo \
	llvmsparcasmprinter \
	llvmx86asmparser \
	llvmx86codegen \
	llvmselectiondag \
	llvmasmprinter \
	llvmcodegen \
	llvmtarget \
	llvmscalaropts \
	llvminstcombine \
	llvminstrumentation \
	llvmprofiledata \
	llvmtransformutils \
	llvmbitwriter \
	llvmanalysis \
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

LIBADD+=	z

.include "../clang.prog.mk"
