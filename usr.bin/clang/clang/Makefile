# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=clang

SRCDIR=	tools/clang/tools/driver
SRCS=	cc1_main.cpp cc1as_main.cpp driver.cpp
MAN=

LINKS=	${BINDIR}/clang ${BINDIR}/clang++

TGHDRS=	CC1AsOptions CC1Options DiagnosticCommonKinds \
	DiagnosticDriverKinds DiagnosticFrontendKinds \
	DiagnosticLexKinds DiagnosticSemaKinds Options
LIBDEPS=clangfrontend clangdriver clangcodegen clangsema clangchecker \
	clanganalysis clangrewrite clangast clangparse clanglex clangbasic \
	\
	llvminstcombine llvmipo llvmbitwriter llvmbitreader \
	llvmpowerpccodegen llvmpowerpcasmprinter llvmpowerpcinfo \
	llvmx86asmparser llvmx86asmprinter llvmx86codegen llvmx86info \
	llvmmipsasmprinter llvmmipscodegen llvmmipsinfo \
	llvmarmasmparser llvmarmasmprinter llvmarmcodegen \
	llvmselectiondag llvmasmprinter llvmcodegen llvmscalaropts \
	llvmtransformutils llvmmc llvmmcparser llvmipa llvmanalysis \
	llvmtarget llvmmc llvmcore llvmarminfo llvmsupport llvmsystem

.include "../clang.prog.mk"
