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
	clangrewrite \
	clangast \
	clanglex \
	clangbasic \
	llvminstcombine \
	llvmipo \
	llvmbitwriter \
	llvmbitreader \
	llvmpowerpccodegen \
	llvmpowerpcinstprinter \
	llvmpowerpcinfo \
	llvmx86asmparser \
	llvmx86disassembler \
	llvmx86codegen \
	llvmx86instprinter \
	llvmx86info \
	llvmx86utils \
	llvmmipscodegen \
	llvmmipsinfo \
	llvmarmasmparser \
	llvmarmcodegen \
	llvmarminstprinter \
	llvmasmparser \
	llvmselectiondag \
	llvmasmprinter \
	llvmcodegen \
	llvmscalaropts \
	llvmtransformutils \
	llvmmc \
	llvmmcparser \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmcore \
	llvmarminfo \
	llvmsupport

.include "../clang.prog.mk"
