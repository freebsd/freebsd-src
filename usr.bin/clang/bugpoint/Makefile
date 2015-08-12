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
	llvmtarget \
	llvmobjcarcopts \
	llvmirreader \
	llvmbitwriter \
	llvmlinker \
	llvmipo \
	llvmvectorize \
	llvmscalaropts \
	llvmprofiledata \
	llvmobject \
	llvmmcparser \
	llvmbitreader \
	llvminstcombine \
	llvminstrumentation \
	llvmtransformutils \
	llvmipa \
	llvmmc \
	llvmanalysis \
	llvmasmparser \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
