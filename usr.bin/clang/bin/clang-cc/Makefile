# $FreeBSD$

PROG_CXX=clang-cc
BINDIR=	/usr/libexec

SRCDIR=	tools/clang/tools/clang-cc
SRCS=	clang-cc.cpp
MAN=

TGHDRS=	DiagnosticCommonKinds DiagnosticFrontendKinds \
	DiagnosticLexKinds DiagnosticSemaKinds
LIBDEPS=clangfrontend clangrewrite clanglex clangparse clanganalysis \
	clangsema clangcodegen clangast clangbasic \
	llvmselectiondag llvmasmprinter llvmcodegen llvmscalaropts \
	llvmbitreader llvmbitwriter llvmtarget llvmipo llvmipa \
	llvmtransformutils llvmanalysis llvmcore llvmsupport \
	llvmsystem
OBJDEPS=llvmarmasmprinter llvmarmcodegen \
	llvmia64asmprinter llvmia64codegen \
	llvmmipsasmprinter llvmmipscodegen \
	llvmpowerpcasmprinter llvmpowerpccodegen \
	llvmx86asmprinter llvmx86codegen

.include "../../clang.prog.mk"
