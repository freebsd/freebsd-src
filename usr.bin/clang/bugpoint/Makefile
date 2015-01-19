# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=bugpoint

SRCDIR=	tools/bugpoint
SRCS=	BugDriver.cpp \
	CrashDebugger.cpp \
	ExecutionDriver.cpp \
	ExtractFunction.cpp \
	FindBugs.cpp \
	Miscompilation.cpp \
	OptimizerDriver.cpp \
	ToolRunner.cpp \
	bugpoint.cpp

TGHDRS=	Intrinsics
LIBDEPS=llvmcodegen \
	llvmobjcarcopts \
	llvmirreader \
	llvmbitwriter \
	llvmbitreader \
	llvmlinker \
	llvmipo \
	llvmvectorize \
	llvmscalaropts \
	llvminstcombine \
	llvminstrumentation \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmasmparser \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
