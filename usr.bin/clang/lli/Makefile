# $FreeBSD$

.include <bsd.own.mk>

PROG_CXX=lli

SRCDIR=	tools/lli
SRCS=	lli.cpp \
	RecordingMemoryManager.cpp \
	RemoteTarget.cpp

LIBDEPS=llvmx86disassembler \
	llvmx86asmparser \
	llvmirreader \
	llvmasmparser \
	llvmbitreader \
	llvmx86codegen \
	llvmx86desc \
	llvmx86info \
	llvmx86instprinter \
	llvmx86utils \
	llvmselectiondag \
	llvmasmprinter \
	llvmmcparser \
	llvminterpreter \
	llvmmcjit \
	llvmjit \
	llvmcodegen \
	llvmobjcarcopts \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmruntimedyld \
	llvmexecutionengine \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
