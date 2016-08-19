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
	llvmipo \
	llvmirreader \
	llvminstrumentation \
	llvmlinker \
	llvmobjcarcopts \
	llvmscalaropts \
	llvmtarget \
	llvmvectorize \
	llvmbitwriter \
	llvmasmparser \
	llvminstcombine \
	llvmobject \
	llvmbitreader \
	llvmmcparser \
	llvmmc \
	llvmtransformutils \
	llvmanalysis \
	llvmprofiledata \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
