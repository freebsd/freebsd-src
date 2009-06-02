# $FreeBSD$

PROG_CXX=bugpoint

SRCDIR=	tools/bugpoint
SRCS=	BugDriver.cpp CrashDebugger.cpp ExecutionDriver.cpp \
	ExtractFunction.cpp FindBugs.cpp Miscompilation.cpp  \
	OptimizerDriver.cpp TestPasses.cpp ToolRunner.cpp bugpoint.cpp
MAN=

LIBDEPS=llvmasmparser llvmscalaropts llvmlinker llvmipo llvmipa \
	llvmtransformutils llvminstrumentation llvmanalysis \
	llvmbitreader llvmbitwriter llvmtarget llvmcore llvmsupport \
	llvmsystem
TGHDRS=	Intrinsics

.include "../../clang.prog.mk"
