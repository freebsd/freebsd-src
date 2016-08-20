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
_clangarcmigrate=		clangarcmigrate
_clangstaticanalyzerfrontend=	clangstaticanalyzerfrontend
_clangstaticanalyzercheckers=	clangstaticanalyzercheckers
_clangstaticanalyzercore=	clangstaticanalyzercore
.endif # MK_CLANG_FULL

LIBDEPS=llvmaarch64codegen \
	llvmaarch64asmparser \
	llvmaarch64disassembler \
	llvmarmcodegen \
	llvmarmasmparser \
	llvmarmdisassembler \
	llvmmipscodegen \
	llvmmipsasmparser \
	llvmmipsdesc \
	llvmpowerpccodegen \
	llvmpowerpcasmparser \
	llvmpowerpcdesc \
	llvmpowerpcdisassembler \
	llvmsparccodegen \
	llvmsparcasmparser \
	llvmsparcdesc \
	llvmx86codegen \
	llvmx86asmparser \
	llvmx86desc \
	clangfrontendtool \
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
	llvmdebuginfocodeview \
	llvmselectiondag \
	llvmcodegen \
	llvmx86asmprinter \
	llvmx86utils \
	llvmx86info \
	llvmmcdisassembler \
	clangcodegen \
	llvmipo \
	llvmvectorize \
	llvminstrumentation \
	llvmobjcarcopts \
	llvmscalaropts \
	llvminstcombine \
	llvmtarget \
	llvmbitwriter \
	llvmirreader \
	llvmasmparser \
	llvmlinker \
	llvmtransformutils \
	llvmanalysis \
	llvmcoverage \
	llvmobject \
	clangrewritefrontend \
	${_clangarcmigrate} \
	${_clangstaticanalyzerfrontend} \
	clangfrontend \
	clangdriver \
	llvmoption \
	llvmprofiledata \
	clangparse \
	llvmmcparser \
	clangserialization \
	llvmbitreader \
	clangsema \
	clangedit \
	${_clangstaticanalyzercheckers} \
	${_clangstaticanalyzercore} \
	clanganalysis \
	clangast \
	clangrewrite \
	clanglex \
	clangbasic \
	llvmcore \
	llvmmc \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
