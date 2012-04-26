# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=clang

SRCDIR=	tools/clang/tools/driver
SRCS=	cc1_main.cpp \
	cc1as_main.cpp \
	driver.cpp

LINKS=	${BINDIR}/clang ${BINDIR}/clang++ \
	${BINDIR}/clang ${BINDIR}/clang-cpp
MLINKS=	clang.1 clang++.1 \
	clang.1 clang-cpp.1
.if ${MK_CLANG_IS_CC} != "no"
LINKS+=	${BINDIR}/clang ${BINDIR}/cc \
	${BINDIR}/clang ${BINDIR}/c++ \
	${BINDIR}/clang ${BINDIR}/cpp
MLINKS+= clang.1 cc.1 \
	clang.1 c++.1 \
	clang.1 cpp.1
.endif

TGHDRS=	CC1AsOptions \
	CC1Options \
	DiagnosticCommonKinds \
	DiagnosticDriverKinds \
	DiagnosticFrontendKinds \
	DiagnosticLexKinds \
	DiagnosticSemaKinds \
	Options
LIBDEPS=clangfrontendtool \
	clangfrontend \
	clangdriver \
	clangserialization \
	clangcodegen \
	clangparse \
	clangsema \
	clangstaticanalyzerfrontend \
	clangstaticanalyzercheckers \
	clangstaticanalyzercore \
	clanganalysis \
	clangindex \
	clangarcmigrate \
	clangrewrite \
	clangedit \
	clangast \
	clanglex \
	clangbasic \
	llvmlinker \
	llvmarchive \
	llvmipo \
	llvmvectorize \
	llvminstrumentation \
	llvmbitwriter \
	llvmbitreader \
	llvmasmparser \
	llvmmipsasmparser \
	llvmmipscodegen \
	llvmmipsdesc \
	llvmmipsinfo \
	llvmmipsinstprinter \
	llvmarmdisassembler \
	llvmarmasmparser \
	llvmarmcodegen \
	llvmarmdesc \
	llvmarminfo \
	llvmarminstprinter \
	llvmpowerpccodegen \
	llvmpowerpcdesc \
	llvmpowerpcinfo \
	llvmpowerpcinstprinter \
	llvmx86asmparser \
	llvmx86disassembler \
	llvmx86codegen \
	llvmselectiondag \
	llvmasmprinter \
	llvmmcparser \
	llvmcodegen \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmx86desc \
	llvmx86info \
	llvmtarget \
	llvmx86instprinter \
	llvmmc \
	llvmobject \
	llvmx86utils \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
