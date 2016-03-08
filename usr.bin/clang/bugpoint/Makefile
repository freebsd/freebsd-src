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

TGHDRS=	Attributes \
	Intrinsics

LIBDEPS=llvmcodegen \
	llvmtarget \
	llvmobjcarcopts \
	llvmbitwriter \
	llvmipo \
	llvmvectorize \
	llvmlinker \
	llvmirreader \
	llvmscalaropts \
	llvminstcombine \
	llvminstrumentation \
	llvmprofiledata \
	llvmobject \
	llvmmcparser \
	llvmbitreader \
	llvmtransformutils \
	llvmmc \
	llvmanalysis \
	llvmasmparser \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
