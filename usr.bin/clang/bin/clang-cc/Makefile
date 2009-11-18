# $FreeBSD$

PROG_CXX=clang-cc
BINDIR=	/usr/libexec

SRCDIR=	tools/clang/tools/clang-cc
SRCS=	clang-cc.cpp Options.cpp
LDFLAGS+=	-pthread
MAN=

TGHDRS=	DiagnosticCommonKinds DiagnosticFrontendKinds \
	DiagnosticLexKinds DiagnosticSemaKinds
LIBDEPS=clangfrontend clangrewrite clanglex clangparse clanganalysis \
	clangsema clangcodegen clangast clangbasic \
	\
	llvmipo llvmbitwriter llvmbitreader llvmpowerpccodegen \
	llvmpowerpcasmprinter llvmpowerpcinfo llvmx86asmparser \
	llvmx86asmprinter llvmx86codegen llvmx86info llvmmipsasmprinter \
	llvmmipscodegen llvmmipsinfo llvmarmasmparser llvmarmasmprinter \
	llvmarmcodegen llvmselectiondag llvmasmprinter llvmcodegen \
	llvmscalaropts llvmtransformutils llvmipa llvmanalysis llvmtarget \
	llvmmc llvmcore llvmarminfo llvmsupport llvmsystem

.include "../../clang.prog.mk"
