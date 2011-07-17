# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=clang

SRCDIR=	tools/clang/tools/driver
SRCS=	cc1_main.cpp \
	cc1as_main.cpp \
	driver.cpp

LINKS=	${BINDIR}/clang ${BINDIR}/clang++

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
	clangast \
	clanglex \
	clangbasic \
	llvmipo \
	llvminstrumentation \
	llvmbitwriter \
	llvmbitreader \
	llvmasmparser \
	llvmarmdisassembler \
	llvmarmasmparser \
	llvmarmcodegen \
	llvmarmdesc \
	llvmarminstprinter \
	llvmarminfo \
	llvmmipscodegen \
	llvmmipsdesc \
	llvmmipsinstprinter \
	llvmmipsinfo \
	llvmpowerpccodegen \
	llvmpowerpcdesc \
	llvmpowerpcinstprinter \
	llvmpowerpcinfo \
	llvmx86disassembler \
	llvmx86asmparser \
	llvmx86codegen \
	llvmx86desc \
	llvmselectiondag \
	llvmasmprinter \
	llvmmcparser \
	llvmcodegen \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmx86instprinter \
	llvmx86utils \
	llvmcore \
	llvmx86info \
	llvmmc \
	llvmsupport

.include "../clang.prog.mk"
