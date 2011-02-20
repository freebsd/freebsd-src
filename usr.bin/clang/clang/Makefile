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
	clangchecker \
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
	llvmpowerpcasmprinter \
	llvmpowerpcinfo \
	llvmx86asmparser \
	llvmx86disassembler \
	llvmx86codegen \
	llvmx86asmprinter \
	llvmx86info \
	llvmmipsasmprinter \
	llvmmipscodegen \
	llvmmipsinfo \
	llvmarmasmparser \
	llvmarmcodegen \
	llvmarmasmprinter \
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
	llvmsupport \
	llvmsystem

.include "../clang.prog.mk"
